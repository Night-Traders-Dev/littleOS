/* display.c - Display subsystem for littleOS
 *
 * Manages the shared framebuffer and drawing primitives.
 * Hardware-specific code lives in driver modules (ssd1306.c, sh1107.c).
 * The active driver is set via display_set_active_driver().
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "display.h"
#include "drivers/display_module.h"

/* ================================================================
 * Internal state
 * ================================================================ */

static uint8_t        framebuffer[DISPLAY_MAX_BUF_SIZE];
static display_type_t disp_type      = DISPLAY_NONE;
static int            disp_width     = 0;
static int            disp_height    = 0;
static bool           disp_connected = false;
static bool           disp_inverted  = false;

/* Active driver callbacks (set by driver modules) */
static const display_driver_ops_t *active_drv = NULL;

/* ================================================================
 * Driver module integration
 * ================================================================ */

void display_set_active_driver(const display_driver_ops_t *ops, int w, int h) {
    active_drv     = ops;
    disp_width     = w;
    disp_height    = h;
    disp_connected = true;
    disp_inverted  = false;
    memset(framebuffer, 0, sizeof(framebuffer));
}

void display_clear_active_driver(void) {
    active_drv     = NULL;
    disp_type      = DISPLAY_NONE;
    disp_width     = 0;
    disp_height    = 0;
    disp_connected = false;
}

uint8_t *display_get_framebuffer(void) {
    return framebuffer;
}

/* ================================================================
 * 5x7 font (ASCII 32-126)
 * ================================================================ */

static const uint8_t font5x7[][5] = {
    /* 32 ' ' */ {0x00, 0x00, 0x00, 0x00, 0x00},
    /* 33 '!' */ {0x00, 0x00, 0x5F, 0x00, 0x00},
    /* 34 '"' */ {0x00, 0x07, 0x00, 0x07, 0x00},
    /* 35 '#' */ {0x14, 0x7F, 0x14, 0x7F, 0x14},
    /* 36 '$' */ {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    /* 37 '%' */ {0x23, 0x13, 0x08, 0x64, 0x62},
    /* 38 '&' */ {0x36, 0x49, 0x55, 0x22, 0x50},
    /* 39 ''' */ {0x00, 0x05, 0x03, 0x00, 0x00},
    /* 40 '(' */ {0x00, 0x1C, 0x22, 0x41, 0x00},
    /* 41 ')' */ {0x00, 0x41, 0x22, 0x1C, 0x00},
    /* 42 '*' */ {0x14, 0x08, 0x3E, 0x08, 0x14},
    /* 43 '+' */ {0x08, 0x08, 0x3E, 0x08, 0x08},
    /* 44 ',' */ {0x00, 0x50, 0x30, 0x00, 0x00},
    /* 45 '-' */ {0x08, 0x08, 0x08, 0x08, 0x08},
    /* 46 '.' */ {0x00, 0x60, 0x60, 0x00, 0x00},
    /* 47 '/' */ {0x20, 0x10, 0x08, 0x04, 0x02},
    /* 48 '0' */ {0x3E, 0x51, 0x49, 0x45, 0x3E},
    /* 49 '1' */ {0x00, 0x42, 0x7F, 0x40, 0x00},
    /* 50 '2' */ {0x42, 0x61, 0x51, 0x49, 0x46},
    /* 51 '3' */ {0x21, 0x41, 0x45, 0x4B, 0x31},
    /* 52 '4' */ {0x18, 0x14, 0x12, 0x7F, 0x10},
    /* 53 '5' */ {0x27, 0x45, 0x45, 0x45, 0x39},
    /* 54 '6' */ {0x3C, 0x4A, 0x49, 0x49, 0x30},
    /* 55 '7' */ {0x01, 0x71, 0x09, 0x05, 0x03},
    /* 56 '8' */ {0x36, 0x49, 0x49, 0x49, 0x36},
    /* 57 '9' */ {0x06, 0x49, 0x49, 0x29, 0x1E},
    /* 58 ':' */ {0x00, 0x36, 0x36, 0x00, 0x00},
    /* 59 ';' */ {0x00, 0x56, 0x36, 0x00, 0x00},
    /* 60 '<' */ {0x08, 0x14, 0x22, 0x41, 0x00},
    /* 61 '=' */ {0x14, 0x14, 0x14, 0x14, 0x14},
    /* 62 '>' */ {0x00, 0x41, 0x22, 0x14, 0x08},
    /* 63 '?' */ {0x02, 0x01, 0x51, 0x09, 0x06},
    /* 64 '@' */ {0x32, 0x49, 0x79, 0x41, 0x3E},
    /* 65 'A' */ {0x7E, 0x11, 0x11, 0x11, 0x7E},
    /* 66 'B' */ {0x7F, 0x49, 0x49, 0x49, 0x36},
    /* 67 'C' */ {0x3E, 0x41, 0x41, 0x41, 0x22},
    /* 68 'D' */ {0x7F, 0x41, 0x41, 0x22, 0x1C},
    /* 69 'E' */ {0x7F, 0x49, 0x49, 0x49, 0x41},
    /* 70 'F' */ {0x7F, 0x09, 0x09, 0x09, 0x01},
    /* 71 'G' */ {0x3E, 0x41, 0x49, 0x49, 0x7A},
    /* 72 'H' */ {0x7F, 0x08, 0x08, 0x08, 0x7F},
    /* 73 'I' */ {0x00, 0x41, 0x7F, 0x41, 0x00},
    /* 74 'J' */ {0x20, 0x40, 0x41, 0x3F, 0x01},
    /* 75 'K' */ {0x7F, 0x08, 0x14, 0x22, 0x41},
    /* 76 'L' */ {0x7F, 0x40, 0x40, 0x40, 0x40},
    /* 77 'M' */ {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    /* 78 'N' */ {0x7F, 0x04, 0x08, 0x10, 0x7F},
    /* 79 'O' */ {0x3E, 0x41, 0x41, 0x41, 0x3E},
    /* 80 'P' */ {0x7F, 0x09, 0x09, 0x09, 0x06},
    /* 81 'Q' */ {0x3E, 0x41, 0x51, 0x21, 0x5E},
    /* 82 'R' */ {0x7F, 0x09, 0x19, 0x29, 0x46},
    /* 83 'S' */ {0x46, 0x49, 0x49, 0x49, 0x31},
    /* 84 'T' */ {0x01, 0x01, 0x7F, 0x01, 0x01},
    /* 85 'U' */ {0x3F, 0x40, 0x40, 0x40, 0x3F},
    /* 86 'V' */ {0x1F, 0x20, 0x40, 0x20, 0x1F},
    /* 87 'W' */ {0x3F, 0x40, 0x38, 0x40, 0x3F},
    /* 88 'X' */ {0x63, 0x14, 0x08, 0x14, 0x63},
    /* 89 'Y' */ {0x07, 0x08, 0x70, 0x08, 0x07},
    /* 90 'Z' */ {0x61, 0x51, 0x49, 0x45, 0x43},
    /* 91 '[' */ {0x00, 0x7F, 0x41, 0x41, 0x00},
    /* 92 '\' */ {0x02, 0x04, 0x08, 0x10, 0x20},
    /* 93 ']' */ {0x00, 0x41, 0x41, 0x7F, 0x00},
    /* 94 '^' */ {0x04, 0x02, 0x01, 0x02, 0x04},
    /* 95 '_' */ {0x40, 0x40, 0x40, 0x40, 0x40},
    /* 96 '`' */ {0x00, 0x01, 0x02, 0x04, 0x00},
    /* 97 'a' */ {0x20, 0x54, 0x54, 0x54, 0x78},
    /* 98 'b' */ {0x7F, 0x48, 0x44, 0x44, 0x38},
    /* 99 'c' */ {0x38, 0x44, 0x44, 0x44, 0x20},
    /*100 'd' */ {0x38, 0x44, 0x44, 0x48, 0x7F},
    /*101 'e' */ {0x38, 0x54, 0x54, 0x54, 0x18},
    /*102 'f' */ {0x08, 0x7E, 0x09, 0x01, 0x02},
    /*103 'g' */ {0x0C, 0x52, 0x52, 0x52, 0x3E},
    /*104 'h' */ {0x7F, 0x08, 0x04, 0x04, 0x78},
    /*105 'i' */ {0x00, 0x44, 0x7D, 0x40, 0x00},
    /*106 'j' */ {0x20, 0x40, 0x44, 0x3D, 0x00},
    /*107 'k' */ {0x7F, 0x10, 0x28, 0x44, 0x00},
    /*108 'l' */ {0x00, 0x41, 0x7F, 0x40, 0x00},
    /*109 'm' */ {0x7C, 0x04, 0x18, 0x04, 0x78},
    /*110 'n' */ {0x7C, 0x08, 0x04, 0x04, 0x78},
    /*111 'o' */ {0x38, 0x44, 0x44, 0x44, 0x38},
    /*112 'p' */ {0x7C, 0x14, 0x14, 0x14, 0x08},
    /*113 'q' */ {0x08, 0x14, 0x14, 0x18, 0x7C},
    /*114 'r' */ {0x7C, 0x08, 0x04, 0x04, 0x08},
    /*115 's' */ {0x48, 0x54, 0x54, 0x54, 0x20},
    /*116 't' */ {0x04, 0x3F, 0x44, 0x40, 0x20},
    /*117 'u' */ {0x3C, 0x40, 0x40, 0x20, 0x7C},
    /*118 'v' */ {0x1C, 0x20, 0x40, 0x20, 0x1C},
    /*119 'w' */ {0x3C, 0x40, 0x30, 0x40, 0x3C},
    /*120 'x' */ {0x44, 0x28, 0x10, 0x28, 0x44},
    /*121 'y' */ {0x0C, 0x50, 0x50, 0x50, 0x3C},
    /*122 'z' */ {0x44, 0x64, 0x54, 0x4C, 0x44},
    /*123 '{' */ {0x00, 0x08, 0x36, 0x41, 0x00},
    /*124 '|' */ {0x00, 0x00, 0x7F, 0x00, 0x00},
    /*125 '}' */ {0x00, 0x41, 0x36, 0x08, 0x00},
    /*126 '~' */ {0x10, 0x08, 0x08, 0x10, 0x10},
};

/* ================================================================
 * Legacy init wrappers (delegate to driver modules)
 * ================================================================ */

void display_init(uint8_t i2c_addr) {
    disp_type = DISPLAY_SSD1306;
    extern void ssd1306_hw_init_direct(uint8_t addr);
    ssd1306_hw_init_direct(i2c_addr);
}

void display_init_sh1107(uint8_t spi_inst, uint8_t mosi, uint8_t sck,
                         uint8_t cs, uint8_t dc, uint8_t rst) {
    disp_type = DISPLAY_SH1107;
    extern void sh1107_hw_init_direct(uint8_t spi, uint8_t mosi,
                                       uint8_t sck, uint8_t cs,
                                       uint8_t dc, uint8_t rst);
    sh1107_hw_init_direct(spi_inst, mosi, sck, cs, dc, rst);
}

/* ================================================================
 * Framebuffer operations
 * ================================================================ */

void display_clear(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
}

void display_flush(void) {
    if (!disp_connected || !active_drv || !active_drv->hw_flush) return;
    active_drv->hw_flush(framebuffer, disp_width, disp_height);
}

/* ================================================================
 * Drawing primitives (work on framebuffer)
 * ================================================================ */

void display_pixel(int x, int y, bool on) {
    if (x < 0 || x >= disp_width || y < 0 || y >= disp_height) return;
    int page = y / 8;
    int bit  = y % 8;
    int idx  = page * disp_width + x;
    if (on)
        framebuffer[idx] |=  (uint8_t)(1 << bit);
    else
        framebuffer[idx] &= (uint8_t)~(1 << bit);
}

void display_line(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    int err = dx - dy;

    for (;;) {
        display_pixel(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void display_rect(int x, int y, int w, int h, bool fill) {
    if (fill) {
        for (int j = y; j < y + h; j++)
            for (int i = x; i < x + w; i++)
                display_pixel(i, j, true);
    } else {
        display_line(x, y, x + w - 1, y);
        display_line(x + w - 1, y, x + w - 1, y + h - 1);
        display_line(x + w - 1, y + h - 1, x, y + h - 1);
        display_line(x, y + h - 1, x, y);
    }
}

void display_circle(int cx, int cy, int r, bool fill) {
    int x = r, y = 0, err = 1 - r;

    while (x >= y) {
        if (fill) {
            display_line(cx - x, cy + y, cx + x, cy + y);
            display_line(cx - x, cy - y, cx + x, cy - y);
            display_line(cx - y, cy + x, cx + y, cy + x);
            display_line(cx - y, cy - x, cx + y, cy - x);
        } else {
            display_pixel(cx + x, cy + y, true);
            display_pixel(cx - x, cy + y, true);
            display_pixel(cx + x, cy - y, true);
            display_pixel(cx - x, cy - y, true);
            display_pixel(cx + y, cy + x, true);
            display_pixel(cx - y, cy + x, true);
            display_pixel(cx + y, cy - x, true);
            display_pixel(cx - y, cy - x, true);
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void display_text(int x, int y, const char *text) {
    int cx = x;
    while (*text) {
        char ch = *text++;
        if (ch < 32 || ch > 126) ch = '?';
        int idx = ch - 32;
        for (int col = 0; col < 5; col++) {
            uint8_t column = font5x7[idx][col];
            for (int row = 0; row < 7; row++) {
                if (column & (1 << row))
                    display_pixel(cx + col, y + row, true);
            }
        }
        cx += 6;
        if (cx + 5 > disp_width) break;
    }
}

void display_printf(int x, int y, const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    display_text(x, y, buf);
}

void display_bitmap(int x, int y, int w, int h, const uint8_t *data) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int byte_idx = (row * w + col) / 8;
            int bit_idx  = 7 - ((row * w + col) % 8);
            if (data[byte_idx] & (1 << bit_idx))
                display_pixel(x + col, y + row, true);
        }
    }
}

void display_scroll_up(int lines) {
    if (lines <= 0 || lines >= disp_height) {
        display_clear();
        return;
    }
    for (int y = 0; y < disp_height - lines; y++) {
        for (int x = 0; x < disp_width; x++) {
            int src_y = y + lines;
            int page = src_y / 8;
            int bit  = src_y % 8;
            int idx  = page * disp_width + x;
            bool px = (framebuffer[idx] >> bit) & 1;
            display_pixel(x, y, px);
        }
    }
    for (int y = disp_height - lines; y < disp_height; y++)
        for (int x = 0; x < disp_width; x++)
            display_pixel(x, y, false);
}

/* ================================================================
 * Control / query
 * ================================================================ */

bool display_is_connected(void) {
    if (active_drv && active_drv->hw_is_connected)
        return active_drv->hw_is_connected();
    return disp_connected;
}

display_type_t display_get_type(void) {
    return disp_type;
}

int display_get_width(void) {
    return disp_width;
}

int display_get_height(void) {
    return disp_height;
}

void display_set_contrast(uint8_t contrast) {
    if (active_drv && active_drv->hw_set_contrast)
        active_drv->hw_set_contrast(contrast);
}

void display_invert(bool invert) {
    disp_inverted = invert;
    if (active_drv && active_drv->hw_invert)
        active_drv->hw_invert(invert);
}
