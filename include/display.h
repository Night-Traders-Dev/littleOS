/* display.h - OLED Display HAL for littleOS
 *
 * Provides drawing primitives and framebuffer management.
 * Hardware-specific drivers (SSD1306, SH1107, etc.) register
 * as kernel modules and plug into this subsystem.
 *
 * The drawing API works in a framebuffer; call display_flush()
 * to push changes to the physical display via the active driver.
 */
#ifndef LITTLEOS_DISPLAY_H
#define LITTLEOS_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max framebuffer covers both 128x64 and 64x128 (1024 bytes each) */
#define DISPLAY_MAX_BUF_SIZE  1024

/* Display types */
typedef enum {
    DISPLAY_NONE    = 0,
    DISPLAY_SSD1306 = 1,   /* 128x64 I2C */
    DISPLAY_SH1107  = 2,   /* 64x128 SPI */
} display_type_t;

/* ---- Lifecycle ---- */

/* Initialize SSD1306 over I2C (128x64).
 * Uses I2C0 on the default pins.  Typical address: 0x3C. */
void display_init(uint8_t i2c_addr);

/* Initialize SH1107 over SPI (64x128).
 * Default Waveshare Pico-OLED-1.3 pins:
 *   spi_inst=1, mosi=GP11, sck=GP10, cs=GP9, dc=GP8, rst=GP12
 * Pass 0xFF for any pin to use the default. */
void display_init_sh1107(uint8_t spi_inst, uint8_t mosi, uint8_t sck,
                         uint8_t cs, uint8_t dc, uint8_t rst);

/* ---- Drawing (operates on framebuffer) ---- */

void display_clear(void);
void display_flush(void);
void display_pixel(int x, int y, bool on);
void display_line(int x0, int y0, int x1, int y1);
void display_rect(int x, int y, int w, int h, bool fill);
void display_circle(int cx, int cy, int r, bool fill);
void display_text(int x, int y, const char *text);
void display_printf(int x, int y, const char *fmt, ...);
void display_bitmap(int x, int y, int w, int h, const uint8_t *data);

/* ---- Control ---- */

bool          display_is_connected(void);
display_type_t display_get_type(void);
int           display_get_width(void);
int           display_get_height(void);
void          display_set_contrast(uint8_t contrast);
void          display_invert(bool invert);
void          display_scroll_up(int lines);

/* ---- Framebuffer access (for drivers) ---- */

/* Get pointer to the raw framebuffer (page-format, SSD1306-style) */
uint8_t *display_get_framebuffer(void);

/* ---- Backward-compat constants (SSD1306 defaults) ---- */
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_PAGES   (DISPLAY_HEIGHT / 8)
#define DISPLAY_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_PAGES)

#ifdef __cplusplus
}
#endif
#endif /* LITTLEOS_DISPLAY_H */
