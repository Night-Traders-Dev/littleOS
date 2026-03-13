/* cmd_display.c - SSD1306 OLED display shell commands for littleOS */
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

int cmd_display(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: display <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  init [ADDR]          - Initialize display (default 0x3C)\r\n");
        printf("  clear                - Clear screen\r\n");
        printf("  text X Y TEXT        - Draw text at position\r\n");
        printf("  pixel X Y            - Set pixel\r\n");
        printf("  line X0 Y0 X1 Y1     - Draw line\r\n");
        printf("  rect X Y W H [fill]  - Draw rectangle\r\n");
        printf("  invert               - Toggle invert\r\n");
        printf("  contrast N           - Set contrast (0-255)\r\n");
        printf("  demo                 - Draw demo pattern\r\n");
        printf("  sysinfo              - Show system info on display\r\n");
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) {
        uint8_t addr = 0x3C;
        if (argc >= 3) {
            addr = (uint8_t)strtoul(argv[2], NULL, 16);
        }
        display_init(addr);
        disp_initialized = true;
        printf("Display initialized at I2C address 0x%02X\r\n", addr);
        display_clear();
        display_flush();
        return 0;
    }

    if (!disp_initialized) {
        printf("Display not initialized. Use: display init [addr]\r\n");
        return 1;
    }

    if (strcmp(argv[1], "clear") == 0) {
        display_clear();
        display_flush();
        printf("Display cleared\r\n");
        return 0;
    }

    if (strcmp(argv[1], "text") == 0) {
        if (argc < 5) { printf("Usage: display text <x> <y> <text...>\r\n"); return 1; }
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        /* Concatenate remaining args as text */
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

    if (strcmp(argv[1], "pixel") == 0) {
        if (argc < 4) { printf("Usage: display pixel <x> <y>\r\n"); return 1; }
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        display_pixel(x, y, true);
        display_flush();
        printf("Pixel set at (%d,%d)\r\n", x, y);
        return 0;
    }

    if (strcmp(argv[1], "line") == 0) {
        if (argc < 6) { printf("Usage: display line <x0> <y0> <x1> <y1>\r\n"); return 1; }
        int x0 = atoi(argv[2]);
        int y0 = atoi(argv[3]);
        int x1 = atoi(argv[4]);
        int y1 = atoi(argv[5]);
        display_line(x0, y0, x1, y1);
        display_flush();
        printf("Line drawn (%d,%d)->(%d,%d)\r\n", x0, y0, x1, y1);
        return 0;
    }

    if (strcmp(argv[1], "rect") == 0) {
        if (argc < 6) { printf("Usage: display rect <x> <y> <w> <h> [fill]\r\n"); return 1; }
        int x = atoi(argv[2]);
        int y = atoi(argv[3]);
        int w = atoi(argv[4]);
        int h = atoi(argv[5]);
        bool fill = (argc >= 7 && strcmp(argv[6], "fill") == 0);
        display_rect(x, y, w, h, fill);
        display_flush();
        printf("Rect at (%d,%d) %dx%d %s\r\n", x, y, w, h, fill ? "(filled)" : "(outline)");
        return 0;
    }

    if (strcmp(argv[1], "invert") == 0) {
        static bool inv_state = false;
        inv_state = !inv_state;
        display_invert(inv_state);
        printf("Display %s\r\n", inv_state ? "inverted" : "normal");
        return 0;
    }

    if (strcmp(argv[1], "contrast") == 0) {
        if (argc < 3) { printf("Usage: display contrast <0-255>\r\n"); return 1; }
        int c = atoi(argv[2]);
        if (c < 0) c = 0;
        if (c > 255) c = 255;
        display_set_contrast((uint8_t)c);
        printf("Contrast: %d/255\r\n", c);
        return 0;
    }

    if (strcmp(argv[1], "demo") == 0) {
        display_clear();
        /* Border rectangle */
        display_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, false);
        /* Title text */
        display_text(4, 4, "littleOS");
        /* Diagonal lines */
        display_line(0, 16, 127, 16);
        display_line(10, 20, 50, 50);
        display_line(60, 50, 100, 20);
        /* Small filled rect */
        display_rect(105, 20, 15, 15, true);
        /* Footer */
        display_text(4, 56, CHIP_MODEL_STR " SSD1306");
        display_flush();
        printf("Demo pattern drawn\r\n");
        return 0;
    }

    if (strcmp(argv[1], "sysinfo") == 0) {
        display_clear();
        display_text(0, 0,  "littleOS");
        display_line(0, 9, 127, 9);
        {
            char cpu_str[20], ram_str[20], clk_str[20];
            snprintf(cpu_str, sizeof(cpu_str), "CPU: %s", CHIP_MODEL_STR);
            snprintf(ram_str, sizeof(ram_str), "RAM: %uKB SRAM",
                     (unsigned)(CHIP_RAM_SIZE / 1024));
#ifdef PICO_BUILD
            snprintf(clk_str, sizeof(clk_str), "CLK: %luMHz",
                     (unsigned long)(clock_get_hz(clk_sys) / 1000000));
#else
            snprintf(clk_str, sizeof(clk_str), "CLK: ???MHz");
#endif
            display_text(0, 12, cpu_str);
            display_text(0, 22, ram_str);
            display_text(0, 32, clk_str);
        }
        display_text(0, 42, "DISP: SSD1306");
        display_text(0, 52, "128x64 I2C OLED");
        display_flush();
        printf("System info displayed\r\n");
        return 0;
    }

    printf("Unknown command: %s\r\n", argv[1]);
    return 1;
}
