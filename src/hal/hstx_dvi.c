/* hstx_dvi.c - HSTX DVI Output Driver for RP2350
 *
 * Functional DVI output using the RP2350 HSTX peripheral with hardware
 * TMDS encoding. Based on the official Pico SDK dvi_out_hstx_encoder example.
 *
 * The HSTX command expander generates proper DVI timing with TMDS-encoded
 * pixel data. DMA ping-pong channels provide glitch-free continuous scanout
 * via IRQ-driven per-scanline reconfiguration.
 *
 * Pin mapping (Pico DVI Sock compatible):
 *   GPIO 12-13: TMDS D0+/D0- (Lane 0)
 *   GPIO 14-15: TMDS CK+/CK- (Clock)
 *   GPIO 16-17: TMDS D2+/D2- (Lane 2)
 *   GPIO 18-19: TMDS D1+/D1- (Lane 1)
 *
 * Requires 270-ohm current-limiting resistors on each GPIO pin.
 */

#include "hal/hstx_dvi.h"
#include "dmesg.h"
#include <string.h>
#include <stdio.h>

#if LITTLEOS_HAS_HSTX

#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"

/* =========================================================================
 * DVI / TMDS Constants
 * ========================================================================= */

/* Pre-encoded 10-bit TMDS control symbols (used during blanking/sync) */
#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

/* Sync symbol combinations: each 30-bit word packs 3 lanes (10 bits each)
 * Lane order in the 30-bit word: [29:20]=Lane2, [19:10]=Lane1, [9:0]=Lane0
 * HSync and VSync are active-low for 640x480 mode */
#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

/* 640x480 @ 60 Hz VGA timing (pixel clock 25.175 MHz)
 * HSTX at 125 MHz with 2 bits/cycle = 250 Mbps ~ 252 MHz needed */
#define MODE_H_FRONT_PORCH   16
#define MODE_H_SYNC_WIDTH    96
#define MODE_H_BACK_PORCH    48
#define MODE_H_ACTIVE_PIXELS 640

#define MODE_V_FRONT_PORCH   10
#define MODE_V_SYNC_WIDTH    2
#define MODE_V_BACK_PORCH    33
#define MODE_V_ACTIVE_LINES  480

#define MODE_V_TOTAL_LINES ( \
    MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + \
    MODE_V_BACK_PORCH  + MODE_V_ACTIVE_LINES \
)

/* HSTX command expander opcodes (bits [15:12] of command word) */
#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

/* DMA channel indices */
#define DMACH_PING 0
#define DMACH_PONG 1

/* =========================================================================
 * HSTX Command Lists (per-scanline DMA payloads)
 * ========================================================================= */

/* Vertical blanking line (VSync inactive):
 * Front porch + HSync + Back porch + Inactive active region */
static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V1_H1,
    HSTX_CMD_NOP
};

/* Vertical blanking line (VSync active) */
static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V0_H1,
    HSTX_CMD_NOP
};

/* Active video line (command portion — pixel data follows via separate DMA):
 * Front porch + HSync + Back porch + TMDS pixel command */
static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS       | MODE_H_ACTIVE_PIXELS
};

/* =========================================================================
 * Driver State
 * ========================================================================= */

static struct {
    bool            initialized;
    bool            active;
    dvi_mode_t      mode;
    dvi_pixel_format_t format;
    uint16_t        width;          /* Framebuffer width (always 640 for DVI) */
    uint16_t        height;         /* Framebuffer height (240 or 480) */
    uint8_t         bpp;            /* Bytes per pixel */
    uint8_t         vscale;         /* Vertical scale factor (1 or 2) */
    const uint8_t  *framebuffer;
    uint32_t        fb_size;
    int             dma_ping;       /* DMA channel A */
    int             dma_pong;       /* DMA channel B */
    volatile uint32_t frame_count;
} s_dvi;

/* IRQ state — must survive across interrupts */
static volatile uint     s_v_scanline = 0;
static volatile bool     s_vactive_cmdlist_posted = false;

/* =========================================================================
 * DMA IRQ Handler (runs in RAM for speed)
 *
 * No chain_to: each channel is started manually by the ISR after
 * reconfiguring it. The 8-entry HSTX FIFO absorbs the brief gap
 * between one channel finishing and the next starting (~1 µs).
 *
 * chain_to was removed because at startup the FIFO can absorb both
 * 7-word vblank transfers without DREQ stalling, causing both channels
 * to complete before any ISR runs. The chained channel then has
 * TRANS_COUNT=0 → zero-length completion → IRQ storm → CPU hang.
 * ========================================================================= */

static void __not_in_flash_func(hstx_dma_irq_handler)(void) {
    /* Determine which channel completed from hardware flags */
    uint32_t ints = dma_hw->ints1;
    uint ch_next;

    if (ints & (1u << s_dvi.dma_ping)) {
        dma_hw->ints1 = 1u << s_dvi.dma_ping;
        ch_next = s_dvi.dma_pong;
    } else {
        dma_hw->ints1 = 1u << s_dvi.dma_pong;
        ch_next = s_dvi.dma_ping;
    }

    /* Configure the OTHER channel for the next scanline segment */
    dma_channel_hw_t *ch = &dma_hw->ch[ch_next];
    uint v = s_v_scanline;

    if (v >= MODE_V_FRONT_PORCH &&
        v < (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH)) {
        /* VSync active region */
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v < MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
        /* VBlank (VSync inactive) */
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else if (!s_vactive_cmdlist_posted) {
        /* Active region: first DMA for this line sends the command list */
        ch->read_addr = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        s_vactive_cmdlist_posted = true;
    } else {
        /* Active region: second DMA for this line sends pixel data.
         * DMA reads directly from the framebuffer — no per-pixel loops
         * in the ISR. Horizontal doubling is done at render time;
         * vertical doubling is just an index division here. */
        uint active_line = v - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES);
        uint fb_row = (s_dvi.vscale > 1) ? active_line / s_dvi.vscale
                                         : active_line;

        ch->read_addr = (uintptr_t)&s_dvi.framebuffer[
            fb_row * s_dvi.width * s_dvi.bpp];
        ch->transfer_count = MODE_H_ACTIVE_PIXELS / sizeof(uint32_t);
        s_vactive_cmdlist_posted = false;
    }

    /* Start the next channel */
    dma_channel_start(ch_next);

    /* Advance scanline only after both command list and pixels are posted */
    if (!s_vactive_cmdlist_posted) {
        s_v_scanline = (v + 1) % MODE_V_TOTAL_LINES;

        /* Track frame count at start of vblank */
        if (s_v_scanline == 0) {
            s_dvi.frame_count++;
        }
    }
}

/* =========================================================================
 * HSTX Hardware Configuration
 * ========================================================================= */

static void hstx_configure(dvi_pixel_format_t format) {
    /* Configure TMDS encoder for the pixel format.
     *
     * The TMDS encoder takes N bits per lane from the shift register,
     * after applying a per-lane rotation. The rotation positions the
     * correct color channel bits at the encoder input.
     *
     * RGB332 (RRRGGGBB, 8 bits per pixel, 4 pixels per 32-bit word):
     *   Lane 2: 3 bits, rot 0  → bits [7:5] = Red
     *   Lane 1: 3 bits, rot 29 → bits [4:2] = Green (after rotation)
     *   Lane 0: 2 bits, rot 26 → bits [1:0] = Blue (after rotation)
     *
     * RGB565 (RRRRRGGGGGGBBBBB, 16 bits per pixel, 2 pixels per word):
     *   Lane 2: 5 bits, rot 24 → bits [15:11] = Red
     *   Lane 1: 6 bits, rot 29 → bits [10:5]  = Green
     *   Lane 0: 5 bits, rot 27 → bits [4:0]   = Blue
     */

    if (format == DVI_PIXEL_RGB332) {
        hstx_ctrl_hw->expand_tmds =
            2  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            2  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

        /* 4 pixels per 32-bit word, advance 8 bits per pixel */
        hstx_ctrl_hw->expand_shift =
            4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
    } else {
        /* RGB565 */
        hstx_ctrl_hw->expand_tmds =
            4  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
            24 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
            5  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
            29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
            4  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
            27 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

        /* 2 pixels per 32-bit word, advance 16 bits per pixel */
        hstx_ctrl_hw->expand_shift =
            2  << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
            16 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
            1  << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
            0  << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;
    }

    /* Serial output config:
     * CLKDIV=5: Clock period of 5 HSTX cycles
     * N_SHIFTS=5: Pop from expander every 5 cycles
     * SHIFT=2: Right-rotate shift register by 2 bits each cycle (DDR)
     * EXPAND_EN: Command expander enabled
     * EN is NOT set here — deferred to hstx_dvi_start() so DMA is
     * ready before the HSTX begins consuming FIFO entries.
     *
     * At 125 MHz HSTX clock, 2 bits/cycle = 250 Mbps output rate.
     * This is within spec for 640x480@60Hz (needs 252 Mbps). */
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB;

    /* Configure HSTX output pin mapping.
     *
     * HSTX outputs 0-7 map to GPIO 12-19. Each TMDS lane uses a
     * differential pair (2 pins). The second pin is inverted.
     *
     * Pinout (Pico DVI Sock):
     *   Output 0-1 (GPIO 12-13): TMDS D0+/D0- (Lane 0)
     *   Output 2-3 (GPIO 14-15): TMDS CK+/CK- (Clock)
     *   Output 4-5 (GPIO 16-17): TMDS D2+/D2- (Lane 2)
     *   Output 6-7 (GPIO 18-19): TMDS D1+/D1- (Lane 1)
     */

    /* Clock pair on outputs 2-3 */
    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;

    /* Data lanes: each TMDS lane serializes 10-bit symbols.
     * The shift register outputs even bits on SEL_P (first half-clock)
     * and odd bits on SEL_N (second half-clock). SHIFT=2 advances
     * by 2 bits each cycle, so we get DDR serialization. */
    static const int lane_to_output_bit[3] = {0, 6, 4};
    for (uint lane = 0; lane < 3; lane++) {
        int bit = lane_to_output_bit[lane];
        uint32_t lane_data_sel =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit]     = lane_data_sel;
        hstx_ctrl_hw->bit[bit + 1] = lane_data_sel | HSTX_CTRL_BIT0_INV_BITS;
    }

    /* Set GPIO 12-19 to HSTX function */
    for (int i = 12; i <= 19; i++) {
        gpio_set_function(i, GPIO_FUNC_HSTX);
    }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

int hstx_dvi_init(dvi_mode_t mode, dvi_pixel_format_t format) {
    if (s_dvi.initialized) {
        dmesg_warn("hstx_dvi: already initialized, call stop first");
        return -1;
    }

    memset((void *)&s_dvi, 0, sizeof(s_dvi));

    /* Hardware reset HSTX to ensure clean state */
    reset_block(RESETS_RESET_HSTX_BITS);
    unreset_block_wait(RESETS_RESET_HSTX_BITS);

    switch (mode) {
    case DVI_MODE_640x480_60HZ:
        s_dvi.width = 640;
        s_dvi.height = 480;
        s_dvi.vscale = 1;
        break;
    case DVI_MODE_320x240_60HZ:
        /* Framebuffer is 640×240 — horizontal doubling is done at render
         * time, vertical doubling via row repetition (active_line / 2).
         * This keeps the ISR loop-free so DMA chain_to is safe. */
        s_dvi.width = 640;
        s_dvi.height = 240;
        s_dvi.vscale = 2;
        break;
    default:
        dmesg_err("hstx_dvi: invalid mode %d", mode);
        return -1;
    }

    switch (format) {
    case DVI_PIXEL_RGB332:
        s_dvi.bpp = 1;
        break;
    case DVI_PIXEL_RGB565:
        s_dvi.bpp = 2;
        if (s_dvi.width * s_dvi.height * 2 > 256 * 1024) {
            dmesg_err("hstx_dvi: RGB565 at %ux%u requires %uKB — exceeds budget",
                      s_dvi.width, s_dvi.height,
                      (s_dvi.width * s_dvi.height * 2) / 1024);
            return -1;
        }
        break;
    default:
        dmesg_err("hstx_dvi: invalid pixel format %d", format);
        return -1;
    }

    s_dvi.mode = mode;
    s_dvi.format = format;

    /* Claim two DMA channels for ping-pong scanout */
    s_dvi.dma_ping = dma_claim_unused_channel(false);
    if (s_dvi.dma_ping < 0) {
        dmesg_err("hstx_dvi: no DMA channel available (ping)");
        return -1;
    }

    s_dvi.dma_pong = dma_claim_unused_channel(false);
    if (s_dvi.dma_pong < 0) {
        dma_channel_unclaim(s_dvi.dma_ping);
        dmesg_err("hstx_dvi: no DMA channel available (pong)");
        return -1;
    }

    /* Configure HSTX hardware */
    hstx_configure(format);

    s_dvi.initialized = true;
    dmesg_info("hstx_dvi: initialized %ux%u %s (DMA ch %d/%d)",
               s_dvi.width, s_dvi.height,
               format == DVI_PIXEL_RGB332 ? "RGB332" : "RGB565",
               s_dvi.dma_ping, s_dvi.dma_pong);

    return 0;
}

int hstx_dvi_set_framebuffer(const uint8_t *fb, uint32_t fb_size) {
    if (!s_dvi.initialized) {
        dmesg_err("hstx_dvi: not initialized");
        return -1;
    }

    uint32_t expected = (uint32_t)s_dvi.width * s_dvi.height * s_dvi.bpp;
    if (fb_size < expected) {
        dmesg_err("hstx_dvi: framebuffer too small (%lu < %lu)",
                  (unsigned long)fb_size, (unsigned long)expected);
        return -1;
    }

    s_dvi.framebuffer = fb;
    s_dvi.fb_size = fb_size;
    return 0;
}

int hstx_dvi_start(void) {
    if (!s_dvi.initialized) {
        dmesg_err("hstx_dvi: not initialized");
        return -1;
    }
    if (!s_dvi.framebuffer) {
        dmesg_err("hstx_dvi: no framebuffer set");
        return -1;
    }
    if (s_dvi.active) {
        dmesg_warn("hstx_dvi: already active");
        return 0;
    }

    /* Reset scanline state */
    s_v_scanline = 0;
    s_vactive_cmdlist_posted = false;
    s_dvi.frame_count = 0;

    /* Configure DMA channels (NO chain_to — ISR starts each manually).
     *
     * chain_to was removed because the 8-entry HSTX FIFO can absorb
     * both initial 7-word vblank transfers without DREQ stalling.
     * Both channels would complete before any ISR runs, and the
     * chained channel (with TRANS_COUNT=0) fires a zero-length
     * transfer → immediate completion → IRQ storm → CPU hang. */
    dma_channel_config c;

    c = dma_channel_get_default_config(s_dvi.dma_ping);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        s_dvi.dma_ping,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );

    c = dma_channel_get_default_config(s_dvi.dma_pong);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        s_dvi.dma_pong,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );

    /* Enable DMA completion IRQs for both channels BEFORE starting.
     * Use DMA_IRQ_1 to avoid conflict with dma_hal (which uses DMA_IRQ_0).
     * Set LOWEST priority so a misbehaving ISR can't starve USB/SysTick. */
    dma_hw->ints1 = (1u << s_dvi.dma_ping) | (1u << s_dvi.dma_pong);
    dma_hw->inte1 = (1u << s_dvi.dma_ping) | (1u << s_dvi.dma_pong);
    irq_set_exclusive_handler(DMA_IRQ_1, hstx_dma_irq_handler);
    irq_set_priority(DMA_IRQ_1, PICO_LOWEST_IRQ_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);

    /* Start DMA ping first (DREQ-paced, won't transfer until HSTX runs) */
    dma_channel_start(s_dvi.dma_ping);

    /* Enable HSTX — begins clocking and generating DREQ.
     * DMA is already started and waiting, so data flows immediately. */
    hstx_ctrl_hw->csr |= HSTX_CTRL_CSR_EN_BITS;

    s_dvi.active = true;

    /* Verify DVI is actually producing frames (timeout 200ms).
     * If the ISR is broken (e.g. IRQ storm), frame_count won't advance
     * normally. At 60fps we expect ~12 frames in 200ms. */
    uint64_t deadline = time_us_64() + 200000;
    while (s_dvi.frame_count < 2) {
        if (time_us_64() > deadline) {
            dmesg_err("hstx_dvi: no frames produced in 200ms, aborting");
            hstx_dvi_stop();
            return -1;
        }
        tight_loop_contents();
    }

    dmesg_info("hstx_dvi: output started (%ux%u → 640x480)",
               s_dvi.width, s_dvi.height);

    return 0;
}

int hstx_dvi_stop(void) {
    if (!s_dvi.initialized) {
        return 0;
    }

    if (s_dvi.active) {
        /* Disable IRQs first (using DMA_IRQ_1) */
        irq_set_enabled(DMA_IRQ_1, false);
        dma_hw->inte1 &= ~((1u << s_dvi.dma_ping) | (1u << s_dvi.dma_pong));

        /* Abort both DMA channels */
        dma_channel_abort(s_dvi.dma_ping);
        dma_channel_abort(s_dvi.dma_pong);

        /* Disable HSTX */
        hstx_ctrl_hw->csr &= ~HSTX_CTRL_CSR_EN_BITS;

        s_dvi.active = false;
        dmesg_info("hstx_dvi: output stopped");
    }

    /* Release DMA channels */
    dma_channel_unclaim(s_dvi.dma_ping);
    dma_channel_unclaim(s_dvi.dma_pong);

    /* Reset GPIO pins to default */
    for (int i = 12; i <= 19; i++) {
        gpio_set_function(i, GPIO_FUNC_SIO);
        gpio_disable_pulls(i);
    }

    s_dvi.initialized = false;
    return 0;
}

bool hstx_dvi_is_active(void) {
    return s_dvi.active;
}

int hstx_dvi_get_status(dvi_status_t *status) {
    if (!status) return -1;

    status->active = s_dvi.active;
    status->mode = s_dvi.mode;
    status->format = s_dvi.format;
    status->width = s_dvi.width;
    status->height = s_dvi.height;
    status->frame_count = s_dvi.frame_count;

    return s_dvi.initialized ? 0 : -1;
}

void hstx_dvi_fill(uint8_t *fb, uint32_t fb_size, uint32_t color) {
    if (!fb || !fb_size) return;

    if (s_dvi.format == DVI_PIXEL_RGB565) {
        /* Fill with 16-bit color */
        uint16_t c16 = (uint16_t)(color & 0xFFFF);
        uint16_t *fb16 = (uint16_t *)fb;
        uint32_t count = fb_size / 2;
        for (uint32_t i = 0; i < count; i++) {
            fb16[i] = c16;
        }
    } else {
        /* Fill with 8-bit RGB332 color */
        memset(fb, (int)(color & 0xFF), fb_size);
    }
}

void hstx_dvi_test_pattern(uint8_t *fb, uint16_t width, uint16_t height,
                           dvi_pixel_format_t format) {
    if (!fb) return;

    uint16_t bar_width = width / 8;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint8_t bar = x / bar_width;

            if (format == DVI_PIXEL_RGB332) {
                /* 8-bit RRRGGGBB color bars:
                 * White, Yellow, Cyan, Green, Magenta, Red, Blue, Black */
                static const uint8_t bars[] = {
                    0xFF, 0xFC, 0x1F, 0x1C, 0xE3, 0xE0, 0x03, 0x00
                };
                uint32_t offset = (uint32_t)y * width + x;
                fb[offset] = bars[bar & 7];
            } else {
                /* 16-bit RGB565 color bars */
                static const uint16_t bars[] = {
                    0xFFFF, 0xFFE0, 0x07FF, 0x07E0,
                    0xF81F, 0xF800, 0x001F, 0x0000
                };
                uint32_t offset = ((uint32_t)y * width + x) * 2;
                uint16_t c = bars[bar & 7];
                fb[offset]     = c & 0xFF;
                fb[offset + 1] = c >> 8;
            }
        }
    }
}

#else /* !LITTLEOS_HAS_HSTX */

/* Non-RP2350 builds: these functions are never called because the shell
 * command is guarded by LITTLEOS_HAS_HSTX, but we provide error returns
 * for safety. */

int hstx_dvi_init(dvi_mode_t mode, dvi_pixel_format_t format) {
    (void)mode; (void)format;
    printf("HSTX DVI not available on this platform\r\n");
    return -1;
}

int hstx_dvi_set_framebuffer(const uint8_t *fb, uint32_t fb_size) {
    (void)fb; (void)fb_size; return -1;
}

int hstx_dvi_start(void) { return -1; }
int hstx_dvi_stop(void) { return 0; }
bool hstx_dvi_is_active(void) { return false; }

int hstx_dvi_get_status(dvi_status_t *status) {
    if (status) memset(status, 0, sizeof(*status));
    return -1;
}

void hstx_dvi_fill(uint8_t *fb, uint32_t fb_size, uint32_t color) {
    (void)fb; (void)fb_size; (void)color;
}

void hstx_dvi_test_pattern(uint8_t *fb, uint16_t width, uint16_t height,
                           dvi_pixel_format_t format) {
    (void)fb; (void)width; (void)height; (void)format;
}

#endif /* LITTLEOS_HAS_HSTX */
