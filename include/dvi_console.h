/* dvi_console.h - DVI Text Console for littleOS
 *
 * Renders a TTY-style text console on the DVI framebuffer.
 * Supports ANSI escape codes for colors, cursor movement, and clearing.
 * Auto-enabled on RP2350 boards with HSTX during boot.
 *
 * Resolution: 320x240 RGB332 → 80x30 characters (4x8 font)
 */
#ifndef LITTLEOS_DVI_CONSOLE_H
#define LITTLEOS_DVI_CONSOLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Console dimensions (4x8 font in 320x240 framebuffer) */
#define DVI_CONSOLE_COLS    80
#define DVI_CONSOLE_ROWS    30
#define DVI_CONSOLE_FONT_W  4
#define DVI_CONSOLE_FONT_H  8

/* Initialize the DVI console (calls hstx_dvi_init + start).
 * Returns 0 on success, -1 on failure. */
int dvi_console_init(void);

/* Shut down the DVI console and release hardware. */
void dvi_console_deinit(void);

/* Write characters to the console (handles ANSI escapes, newlines, etc.).
 * This is the main entry point — hook it into stdout. */
void dvi_console_write(const char *data, size_t len);

/* Write a single character. */
void dvi_console_putchar(char c);

/* Clear the entire console. */
void dvi_console_clear(void);

/* Check if the DVI console is active. */
bool dvi_console_is_active(void);

/* Install/remove the DVI console as a printf output hook.
 * When installed, all printf/putchar output is mirrored to the DVI display. */
void dvi_console_install_hook(void);
void dvi_console_remove_hook(void);

#ifdef __cplusplus
}
#endif
#endif /* LITTLEOS_DVI_CONSOLE_H */
