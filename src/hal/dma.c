/* dma.c - DMA Engine HAL Implementation for RP2040 */
#include "hal/dma.h"
#include "dmesg.h"
#include <string.h>

#ifdef PICO_BUILD
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/time.h"
#endif

/* Per-channel tracking state */
typedef struct {
    bool     claimed;
    bool     busy;
    uint32_t total_transfers;
    uint32_t total_bytes;
    dma_transfer_size_t last_size;
    uint32_t last_count;
    dma_callback_t callback;
    void    *callback_data;
} dma_channel_state_t;

static dma_channel_state_t dma_channels[DMA_NUM_CHANNELS];
static bool dma_initialized = false;

/* Sniffer tracking */
static int  sniff_channel = -1;
static bool sniff_active  = false;

#ifdef PICO_BUILD
/* DMA IRQ0 handler - dispatches to per-channel callbacks */
static void dma_irq0_handler(void) {
    for (int ch = 0; ch < DMA_NUM_CHANNELS; ch++) {
        if (dma_channel_get_irq0_status(ch)) {
            dma_channel_acknowledge_irq0(ch);
            dma_channels[ch].busy = false;
            dma_channels[ch].total_transfers++;
            dma_channels[ch].total_bytes +=
                dma_channels[ch].last_count << dma_channels[ch].last_size;
            if (dma_channels[ch].callback) {
                dma_channels[ch].callback(ch, dma_channels[ch].callback_data);
            }
        }
    }
}
#endif

int dma_hal_init(void) {
    if (dma_initialized) {
        dmesg_warn("dma: already initialized");
        return 0;
    }

    memset(dma_channels, 0, sizeof(dma_channels));
    sniff_channel = -1;
    sniff_active = false;

#ifdef PICO_BUILD
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq0_handler);
    irq_set_enabled(DMA_IRQ_0, true);
#endif

    dma_initialized = true;
    dmesg_info("dma: initialized %d channels", DMA_NUM_CHANNELS);
    return 0;
}

int dma_hal_claim(int channel) {
    if (!dma_initialized) {
        dmesg_err("dma: not initialized");
        return -1;
    }

    if (channel == -1) {
        /* Auto-assign: find first unclaimed channel */
#ifdef PICO_BUILD
        int ch = dma_claim_unused_channel(false);
        if (ch < 0) {
            dmesg_err("dma: no free channels");
            return -1;
        }
        dma_channels[ch].claimed = true;
        dmesg_info("dma: auto-claimed channel %d", ch);
        return ch;
#else
        for (int i = 0; i < DMA_NUM_CHANNELS; i++) {
            if (!dma_channels[i].claimed) {
                dma_channels[i].claimed = true;
                dmesg_info("dma: auto-claimed channel %d", i);
                return i;
            }
        }
        dmesg_err("dma: no free channels");
        return -1;
#endif
    }

    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: invalid channel %d", channel);
        return -1;
    }

    if (dma_channels[channel].claimed) {
        dmesg_err("dma: channel %d already claimed", channel);
        return -1;
    }

#ifdef PICO_BUILD
    dma_channel_claim(channel);
#endif

    dma_channels[channel].claimed = true;
    dmesg_info("dma: claimed channel %d", channel);
    return channel;
}

int dma_hal_release(int channel) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: invalid channel %d", channel);
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        dmesg_warn("dma: channel %d not claimed", channel);
        return -1;
    }

    if (dma_channels[channel].busy) {
        dmesg_warn("dma: aborting busy channel %d before release", channel);
        dma_hal_abort(channel);
    }

#ifdef PICO_BUILD
    dma_channel_unclaim(channel);
#endif

    dma_channels[channel].claimed = false;
    dma_channels[channel].callback = NULL;
    dma_channels[channel].callback_data = NULL;
    dmesg_info("dma: released channel %d", channel);
    return 0;
}

int dma_hal_start(int channel, const dma_transfer_t *transfer) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: invalid channel %d", channel);
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        dmesg_err("dma: channel %d not claimed", channel);
        return -1;
    }

    if (!transfer) {
        dmesg_err("dma: null transfer descriptor");
        return -1;
    }

    if (dma_channels[channel].busy) {
        dmesg_err("dma: channel %d busy", channel);
        return -1;
    }

    dma_channels[channel].last_size = transfer->size;
    dma_channels[channel].last_count = transfer->transfer_count;

#ifdef PICO_BUILD
    dma_channel_config cfg = dma_channel_get_default_config(channel);

    channel_config_set_transfer_data_size(&cfg,
        (transfer->size == DMA_XFER_SIZE_8)  ? DMA_XFER_SIZE_8  :
        (transfer->size == DMA_XFER_SIZE_16) ? DMA_XFER_SIZE_16 :
                                          DMA_XFER_SIZE_32);

    channel_config_set_read_increment(&cfg, transfer->read_increment);
    channel_config_set_write_increment(&cfg, transfer->write_increment);
    channel_config_set_dreq(&cfg, (uint)transfer->dreq);

    if (transfer->ring_read && transfer->ring_size_bits > 0) {
        channel_config_set_ring(&cfg, false, transfer->ring_size_bits);
    } else if (transfer->ring_write && transfer->ring_size_bits > 0) {
        channel_config_set_ring(&cfg, true, transfer->ring_size_bits);
    }

    if (transfer->chain_to >= 0 && transfer->chain_to < DMA_NUM_CHANNELS) {
        channel_config_set_chain_to(&cfg, (uint)transfer->chain_to);
    } else {
        channel_config_set_chain_to(&cfg, (uint)channel);
    }

    channel_config_set_irq_quiet(&cfg, transfer->irq_quiet);

    if (!transfer->irq_quiet) {
        dma_channel_set_irq0_enabled(channel, true);
    }

    dma_channel_configure(channel, &cfg,
                          (void *)transfer->write_addr,
                          (const void *)transfer->read_addr,
                          transfer->transfer_count,
                          true);
#endif

    dma_channels[channel].busy = true;
    dmesg_debug("dma: ch%d started %lu transfers (size=%d dreq=%d)",
                channel, (unsigned long)transfer->transfer_count,
                transfer->size, transfer->dreq);

#ifndef PICO_BUILD
    /* In stub mode, mark transfer as instantly complete */
    dma_channels[channel].busy = false;
    dma_channels[channel].total_transfers++;
    dma_channels[channel].total_bytes +=
        transfer->transfer_count << transfer->size;
    if (dma_channels[channel].callback) {
        dma_channels[channel].callback(channel, dma_channels[channel].callback_data);
    }
#endif

    return 0;
}

int dma_hal_memcpy(int channel, void *dst, const void *src, size_t len) {
    if (!dst || !src || len == 0) {
        dmesg_err("dma: memcpy invalid args");
        return -1;
    }

    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: memcpy invalid channel %d", channel);
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        dmesg_err("dma: memcpy channel %d not claimed", channel);
        return -1;
    }

    dma_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));

    /* Pick the widest aligned transfer size */
    if ((len % 4 == 0) &&
        ((uintptr_t)src % 4 == 0) &&
        ((uintptr_t)dst % 4 == 0)) {
        xfer.size = DMA_XFER_SIZE_32;
        xfer.transfer_count = (uint32_t)(len / 4);
    } else if ((len % 2 == 0) &&
               ((uintptr_t)src % 2 == 0) &&
               ((uintptr_t)dst % 2 == 0)) {
        xfer.size = DMA_XFER_SIZE_16;
        xfer.transfer_count = (uint32_t)(len / 2);
    } else {
        xfer.size = DMA_XFER_SIZE_8;
        xfer.transfer_count = (uint32_t)len;
    }

    xfer.read_addr = (volatile void *)src;
    xfer.write_addr = (volatile void *)dst;
    xfer.dreq = DMA_DREQ_FORCE;
    xfer.read_increment = true;
    xfer.write_increment = true;
    xfer.chain_to = -1;
    xfer.irq_quiet = false;

#ifndef PICO_BUILD
    /* Stub mode: just do a regular memcpy */
    memcpy(dst, src, len);
#endif

    return dma_hal_start(channel, &xfer);
}

int dma_hal_wait(int channel, uint32_t timeout_ms) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        return -1;
    }

#ifdef PICO_BUILD
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (dma_channel_is_busy(channel)) {
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
            dmesg_warn("dma: ch%d wait timed out after %lu ms",
                       channel, (unsigned long)timeout_ms);
            return -1;
        }
        tight_loop_contents();
    }
    /* Update tracking when poll-wait completes */
    if (dma_channels[channel].busy) {
        dma_channels[channel].busy = false;
        dma_channels[channel].total_transfers++;
        dma_channels[channel].total_bytes +=
            dma_channels[channel].last_count << dma_channels[channel].last_size;
    }
#else
    /* Stub mode: transfers complete instantly */
    (void)timeout_ms;
#endif

    return 0;
}

bool dma_hal_busy(int channel) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        return false;
    }

#ifdef PICO_BUILD
    return dma_channel_is_busy(channel);
#else
    return dma_channels[channel].busy;
#endif
}

int dma_hal_abort(int channel) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: abort invalid channel %d", channel);
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        dmesg_err("dma: abort channel %d not claimed", channel);
        return -1;
    }

#ifdef PICO_BUILD
    dma_channel_abort(channel);
#endif

    dma_channels[channel].busy = false;
    dmesg_info("dma: ch%d aborted", channel);
    return 0;
}

int dma_hal_status(int channel, dma_channel_status_t *status) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        return -1;
    }

    if (!status) {
        return -1;
    }

    status->claimed = dma_channels[channel].claimed;
    status->total_transfers = dma_channels[channel].total_transfers;
    status->total_bytes = dma_channels[channel].total_bytes;

#ifdef PICO_BUILD
    status->busy = dma_channel_is_busy(channel);
    status->transfer_count = dma_hw->ch[channel].transfer_count;
#else
    status->busy = dma_channels[channel].busy;
    status->transfer_count = 0;
#endif

    return 0;
}

int dma_hal_set_callback(int channel, dma_callback_t cb, void *user_data) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: set_callback invalid channel %d", channel);
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        dmesg_err("dma: set_callback channel %d not claimed", channel);
        return -1;
    }

    dma_channels[channel].callback = cb;
    dma_channels[channel].callback_data = user_data;

#ifdef PICO_BUILD
    dma_channel_set_irq0_enabled(channel, cb != NULL);
#endif

    dmesg_debug("dma: ch%d callback %s", channel, cb ? "set" : "cleared");
    return 0;
}

int dma_hal_sniff_enable(int channel, dma_sniff_mode_t mode) {
    if (channel < 0 || channel >= DMA_NUM_CHANNELS) {
        dmesg_err("dma: sniff invalid channel %d", channel);
        return -1;
    }

    if (!dma_channels[channel].claimed) {
        dmesg_err("dma: sniff channel %d not claimed", channel);
        return -1;
    }

#ifdef PICO_BUILD
    dma_sniffer_enable(channel, (uint)mode, true);
#endif

    sniff_channel = channel;
    sniff_active = true;
    dmesg_info("dma: sniffer enabled on ch%d mode=%d", channel, mode);
    return 0;
}

uint32_t dma_hal_sniff_result(void) {
#ifdef PICO_BUILD
    if (sniff_active) {
        return dma_hw->sniff_data;
    }
#endif
    return 0;
}

void dma_hal_sniff_reset(uint32_t seed) {
#ifdef PICO_BUILD
    if (sniff_active) {
        dma_hw->sniff_data = seed;
    }
#else
    (void)seed;
#endif
}

int dma_hal_set_timer(uint8_t timer_num, uint16_t numerator, uint16_t denominator) {
    if (timer_num > 3) {
        dmesg_err("dma: invalid timer %u (0-3)", timer_num);
        return -1;
    }

    if (denominator == 0) {
        dmesg_err("dma: timer denominator cannot be zero");
        return -1;
    }

#ifdef PICO_BUILD
    dma_timer_set_fraction(timer_num, numerator, denominator);
#else
    (void)numerator;
#endif

    dmesg_info("dma: timer%u set %u/%u", timer_num, numerator, denominator);
    return 0;
}
