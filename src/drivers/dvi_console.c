/* dvi_console.c - DVI Text Console for littleOS
 *
 * Renders an 80x30 text-mode console on the DVI framebuffer using
 * a compact 4x8 bitmap font. Supports basic ANSI escape sequences
 * for color, cursor movement, and screen clearing.
 *
 * The console hooks into the Pico SDK's stdio system so all printf()
 * output is automatically mirrored to the DVI display.
 */
#include "dvi_console.h"
#include "module.h"
#include "dmesg.h"

#include <stdio.h>
#include <string.h>

#if LITTLEOS_HAS_HSTX

#include "hal/hstx_dvi.h"
#include "pico/stdlib.h"
#include "pico/stdio/driver.h"

/* ================================================================
 * 4x8 Bitmap Font (ASCII 32-126)
 * ================================================================
 * Each character is 4 columns x 8 rows, stored as 8 bytes (one per row).
 * Bit 3 = leftmost pixel, bit 0 = rightmost pixel.
 */
static const uint8_t font4x8[][8] = {
    /* 32 ' ' */ {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0},
    /* 33 '!' */ {0x4,0x4,0x4,0x4,0x4,0x0,0x4,0x0},
    /* 34 '"' */ {0xA,0xA,0x0,0x0,0x0,0x0,0x0,0x0},
    /* 35 '#' */ {0xA,0xF,0xA,0xA,0xF,0xA,0x0,0x0},
    /* 36 '$' */ {0x4,0xF,0xC,0x6,0x3,0xF,0x4,0x0},
    /* 37 '%' */ {0x9,0x2,0x2,0x4,0x4,0x9,0x0,0x0},
    /* 38 '&' */ {0x4,0xA,0x4,0xB,0xA,0x5,0x0,0x0},
    /* 39 ''' */ {0x4,0x4,0x0,0x0,0x0,0x0,0x0,0x0},
    /* 40 '(' */ {0x2,0x4,0x4,0x4,0x4,0x4,0x2,0x0},
    /* 41 ')' */ {0x4,0x2,0x2,0x2,0x2,0x2,0x4,0x0},
    /* 42 '*' */ {0x0,0xA,0x4,0xE,0x4,0xA,0x0,0x0},
    /* 43 '+' */ {0x0,0x4,0x4,0xE,0x4,0x4,0x0,0x0},
    /* 44 ',' */ {0x0,0x0,0x0,0x0,0x0,0x4,0x4,0x8},
    /* 45 '-' */ {0x0,0x0,0x0,0xE,0x0,0x0,0x0,0x0},
    /* 46 '.' */ {0x0,0x0,0x0,0x0,0x0,0x0,0x4,0x0},
    /* 47 '/' */ {0x1,0x1,0x2,0x4,0x4,0x8,0x8,0x0},
    /* 48 '0' */ {0x6,0x9,0x9,0x9,0x9,0x6,0x0,0x0},
    /* 49 '1' */ {0x4,0xC,0x4,0x4,0x4,0xE,0x0,0x0},
    /* 50 '2' */ {0x6,0x9,0x1,0x6,0x8,0xF,0x0,0x0},
    /* 51 '3' */ {0xE,0x1,0x6,0x1,0x1,0xE,0x0,0x0},
    /* 52 '4' */ {0x2,0x6,0xA,0xF,0x2,0x2,0x0,0x0},
    /* 53 '5' */ {0xF,0x8,0xE,0x1,0x1,0xE,0x0,0x0},
    /* 54 '6' */ {0x6,0x8,0xE,0x9,0x9,0x6,0x0,0x0},
    /* 55 '7' */ {0xF,0x1,0x2,0x4,0x4,0x4,0x0,0x0},
    /* 56 '8' */ {0x6,0x9,0x6,0x9,0x9,0x6,0x0,0x0},
    /* 57 '9' */ {0x6,0x9,0x9,0x7,0x1,0x6,0x0,0x0},
    /* 58 ':' */ {0x0,0x0,0x4,0x0,0x0,0x4,0x0,0x0},
    /* 59 ';' */ {0x0,0x0,0x4,0x0,0x0,0x4,0x4,0x8},
    /* 60 '<' */ {0x1,0x2,0x4,0x8,0x4,0x2,0x1,0x0},
    /* 61 '=' */ {0x0,0x0,0xF,0x0,0xF,0x0,0x0,0x0},
    /* 62 '>' */ {0x8,0x4,0x2,0x1,0x2,0x4,0x8,0x0},
    /* 63 '?' */ {0x6,0x9,0x1,0x2,0x4,0x0,0x4,0x0},
    /* 64 '@' */ {0x6,0x9,0xB,0xB,0x8,0x6,0x0,0x0},
    /* 65 'A' */ {0x6,0x9,0x9,0xF,0x9,0x9,0x0,0x0},
    /* 66 'B' */ {0xE,0x9,0xE,0x9,0x9,0xE,0x0,0x0},
    /* 67 'C' */ {0x6,0x9,0x8,0x8,0x9,0x6,0x0,0x0},
    /* 68 'D' */ {0xE,0x9,0x9,0x9,0x9,0xE,0x0,0x0},
    /* 69 'E' */ {0xF,0x8,0xE,0x8,0x8,0xF,0x0,0x0},
    /* 70 'F' */ {0xF,0x8,0xE,0x8,0x8,0x8,0x0,0x0},
    /* 71 'G' */ {0x6,0x9,0x8,0xB,0x9,0x6,0x0,0x0},
    /* 72 'H' */ {0x9,0x9,0xF,0x9,0x9,0x9,0x0,0x0},
    /* 73 'I' */ {0xE,0x4,0x4,0x4,0x4,0xE,0x0,0x0},
    /* 74 'J' */ {0x7,0x2,0x2,0x2,0xA,0x4,0x0,0x0},
    /* 75 'K' */ {0x9,0xA,0xC,0xA,0x9,0x9,0x0,0x0},
    /* 76 'L' */ {0x8,0x8,0x8,0x8,0x8,0xF,0x0,0x0},
    /* 77 'M' */ {0x9,0xF,0xF,0x9,0x9,0x9,0x0,0x0},
    /* 78 'N' */ {0x9,0xD,0xF,0xB,0x9,0x9,0x0,0x0},
    /* 79 'O' */ {0x6,0x9,0x9,0x9,0x9,0x6,0x0,0x0},
    /* 80 'P' */ {0xE,0x9,0x9,0xE,0x8,0x8,0x0,0x0},
    /* 81 'Q' */ {0x6,0x9,0x9,0x9,0xA,0x5,0x0,0x0},
    /* 82 'R' */ {0xE,0x9,0x9,0xE,0xA,0x9,0x0,0x0},
    /* 83 'S' */ {0x6,0x8,0x6,0x1,0x9,0x6,0x0,0x0},
    /* 84 'T' */ {0xE,0x4,0x4,0x4,0x4,0x4,0x0,0x0},
    /* 85 'U' */ {0x9,0x9,0x9,0x9,0x9,0x6,0x0,0x0},
    /* 86 'V' */ {0x9,0x9,0x9,0x9,0x6,0x6,0x0,0x0},
    /* 87 'W' */ {0x9,0x9,0x9,0xF,0xF,0x9,0x0,0x0},
    /* 88 'X' */ {0x9,0x9,0x6,0x6,0x9,0x9,0x0,0x0},
    /* 89 'Y' */ {0xA,0xA,0x4,0x4,0x4,0x4,0x0,0x0},
    /* 90 'Z' */ {0xF,0x1,0x2,0x4,0x8,0xF,0x0,0x0},
    /* 91 '[' */ {0x6,0x4,0x4,0x4,0x4,0x4,0x6,0x0},
    /* 92 '\' */ {0x8,0x8,0x4,0x2,0x2,0x1,0x1,0x0},
    /* 93 ']' */ {0x6,0x2,0x2,0x2,0x2,0x2,0x6,0x0},
    /* 94 '^' */ {0x4,0xA,0x0,0x0,0x0,0x0,0x0,0x0},
    /* 95 '_' */ {0x0,0x0,0x0,0x0,0x0,0x0,0xF,0x0},
    /* 96 '`' */ {0x4,0x2,0x0,0x0,0x0,0x0,0x0,0x0},
    /* 97 'a' */ {0x0,0x0,0x6,0x1,0x7,0x9,0x7,0x0},
    /* 98 'b' */ {0x8,0x8,0xE,0x9,0x9,0x9,0xE,0x0},
    /* 99 'c' */ {0x0,0x0,0x7,0x8,0x8,0x8,0x7,0x0},
    /*100 'd' */ {0x1,0x1,0x7,0x9,0x9,0x9,0x7,0x0},
    /*101 'e' */ {0x0,0x0,0x6,0x9,0xF,0x8,0x6,0x0},
    /*102 'f' */ {0x3,0x4,0xE,0x4,0x4,0x4,0x4,0x0},
    /*103 'g' */ {0x0,0x0,0x7,0x9,0x9,0x7,0x1,0x6},
    /*104 'h' */ {0x8,0x8,0xE,0x9,0x9,0x9,0x9,0x0},
    /*105 'i' */ {0x4,0x0,0xC,0x4,0x4,0x4,0xE,0x0},
    /*106 'j' */ {0x2,0x0,0x2,0x2,0x2,0x2,0xA,0x4},
    /*107 'k' */ {0x8,0x8,0x9,0xA,0xC,0xA,0x9,0x0},
    /*108 'l' */ {0xC,0x4,0x4,0x4,0x4,0x4,0xE,0x0},
    /*109 'm' */ {0x0,0x0,0xA,0xF,0xF,0x9,0x9,0x0},
    /*110 'n' */ {0x0,0x0,0xE,0x9,0x9,0x9,0x9,0x0},
    /*111 'o' */ {0x0,0x0,0x6,0x9,0x9,0x9,0x6,0x0},
    /*112 'p' */ {0x0,0x0,0xE,0x9,0x9,0xE,0x8,0x8},
    /*113 'q' */ {0x0,0x0,0x7,0x9,0x9,0x7,0x1,0x1},
    /*114 'r' */ {0x0,0x0,0xB,0xC,0x8,0x8,0x8,0x0},
    /*115 's' */ {0x0,0x0,0x7,0x8,0x6,0x1,0xE,0x0},
    /*116 't' */ {0x4,0x4,0xE,0x4,0x4,0x4,0x3,0x0},
    /*117 'u' */ {0x0,0x0,0x9,0x9,0x9,0x9,0x7,0x0},
    /*118 'v' */ {0x0,0x0,0x9,0x9,0x9,0x6,0x6,0x0},
    /*119 'w' */ {0x0,0x0,0x9,0x9,0xF,0xF,0x6,0x0},
    /*120 'x' */ {0x0,0x0,0x9,0x6,0x6,0x9,0x0,0x0},
    /*121 'y' */ {0x0,0x0,0x9,0x9,0x9,0x7,0x1,0x6},
    /*122 'z' */ {0x0,0x0,0xF,0x2,0x4,0x8,0xF,0x0},
    /*123 '{' */ {0x2,0x4,0x4,0x8,0x4,0x4,0x2,0x0},
    /*124 '|' */ {0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x0},
    /*125 '}' */ {0x4,0x2,0x2,0x1,0x2,0x2,0x4,0x0},
    /*126 '~' */ {0x0,0x5,0xA,0x0,0x0,0x0,0x0,0x0},
};

/* ================================================================
 * RGB332 Color Palette (ANSI 8-color → RGB332)
 * ================================================================ */

/* Standard ANSI colors mapped to RGB332 (RRRGGGBB) */
static const uint8_t ansi_fg_colors[8] = {
    0x00,   /* 0: Black */
    0xE0,   /* 1: Red      (111_000_00) */
    0x1C,   /* 2: Green    (000_111_00) */
    0xFC,   /* 3: Yellow   (111_111_00) */
    0x03,   /* 4: Blue     (000_000_11) */
    0xE3,   /* 5: Magenta  (111_000_11) */
    0x1F,   /* 6: Cyan     (000_111_11) */
    0xFF,   /* 7: White    (111_111_11) */
};

/* Bright ANSI colors */
static const uint8_t ansi_fg_bright[8] = {
    0x49,   /* 0: Bright Black (dark gray) */
    0xE0,   /* 1: Bright Red */
    0x1C,   /* 2: Bright Green */
    0xFC,   /* 3: Bright Yellow */
    0x03,   /* 4: Bright Blue */
    0xE3,   /* 5: Bright Magenta */
    0x1F,   /* 6: Bright Cyan */
    0xFF,   /* 7: Bright White */
};

/* ================================================================
 * Console State
 * ================================================================ */

#define FB_W 320
#define FB_H 240

/* Character cell: stores character + attributes */
typedef struct {
    char    ch;
    uint8_t fg;     /* RGB332 foreground */
    uint8_t bg;     /* RGB332 background */
} cell_t;

static struct {
    bool        active;
    uint8_t     *framebuffer;   /* Points to DVI framebuffer (320*240 bytes) */

    /* Text grid */
    cell_t      cells[DVI_CONSOLE_ROWS][DVI_CONSOLE_COLS];
    int         cursor_x;       /* Column (0..79) */
    int         cursor_y;       /* Row (0..29) */

    /* Current text attributes */
    uint8_t     fg_color;       /* Current foreground RGB332 */
    uint8_t     bg_color;       /* Current background RGB332 */

    /* ANSI escape parser state */
    enum {
        PARSE_NORMAL,
        PARSE_ESC,              /* Got ESC (0x1B) */
        PARSE_CSI,              /* Got ESC [ */
    } parse_state;
    int         csi_params[8];  /* CSI parameter accumulator */
    int         csi_param_count;
    int         csi_current;    /* Current parameter being built */

    bool        dirty;          /* Framebuffer needs redraw */
} s_con;

/* ================================================================
 * Framebuffer Rendering
 * ================================================================ */

/* Render a single character cell to the framebuffer */
static void render_cell(int col, int row) {
    if (!s_con.framebuffer) return;

    cell_t *cell = &s_con.cells[row][col];
    int px = col * DVI_CONSOLE_FONT_W;
    int py = row * DVI_CONSOLE_FONT_H;

    char ch = cell->ch;
    if (ch < 32 || ch > 126) ch = ' ';
    const uint8_t *glyph = font4x8[ch - 32];

    for (int y = 0; y < DVI_CONSOLE_FONT_H; y++) {
        uint8_t bits = glyph[y];
        int fb_y = py + y;
        if (fb_y >= FB_H) break;

        for (int x = 0; x < DVI_CONSOLE_FONT_W; x++) {
            int fb_x = px + x;
            if (fb_x >= FB_W) break;

            bool on = (bits >> (3 - x)) & 1;
            s_con.framebuffer[fb_y * FB_W + fb_x] = on ? cell->fg : cell->bg;
        }
    }
}

/* Render the cursor (inverse block at cursor position) */
static void render_cursor(void) {
    if (!s_con.framebuffer) return;
    if (s_con.cursor_x >= DVI_CONSOLE_COLS || s_con.cursor_y >= DVI_CONSOLE_ROWS) return;

    int px = s_con.cursor_x * DVI_CONSOLE_FONT_W;
    int py = s_con.cursor_y * DVI_CONSOLE_FONT_H;

    /* Draw a solid block cursor in the fg color */
    for (int y = 0; y < DVI_CONSOLE_FONT_H; y++) {
        int fb_y = py + y;
        if (fb_y >= FB_H) break;
        for (int x = 0; x < DVI_CONSOLE_FONT_W; x++) {
            int fb_x = px + x;
            if (fb_x >= FB_W) break;
            s_con.framebuffer[fb_y * FB_W + fb_x] = s_con.fg_color;
        }
    }
}

/* Full redraw of the entire screen */
static void render_all(void) {
    if (!s_con.framebuffer) return;

    for (int row = 0; row < DVI_CONSOLE_ROWS; row++)
        for (int col = 0; col < DVI_CONSOLE_COLS; col++)
            render_cell(col, row);

    render_cursor();
    s_con.dirty = false;
}

/* ================================================================
 * Text Operations
 * ================================================================ */

static void scroll_up(void) {
    /* Move rows 1..N-1 to 0..N-2 */
    memmove(&s_con.cells[0], &s_con.cells[1],
            sizeof(cell_t) * DVI_CONSOLE_COLS * (DVI_CONSOLE_ROWS - 1));

    /* Clear the last row */
    for (int col = 0; col < DVI_CONSOLE_COLS; col++) {
        s_con.cells[DVI_CONSOLE_ROWS - 1][col].ch = ' ';
        s_con.cells[DVI_CONSOLE_ROWS - 1][col].fg = s_con.fg_color;
        s_con.cells[DVI_CONSOLE_ROWS - 1][col].bg = s_con.bg_color;
    }

    s_con.dirty = true;
}

static void newline(void) {
    s_con.cursor_x = 0;
    s_con.cursor_y++;
    if (s_con.cursor_y >= DVI_CONSOLE_ROWS) {
        s_con.cursor_y = DVI_CONSOLE_ROWS - 1;
        scroll_up();
    }
}

static void put_char_at(char ch) {
    if (s_con.cursor_x >= DVI_CONSOLE_COLS) {
        newline();
    }

    cell_t *cell = &s_con.cells[s_con.cursor_y][s_con.cursor_x];
    cell->ch = ch;
    cell->fg = s_con.fg_color;
    cell->bg = s_con.bg_color;

    render_cell(s_con.cursor_x, s_con.cursor_y);
    s_con.cursor_x++;
}

/* ================================================================
 * ANSI Escape Sequence Parser
 * ================================================================ */

static void csi_reset(void) {
    s_con.csi_param_count = 0;
    s_con.csi_current = 0;
    memset(s_con.csi_params, 0, sizeof(s_con.csi_params));
}

static void csi_push_param(void) {
    if (s_con.csi_param_count < 8) {
        s_con.csi_params[s_con.csi_param_count++] = s_con.csi_current;
        s_con.csi_current = 0;
    }
}

static int csi_param(int idx, int def) {
    if (idx < s_con.csi_param_count)
        return s_con.csi_params[idx] ? s_con.csi_params[idx] : def;
    return def;
}

static void handle_sgr(void) {
    /* SGR - Select Graphic Rendition */
    if (s_con.csi_param_count == 0) {
        /* ESC[m = reset */
        s_con.fg_color = ansi_fg_colors[7];  /* white */
        s_con.bg_color = ansi_fg_colors[0];  /* black */
        return;
    }

    for (int i = 0; i < s_con.csi_param_count; i++) {
        int p = s_con.csi_params[i];
        if (p == 0) {
            s_con.fg_color = ansi_fg_colors[7];
            s_con.bg_color = ansi_fg_colors[0];
        } else if (p == 1) {
            /* Bold — use bright colors (approximation) */
        } else if (p >= 30 && p <= 37) {
            s_con.fg_color = ansi_fg_colors[p - 30];
        } else if (p >= 40 && p <= 47) {
            s_con.bg_color = ansi_fg_colors[p - 40];
        } else if (p >= 90 && p <= 97) {
            s_con.fg_color = ansi_fg_bright[p - 90];
        } else if (p >= 100 && p <= 107) {
            s_con.bg_color = ansi_fg_bright[p - 100];
        } else if (p == 39) {
            s_con.fg_color = ansi_fg_colors[7];  /* default fg */
        } else if (p == 49) {
            s_con.bg_color = ansi_fg_colors[0];  /* default bg */
        }
    }
}

static void handle_csi(char final) {
    switch (final) {
    case 'A': { /* Cursor Up */
        int n = csi_param(0, 1);
        s_con.cursor_y -= n;
        if (s_con.cursor_y < 0) s_con.cursor_y = 0;
        break;
    }
    case 'B': { /* Cursor Down */
        int n = csi_param(0, 1);
        s_con.cursor_y += n;
        if (s_con.cursor_y >= DVI_CONSOLE_ROWS) s_con.cursor_y = DVI_CONSOLE_ROWS - 1;
        break;
    }
    case 'C': { /* Cursor Forward */
        int n = csi_param(0, 1);
        s_con.cursor_x += n;
        if (s_con.cursor_x >= DVI_CONSOLE_COLS) s_con.cursor_x = DVI_CONSOLE_COLS - 1;
        break;
    }
    case 'D': { /* Cursor Back */
        int n = csi_param(0, 1);
        s_con.cursor_x -= n;
        if (s_con.cursor_x < 0) s_con.cursor_x = 0;
        break;
    }
    case 'H':
    case 'f': { /* Cursor Position */
        int row = csi_param(0, 1) - 1;
        int col = csi_param(1, 1) - 1;
        if (row < 0) row = 0;
        if (row >= DVI_CONSOLE_ROWS) row = DVI_CONSOLE_ROWS - 1;
        if (col < 0) col = 0;
        if (col >= DVI_CONSOLE_COLS) col = DVI_CONSOLE_COLS - 1;
        s_con.cursor_y = row;
        s_con.cursor_x = col;
        break;
    }
    case 'J': { /* Erase in Display */
        int mode = csi_param(0, 0);
        if (mode == 2) {
            /* Clear entire screen */
            for (int r = 0; r < DVI_CONSOLE_ROWS; r++)
                for (int c = 0; c < DVI_CONSOLE_COLS; c++) {
                    s_con.cells[r][c].ch = ' ';
                    s_con.cells[r][c].fg = s_con.fg_color;
                    s_con.cells[r][c].bg = s_con.bg_color;
                }
            s_con.dirty = true;
        } else if (mode == 0) {
            /* Clear from cursor to end */
            for (int c = s_con.cursor_x; c < DVI_CONSOLE_COLS; c++) {
                s_con.cells[s_con.cursor_y][c].ch = ' ';
                s_con.cells[s_con.cursor_y][c].fg = s_con.fg_color;
                s_con.cells[s_con.cursor_y][c].bg = s_con.bg_color;
            }
            for (int r = s_con.cursor_y + 1; r < DVI_CONSOLE_ROWS; r++)
                for (int c = 0; c < DVI_CONSOLE_COLS; c++) {
                    s_con.cells[r][c].ch = ' ';
                    s_con.cells[r][c].fg = s_con.fg_color;
                    s_con.cells[r][c].bg = s_con.bg_color;
                }
            s_con.dirty = true;
        }
        break;
    }
    case 'K': { /* Erase in Line */
        int mode = csi_param(0, 0);
        int start = 0, end = DVI_CONSOLE_COLS;
        if (mode == 0) start = s_con.cursor_x;
        else if (mode == 1) end = s_con.cursor_x + 1;
        for (int c = start; c < end; c++) {
            s_con.cells[s_con.cursor_y][c].ch = ' ';
            s_con.cells[s_con.cursor_y][c].fg = s_con.fg_color;
            s_con.cells[s_con.cursor_y][c].bg = s_con.bg_color;
            render_cell(c, s_con.cursor_y);
        }
        break;
    }
    case 'm': /* SGR - colors */
        handle_sgr();
        break;
    default:
        break;
    }
}

/* ================================================================
 * Character Processing
 * ================================================================ */

static void process_char(char c) {
    switch (s_con.parse_state) {
    case PARSE_NORMAL:
        if (c == '\033') {
            s_con.parse_state = PARSE_ESC;
        } else if (c == '\r') {
            /* Erase cursor at old position */
            render_cell(s_con.cursor_x, s_con.cursor_y);
            s_con.cursor_x = 0;
        } else if (c == '\n') {
            render_cell(s_con.cursor_x, s_con.cursor_y);
            newline();
            if (s_con.dirty) render_all();
        } else if (c == '\b') {
            if (s_con.cursor_x > 0) {
                render_cell(s_con.cursor_x, s_con.cursor_y);
                s_con.cursor_x--;
            }
        } else if (c == '\t') {
            int spaces = 4 - (s_con.cursor_x % 4);
            for (int i = 0; i < spaces; i++)
                put_char_at(' ');
        } else if ((unsigned char)c >= 32) {
            render_cell(s_con.cursor_x, s_con.cursor_y);
            put_char_at(c);
        }
        /* Render cursor at new position */
        render_cursor();
        break;

    case PARSE_ESC:
        if (c == '[') {
            s_con.parse_state = PARSE_CSI;
            csi_reset();
        } else {
            s_con.parse_state = PARSE_NORMAL;
        }
        break;

    case PARSE_CSI:
        if (c >= '0' && c <= '9') {
            s_con.csi_current = s_con.csi_current * 10 + (c - '0');
        } else if (c == ';') {
            csi_push_param();
        } else if (c >= 0x40 && c <= 0x7E) {
            /* Final byte — execute the command */
            csi_push_param();
            handle_csi(c);
            s_con.parse_state = PARSE_NORMAL;
            if (s_con.dirty) render_all();
            else render_cursor();
        } else {
            /* Unknown, reset */
            s_con.parse_state = PARSE_NORMAL;
        }
        break;
    }
}

/* ================================================================
 * Pico SDK stdio driver integration
 * ================================================================ */

static void dvi_stdio_out_chars(const char *buf, int len) {
    if (!s_con.active) return;
    for (int i = 0; i < len; i++)
        process_char(buf[i]);
}

static stdio_driver_t dvi_stdio_driver = {
    .out_chars = dvi_stdio_out_chars,
    .in_chars  = NULL,      /* No input from DVI — that comes from USB keyboard */
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = false,  /* Shell already sends \r\n */
#endif
};

/* ================================================================
 * Public API
 * ================================================================ */

static uint8_t s_dvi_fb[FB_W * FB_H] __attribute__((aligned(4)));

int dvi_console_init(void) {
    if (s_con.active) return 0;

    memset(&s_con, 0, sizeof(s_con));
    s_con.fg_color = ansi_fg_colors[7];  /* white */
    s_con.bg_color = ansi_fg_colors[0];  /* black */
    s_con.framebuffer = s_dvi_fb;

    /* Clear cell grid */
    for (int r = 0; r < DVI_CONSOLE_ROWS; r++)
        for (int c = 0; c < DVI_CONSOLE_COLS; c++) {
            s_con.cells[r][c].ch = ' ';
            s_con.cells[r][c].fg = s_con.fg_color;
            s_con.cells[r][c].bg = s_con.bg_color;
        }

    /* Initialize DVI hardware */
    hstx_dvi_stop();  /* in case it was running */

    int ret = hstx_dvi_init(DVI_MODE_320x240_60HZ, DVI_PIXEL_RGB332);
    if (ret < 0) {
        dmesg_warn("dvi_console: failed to init DVI");
        return -1;
    }

    memset(s_dvi_fb, 0, sizeof(s_dvi_fb));

    ret = hstx_dvi_set_framebuffer(s_dvi_fb, sizeof(s_dvi_fb));
    if (ret < 0) {
        hstx_dvi_stop();
        return -1;
    }

    ret = hstx_dvi_start();
    if (ret < 0) {
        hstx_dvi_stop();
        return -1;
    }

    s_con.active = true;
    render_all();

    dmesg_info("DVI console active (80x30, 320x240 RGB332)");
    return 0;
}

void dvi_console_deinit(void) {
    if (!s_con.active) return;
    dvi_console_remove_hook();
    hstx_dvi_stop();
    s_con.active = false;
}

void dvi_console_write(const char *data, size_t len) {
    if (!s_con.active) return;
    for (size_t i = 0; i < len; i++)
        process_char(data[i]);
}

void dvi_console_putchar(char c) {
    if (!s_con.active) return;
    process_char(c);
}

void dvi_console_clear(void) {
    if (!s_con.active) return;
    s_con.cursor_x = 0;
    s_con.cursor_y = 0;
    for (int r = 0; r < DVI_CONSOLE_ROWS; r++)
        for (int c = 0; c < DVI_CONSOLE_COLS; c++) {
            s_con.cells[r][c].ch = ' ';
            s_con.cells[r][c].fg = s_con.fg_color;
            s_con.cells[r][c].bg = s_con.bg_color;
        }
    render_all();
}

bool dvi_console_is_active(void) {
    return s_con.active;
}

void dvi_console_install_hook(void) {
    stdio_set_driver_enabled(&dvi_stdio_driver, true);
}

void dvi_console_remove_hook(void) {
    stdio_set_driver_enabled(&dvi_stdio_driver, false);
}

/* ================================================================
 * Kernel Module Interface
 * ================================================================ */

static int dvi_console_mod_init(module_t *mod) {
    (void)mod;
    int ret = dvi_console_init();
    if (ret == 0)
        dvi_console_install_hook();
    return ret;
}

static void dvi_console_mod_deinit(module_t *mod) {
    (void)mod;
    dvi_console_deinit();
}

static void dvi_console_mod_status(module_t *mod) {
    (void)mod;
    printf("DVI Console: %s\r\n", s_con.active ? "active" : "inactive");
    printf("  Resolution:  320x240 RGB332\r\n");
    printf("  Text grid:   %dx%d\r\n", DVI_CONSOLE_COLS, DVI_CONSOLE_ROWS);
    printf("  Cursor:      (%d, %d)\r\n", s_con.cursor_x, s_con.cursor_y);
    printf("  GPIO:        12-19 (HSTX)\r\n");
}

static const module_ops_t dvi_console_mod_ops = {
    .init   = dvi_console_mod_init,
    .deinit = dvi_console_mod_deinit,
    .status = dvi_console_mod_status,
};

static module_t dvi_console_module = {
    .name        = "dvi_console",
    .description = "DVI TTY console (80x30 text mode via HSTX)",
    .version     = "1.0.0",
    .type        = MODULE_TYPE_DRIVER,
    .ops         = &dvi_console_mod_ops,
};

void dvi_console_module_register(void) {
    module_register(&dvi_console_module);
}

#else /* !LITTLEOS_HAS_HSTX */

/* Stubs for non-RP2350 builds */
int  dvi_console_init(void) { return -1; }
void dvi_console_deinit(void) {}
void dvi_console_write(const char *data, size_t len) { (void)data; (void)len; }
void dvi_console_putchar(char c) { (void)c; }
void dvi_console_clear(void) {}
bool dvi_console_is_active(void) { return false; }
void dvi_console_install_hook(void) {}
void dvi_console_remove_hook(void) {}
void dvi_console_module_register(void) {}

#endif /* LITTLEOS_HAS_HSTX */
