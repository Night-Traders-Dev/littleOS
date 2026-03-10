/* cmd_neopixel.c - NeoPixel shell commands for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "neopixel.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

static uint32_t parse_color(const char *s) {
    if (s[0] == '#') s++;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    return (uint32_t)strtoul(s, NULL, 16);
}

int cmd_neopixel(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: neopixel <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  init PIN COUNT      - Initialize NeoPixel strip\r\n");
        printf("  set INDEX COLOR     - Set pixel (color: RRGGBB hex)\r\n");
        printf("  fill COLOR          - Fill all pixels\r\n");
        printf("  clear               - Turn off all pixels\r\n");
        printf("  brightness N        - Set brightness (0-255)\r\n");
        printf("  rainbow [SPEED]     - Rainbow animation\r\n");
        printf("  chase COLOR [SPEED] - Chase animation\r\n");
        printf("  status              - Show current state\r\n");
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) {
        if (argc < 4) { printf("Usage: neopixel init <pin> <count>\r\n"); return 1; }
        unsigned int pin = (unsigned int)atoi(argv[2]);
        int count = atoi(argv[3]);
        neopixel_init(pin, count);
        printf("NeoPixel: %d LEDs on GPIO%u initialized\r\n", neopixel_get_count(), pin);
        return 0;
    }

    if (neopixel_get_count() == 0) {
        printf("NeoPixels not initialized. Use: neopixel init <pin> <count>\r\n");
        return 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) { printf("Usage: neopixel set <index> <RRGGBB>\r\n"); return 1; }
        int idx = atoi(argv[2]);
        uint32_t color = parse_color(argv[3]);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        neopixel_set(idx, r, g, b);
        neopixel_show();
        printf("Pixel %d = #%02X%02X%02X\r\n", idx, r, g, b);
        return 0;
    }

    if (strcmp(argv[1], "fill") == 0) {
        if (argc < 3) { printf("Usage: neopixel fill <RRGGBB>\r\n"); return 1; }
        uint32_t color = parse_color(argv[2]);
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        neopixel_set_all(r, g, b);
        neopixel_show();
        printf("All pixels = #%02X%02X%02X\r\n", r, g, b);
        return 0;
    }

    if (strcmp(argv[1], "clear") == 0) {
        neopixel_clear();
        neopixel_show();
        printf("All pixels off\r\n");
        return 0;
    }

    if (strcmp(argv[1], "brightness") == 0) {
        if (argc < 3) { printf("Usage: neopixel brightness <0-255>\r\n"); return 1; }
        int b = atoi(argv[2]);
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        neopixel_set_brightness((uint8_t)b);
        neopixel_show();
        printf("Brightness: %d/255\r\n", b);
        return 0;
    }

    if (strcmp(argv[1], "rainbow") == 0) {
#ifdef PICO_BUILD
        int speed = (argc >= 3) ? atoi(argv[2]) : 50;
        printf("Rainbow animation (speed=%d ms). Press any key to stop.\r\n", speed);
        for (int offset = 0; ; offset = (offset + 1) % 256) {
            neopixel_rainbow(offset);
            neopixel_show();
            sleep_ms((uint32_t)speed);
            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT) break;
        }
        printf("Stopped.\r\n");
#else
        printf("Animations require hardware\r\n");
#endif
        return 0;
    }

    if (strcmp(argv[1], "chase") == 0) {
#ifdef PICO_BUILD
        uint32_t color = (argc >= 3) ? parse_color(argv[2]) : 0xFF0000;
        int speed = (argc >= 4) ? atoi(argv[3]) : 100;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;

        printf("Chase animation. Press any key to stop.\r\n");
        for (int pos = 0; ; pos = (pos + 1) % neopixel_get_count()) {
            neopixel_chase(r, g, b, pos);
            neopixel_show();
            sleep_ms((uint32_t)speed);
            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT) break;
        }
        printf("Stopped.\r\n");
#else
        printf("Animations require hardware\r\n");
#endif
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        printf("NeoPixel: %d LEDs configured\r\n", neopixel_get_count());
        return 0;
    }

    printf("Unknown command: %s\r\n", argv[1]);
    return 1;
}
