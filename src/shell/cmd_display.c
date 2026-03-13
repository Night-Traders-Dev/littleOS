/* cmd_display.c - OLED display shell commands for littleOS
 *
 * Supports SSD1306 (128x64 I2C) and SH1107 (64x128 SPI).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "display.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#endif

#include "board/board_config.h"

static bool disp_initialized = false;

static const char *disp_type_str(display_type_t t) {
    switch (t) {
        case DISPLAY_SSD1306: return "SSD1306";
        case DISPLAY_SH1107:  return "SH1107";
        default:              return "none";
    }
}

static const char *disp_iface_str(display_type_t t) {
    switch (t) {
        case DISPLAY_SSD1306: return "I2C";
        case DISPLAY_SH1107:  return "SPI";
        default:              return "?";
    }
}

int cmd_display(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: display <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  init [ADDR]          - Init SSD1306 I2C (default 0x3C)\r\n");
        printf("  init sh1107 [spi mosi sck cs dc rst]\r\n");
        printf("                       - Init SH1107 SPI (Waveshare 1.3\")\r\n");
        printf("  status               - Show display info\r\n");
        printf("  clear                - Clear screen\r\n");
        printf("  text X Y TEXT        - Draw text at position\r\n");
        printf("  pixel X Y            - Set pixel\r\n");
        printf("  line X0 Y0 X1 Y1     - Draw line\r\n");
        printf("  rect X Y W H [fill]  - Draw rectangle\r\n");
        printf("  circle CX CY R [fill]- Draw circle\r\n");
        printf("  invert               - Toggle invert\r\n");
        printf("  contrast N           - Set contrast (0-255)\r\n");
        printf("  demo                 - Draw demo pattern\r\n");
        printf("  sysinfo              - Show system info on display\r\n");
        return 0;
    }

    /* ---- init ---- */
    if (strcmp(argv[1], "init") == 0) {
        if (argc >= 3 && strcmp(argv[2], "sh1107") == 0) {
            /* SH1107 SPI init */
            uint8_t spi  = 0xFF, mosi = 0xFF, sck = 0xFF;
            uint8_t cs   = 0xFF, dc   = 0xFF, rst = 0xFF;
            if (argc >= 9) {
                spi  = (uint8_t)atoi(argv[3]);
                mosi = (uint8_t)atoi(argv[4]);
                sck  = (uint8_t)atoi(argv[5]);
                cs   = (uint8_t)atoi(argv[6]);
                dc   = (uint8_t)atoi(argv[7]);
                rst  = (uint8_t)atoi(argv[8]);
            }
            printf("Initializing SH1107 (64x128 SPI)...\r\n");
            display_init_sh1107(spi, mosi, sck, cs, dc, rst);
            disp_initialized = true;
            display_clear();
            display_flush();
            printf("SH1107 ready (%dx%d)\r\n",
                   display_get_width(), display_get_height());
            return 0;
        }

        /* SSD1306 I2C init (default) */
        uint8_t addr = 0x3C;
        if (argc >= 3)
            addr = (uint8_t)strtoul(argv[2], NULL, 16);
        printf("Initializing SSD1306 (128x64 I2C 0x%02X)...\r\n", addr);
        display_init(addr);
        disp_initialized = true;
        display_clear();
        display_flush();
        printf("SSD1306 ready (%dx%d)\r\n",
               display_get_width(), display_get_height());
        return 0;
    }

    /* ---- status (no init required) ---- */
    if (strcmp(argv[1], "status") == 0) {
        if (!disp_initialized) {
            printf("Display: not initialized\r\n");
            printf("Use 'display init' (SSD1306) or 'display init sh1107'\r\n");
            return 0;
        }
        display_type_t t = display_get_type();
        printf("Display: %s (%dx%d %s)\r\n",
               disp_type_str(t),
               display_get_width(), display_get_height(),
               disp_iface_str(t));
        printf("Connected: %s\r\n", display_is_connected() ? "yes" : "no");
        return 0;
    }

    if (!disp_initialized) {
        printf("Display not initialized.\r\n");
        printf("  display init [addr]     - SSD1306 I2C\r\n");
        printf("  display init sh1107     - SH1107 SPI\r\n");
        return 1;
    }

    /* ---- clear ---- */
    if (strcmp(argv[1], "clear") == 0) {
        display_clear();
        display_flush();
        printf("Display cleared\r\n");
        return 0;
    }

    /* ---- text ---- */
    if (strcmp(argv[1], "text") == 0) {
        if (argc < 5) { printf("Usage: display text <x> <y> <text...>\r\n"); return 1; }
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        char textbuf[128];
        textbuf[0] = '\0';
        for (int i = 4; i < argc; i++) {
            if (i > 4) strncat(textbuf, " ", sizeof(textbuf) - strlen(textbuf) - 1);
            strncat(textbuf, argv[i], sizeof(textbuf) - strlen(textbuf) - 1);
        }
        display_text(x, y, textbuf);
        display_flush();
        printf("Text drawn at (%d,%d)\r\n", x, y);
        return 0;
    }

    /* ---- pixel ---- */
    if (strcmp(argv[1], "pixel") == 0) {
        if (argc < 4) { printf("Usage: display pixel <x> <y>\r\n"); return 1; }
        display_pixel(atoi(argv[2]), atoi(argv[3]), true);
        display_flush();
        return 0;
    }

    /* ---- line ---- */
    if (strcmp(argv[1], "line") == 0) {
        if (argc < 6) { printf("Usage: display line <x0> <y0> <x1> <y1>\r\n"); return 1; }
        display_line(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        display_flush();
        printf("Line drawn\r\n");
        return 0;
    }

    /* ---- rect ---- */
    if (strcmp(argv[1], "rect") == 0) {
        if (argc < 6) { printf("Usage: display rect <x> <y> <w> <h> [fill]\r\n"); return 1; }
        bool fill = (argc >= 7 && strcmp(argv[6], "fill") == 0);
        display_rect(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), fill);
        display_flush();
        printf("Rect drawn\r\n");
        return 0;
    }

    /* ---- circle ---- */
    if (strcmp(argv[1], "circle") == 0) {
        if (argc < 5) { printf("Usage: display circle <cx> <cy> <r> [fill]\r\n"); return 1; }
        bool fill = (argc >= 6 && strcmp(argv[5], "fill") == 0);
        display_circle(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), fill);
        display_flush();
        printf("Circle drawn\r\n");
        return 0;
    }

    /* ---- invert ---- */
    if (strcmp(argv[1], "invert") == 0) {
        static bool inv_state = false;
        inv_state = !inv_state;
        display_invert(inv_state);
        printf("Display %s\r\n", inv_state ? "inverted" : "normal");
        return 0;
    }

    /* ---- contrast ---- */
    if (strcmp(argv[1], "contrast") == 0) {
        if (argc < 3) { printf("Usage: display contrast <0-255>\r\n"); return 1; }
        int c = atoi(argv[2]);
        if (c < 0) c = 0;
        if (c > 255) c = 255;
        display_set_contrast((uint8_t)c);
        printf("Contrast: %d/255\r\n", c);
        return 0;
    }

    /* ---- demo ---- */
    if (strcmp(argv[1], "demo") == 0) {
        int w = display_get_width();
        int h = display_get_height();
        display_clear();
        /* Border */
        display_rect(0, 0, w, h, false);
        /* Title */
        display_text(4, 4, "littleOS");
        /* Divider line */
        display_line(0, 14, w - 1, 14);
        /* Shapes */
        display_circle(w / 4, h / 2, 10, false);
        display_rect(w / 2, h / 2 - 8, 16, 16, true);
        display_line(w * 3 / 4 - 8, h / 2 + 8,
                     w * 3 / 4 + 8, h / 2 - 8);
        /* Footer */
        display_printf(4, h - 10, "%s %dx%d",
                       disp_type_str(display_get_type()), w, h);
        display_flush();
        printf("Demo pattern drawn (%dx%d)\r\n", w, h);
        return 0;
    }

    /* ---- sysinfo ---- */
    if (strcmp(argv[1], "sysinfo") == 0) {
        int w = display_get_width();
        int h = display_get_height();
        display_clear();
        display_text(0, 0, "littleOS");
        display_line(0, 9, w - 1, 9);

        char buf[22];
        snprintf(buf, sizeof(buf), "CPU: %s", CHIP_MODEL_STR);
        display_text(0, 12, buf);

        snprintf(buf, sizeof(buf), "RAM: %uKB",
                 (unsigned)(CHIP_RAM_SIZE / 1024));
        display_text(0, 22, buf);

#ifdef PICO_BUILD
        snprintf(buf, sizeof(buf), "CLK: %luMHz",
                 (unsigned long)(clock_get_hz(clk_sys) / 1000000));
#else
        snprintf(buf, sizeof(buf), "CLK: ???");
#endif
        display_text(0, 32, buf);

        snprintf(buf, sizeof(buf), "%s %s",
                 disp_type_str(display_get_type()),
                 disp_iface_str(display_get_type()));
        display_text(0, 42, buf);

        snprintf(buf, sizeof(buf), "%dx%d OLED", w, h);
        display_text(0, 52, buf);

        /* Use remaining vertical space on tall displays */
        if (h > 64) {
            snprintf(buf, sizeof(buf), "Flash: %uMB",
                     (unsigned)(CHIP_FLASH_SIZE / (1024 * 1024)));
            display_text(0, 64, buf);
            display_text(0, 76, BOARD_NAME);
        }

        display_flush();
        printf("System info displayed\r\n");
        return 0;
    }

    printf("Unknown command: %s\r\n", argv[1]);
    return 1;
}
