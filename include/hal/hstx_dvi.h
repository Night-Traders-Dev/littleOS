/* hstx_dvi.h - HSTX DVI Output Driver for RP2350
 *
 * Uses the RP2350's HSTX peripheral with built-in TMDS encoding
 * to generate DVI output via GPIO 12-19 (HSTX pins).
 *
 * Only available on RP2350 builds (guarded by LITTLEOS_HAS_HSTX).
 */
#ifndef LITTLEOS_HAL_HSTX_DVI_H
#define LITTLEOS_HAL_HSTX_DVI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DVI resolution modes */
typedef enum {
    DVI_MODE_640x480_60HZ,      /* Standard VGA, 25.175 MHz pixel clock */
    DVI_MODE_320x240_60HZ,      /* Pixel-doubled, lower bandwidth */
} dvi_mode_t;

/* DVI pixel format */
typedef enum {
    DVI_PIXEL_RGB332,           /* 8-bit: 3R + 3G + 2B (1 byte/pixel) */
    DVI_PIXEL_RGB565,           /* 16-bit: 5R + 6G + 5B (2 bytes/pixel) */
} dvi_pixel_format_t;

/* DVI status information */
typedef struct {
    bool active;                /* DVI output is running */
    dvi_mode_t mode;            /* Current mode */
    dvi_pixel_format_t format;  /* Current pixel format */
    uint16_t width;             /* Horizontal resolution */
    uint16_t height;            /* Vertical resolution */
    uint32_t frame_count;       /* Total frames rendered */
} dvi_status_t;

/* Initialize HSTX for DVI output.
 * Configures GPIO 12-19 for HSTX function and sets up TMDS encoding.
 * Returns 0 on success, -1 on error. */
int hstx_dvi_init(dvi_mode_t mode, dvi_pixel_format_t format);

/* Set the framebuffer for DMA-driven scanout.
 * The buffer must remain valid while DVI output is active.
 * Buffer size: width * height * bytes_per_pixel */
int hstx_dvi_set_framebuffer(const uint8_t *fb, uint32_t fb_size);

/* Start DVI output (begins DMA scanout of framebuffer). */
int hstx_dvi_start(void);

/* Stop DVI output and release HSTX/DMA resources. */
int hstx_dvi_stop(void);

/* Check if DVI output is currently active. */
bool hstx_dvi_is_active(void);

/* Get current DVI status. */
int hstx_dvi_get_status(dvi_status_t *status);

/* Fill framebuffer with a solid color (utility). */
void hstx_dvi_fill(uint8_t *fb, uint32_t fb_size, uint32_t color);

/* Draw a test pattern to framebuffer (utility). */
void hstx_dvi_test_pattern(uint8_t *fb, uint16_t width, uint16_t height,
                           dvi_pixel_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_HSTX_DVI_H */
