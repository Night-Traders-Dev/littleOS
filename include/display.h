#ifndef LITTLEOS_DISPLAY_H
#define LITTLEOS_DISPLAY_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT  64
#define DISPLAY_PAGES   (DISPLAY_HEIGHT / 8)
#define DISPLAY_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_PAGES)

typedef enum {
    DISPLAY_NONE    = 0,
    DISPLAY_SSD1306 = 1,
} display_type_t;

void display_init(uint8_t i2c_addr);
void display_clear(void);
void display_flush(void);
void display_pixel(int x, int y, bool on);
void display_line(int x0, int y0, int x1, int y1);
void display_rect(int x, int y, int w, int h, bool fill);
void display_text(int x, int y, const char *text);
void display_printf(int x, int y, const char *fmt, ...);
bool display_is_connected(void);
void display_set_contrast(uint8_t contrast);
void display_invert(bool invert);

#ifdef __cplusplus
}
#endif
#endif
