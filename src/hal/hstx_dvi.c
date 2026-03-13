/* hstx_dvi.c - HSTX DVI Output Driver for RP2350
 *
 * Implements DVI output using the RP2350 HSTX peripheral.
 * HSTX provides hardware TMDS encoding and shift-register-based
 * serialization on GPIO 12-19.
 *
 * DVI timing uses DMA to feed pixel data from a framebuffer
 * through the HSTX FIFO with DREQ pacing.
 */

#include "hal/hstx_dvi.h"
#include "dmesg.h"
#include <string.h>
#include <stdio.h>

#if LITTLEOS_HAS_HSTX

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/regs/dreq.h"

/* HSTX uses GPIO 12-19 (8 output pins) */
#define HSTX_GPIO_BASE  12
#define HSTX_GPIO_COUNT 8

/* 640x480@60Hz timing constants (in pixel clocks)
 * Pixel clock: 25.175 MHz
 * H: 640 active + 16 front porch + 96 sync + 48 back porch = 800 total
 * V: 480 active + 10 front porch + 2 sync + 33 back porch = 525 total */
#define VGA_H_ACTIVE    640
#define VGA_H_FRONT     16
#define VGA_H_SYNC      96
#define VGA_H_BACK      48
#define VGA_H_TOTAL     (VGA_H_ACTIVE + VGA_H_FRONT + VGA_H_SYNC + VGA_H_BACK)

#define VGA_V_ACTIVE    480
#define VGA_V_FRONT     10
#define VGA_V_SYNC      2
#define VGA_V_BACK      33
#define VGA_V_TOTAL     (VGA_V_ACTIVE + VGA_V_FRONT + VGA_V_SYNC + VGA_V_BACK)

/* Driver state */
static struct {
    bool initialized;
    bool active;
    dvi_mode_t mode;
    dvi_pixel_format_t format;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;            /* bytes per pixel */
    const uint8_t *framebuffer;
    uint32_t fb_size;
    int dma_channel;
    uint32_t frame_count;
} s_dvi;

/* Configure HSTX GPIO pins for HSTX function */
static void hstx_gpio_init(void) {
    for (int i = 0; i < HSTX_GPIO_COUNT; i++) {
        gpio_set_function(HSTX_GPIO_BASE + i, GPIO_FUNC_HSTX);
    }
    dmesg_debug("hstx_dvi: GPIO %d-%d configured for HSTX",
                HSTX_GPIO_BASE, HSTX_GPIO_BASE + HSTX_GPIO_COUNT - 1);
}

/* Configure HSTX control registers for TMDS encoding */
static void hstx_configure_tmds(void) {
    /* Reset HSTX */
    hstx_ctrl_hw->csr = 0;

    /* Configure HSTX for TMDS output:
     * - Clock divider for pixel clock generation
     * - TMDS encoding enabled via expand mode
     * - 3 TMDS data lanes + 1 clock lane mapped to 8 GPIO pins
     *
     * Pin mapping (typical DVI-over-HSTX):
     *   GPIO12-13: TMDS Lane 2 (Blue)  differential pair
     *   GPIO14-15: TMDS Lane 1 (Green) differential pair
     *   GPIO16-17: TMDS Lane 0 (Red)   differential pair
     *   GPIO18-19: TMDS Clock          differential pair
     *
     * Each lane uses 2 pins for differential signaling.
     * The HSTX shift register feeds the TMDS encoder which
     * produces 10-bit symbols serialized across the pin pairs.
     */

    /* Configure bit outputs: each BITn register maps a shift register
     * bit to an output pin. For TMDS, we configure the expand/TMDS
     * mode which handles the encoding automatically. */

    /* Set clock divider: determines HSTX output rate
     * For 640x480@60Hz, we need ~252 MHz HSTX clock for 25.2 MHz pixel
     * clock with 10:1 TMDS serialization */
    uint32_t csr = 0;
    csr |= (1u << HSTX_CTRL_CSR_CLKDIV_LSB);     /* clkdiv = 1 */
    csr |= (5u << HSTX_CTRL_CSR_N_SHIFTS_LSB);    /* 5 shifts per refill */
    csr |= HSTX_CTRL_CSR_EXPAND_EN_BITS;          /* Enable TMDS expand */

    hstx_ctrl_hw->csr = csr;

    dmesg_debug("hstx_dvi: TMDS encoding configured, CSR=0x%08lx",
                (unsigned long)hstx_ctrl_hw->csr);
}

int hstx_dvi_init(dvi_mode_t mode, dvi_pixel_format_t format) {
    if (s_dvi.initialized) {
        dmesg_warn("hstx_dvi: already initialized");
        return -1;
    }

    memset(&s_dvi, 0, sizeof(s_dvi));

    switch (mode) {
    case DVI_MODE_640x480_60HZ:
        s_dvi.width = 640;
        s_dvi.height = 480;
        break;
    case DVI_MODE_320x240_60HZ:
        s_dvi.width = 320;
        s_dvi.height = 240;
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
        break;
    default:
        dmesg_err("hstx_dvi: invalid pixel format %d", format);
        return -1;
    }

    s_dvi.mode = mode;
    s_dvi.format = format;
    s_dvi.dma_channel = -1;

    /* Configure GPIO pins for HSTX */
    hstx_gpio_init();

    /* Configure HSTX for TMDS encoding */
    hstx_configure_tmds();

    /* Claim a DMA channel for framebuffer scanout */
    s_dvi.dma_channel = dma_claim_unused_channel(false);
    if (s_dvi.dma_channel < 0) {
        dmesg_err("hstx_dvi: no DMA channel available");
        return -1;
    }

    s_dvi.initialized = true;
    dmesg_info("hstx_dvi: initialized %ux%u %s",
               s_dvi.width, s_dvi.height,
               format == DVI_PIXEL_RGB332 ? "RGB332" : "RGB565");

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

    dmesg_debug("hstx_dvi: framebuffer set at %p (%lu bytes)",
                (const void *)fb, (unsigned long)fb_size);
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

    /* Configure DMA to feed framebuffer into HSTX FIFO */
    dma_channel_config cfg = dma_channel_get_default_config(s_dvi.dma_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, DREQ_HSTX);

    dma_channel_configure(
        s_dvi.dma_channel,
        &cfg,
        &hstx_fifo_hw->fifo,           /* Write to HSTX FIFO */
        s_dvi.framebuffer,              /* Read from framebuffer */
        s_dvi.fb_size / 4,             /* Transfer count (32-bit words) */
        false                           /* Don't start yet */
    );

    /* Enable HSTX */
    hstx_ctrl_hw->csr |= HSTX_CTRL_CSR_EN_BITS;

    /* Start DMA */
    dma_channel_start(s_dvi.dma_channel);

    s_dvi.active = true;
    dmesg_info("hstx_dvi: output started (%ux%u)", s_dvi.width, s_dvi.height);

    return 0;
}

int hstx_dvi_stop(void) {
    if (!s_dvi.initialized || !s_dvi.active) {
        return 0;
    }

    /* Stop DMA */
    dma_channel_abort(s_dvi.dma_channel);

    /* Disable HSTX */
    hstx_ctrl_hw->csr &= ~HSTX_CTRL_CSR_EN_BITS;

    s_dvi.active = false;
    s_dvi.frame_count = 0;
    dmesg_info("hstx_dvi: output stopped");

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

    return 0;
}

void hstx_dvi_fill(uint8_t *fb, uint32_t fb_size, uint32_t color) {
    if (!fb) return;
    memset(fb, (int)(color & 0xFF), fb_size);
}

void hstx_dvi_test_pattern(uint8_t *fb, uint16_t width, uint16_t height,
                           dvi_pixel_format_t format) {
    if (!fb) return;

    /* Draw vertical color bars */
    uint16_t bar_width = width / 8;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint8_t bar = x / bar_width;
            uint32_t offset;

            if (format == DVI_PIXEL_RGB332) {
                /* 8-bit color: RRRGGGBB */
                static const uint8_t bars_rgb332[] = {
                    0xFF, 0xE0, 0x1C, 0xFC, 0x03, 0xE3, 0x1F, 0x00
                };
                offset = (uint32_t)y * width + x;
                fb[offset] = bars_rgb332[bar & 7];
            } else {
                /* 16-bit RGB565 */
                static const uint16_t bars_rgb565[] = {
                    0xFFFF, 0xFFE0, 0x07E0, 0x07FF,
                    0x001F, 0xF81F, 0xF800, 0x0000
                };
                offset = ((uint32_t)y * width + x) * 2;
                uint16_t c = bars_rgb565[bar & 7];
                fb[offset] = c & 0xFF;
                fb[offset + 1] = c >> 8;
            }
        }
    }
}

#else /* !LITTLEOS_HAS_HSTX */

/* Stubs for non-RP2350 builds */
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
