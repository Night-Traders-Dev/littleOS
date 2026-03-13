/* cmd_display_dvi.c - DVI display shell command for HSTX output */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal/hstx_dvi.h"
#include "board/board_config.h"

#if BOARD_HAS_HSTX

/* Static framebuffer for 320x240 RGB332 (76.8 KB)
 * This is the practical default for littleOS — fits comfortably in SRAM.
 * 640x480 RGB332 would need 307 KB which is too large for a built-in buffer. */
static uint8_t s_dvi_fb[320 * 240] __attribute__((aligned(4)));

static void dvi_usage(void) {
    printf("DVI Display (HSTX output on GPIO 12-19)\r\n");
    printf("  Requires DVI/HDMI breakout with 270-ohm resistors\r\n");
    printf("  Pinout: D0+/- GP12-13, CK+/- GP14-15, D2+/- GP16-17, D1+/- GP18-19\r\n");
    printf("\r\n");
    printf("Usage:\r\n");
    printf("  dvi init [mode]    - Initialize (mode: 320x240 (default), 640x480)\r\n");
    printf("  dvi test           - Show color bar test pattern\r\n");
    printf("  dvi fill <color>   - Fill with RGB332 color (0x00-0xFF)\r\n");
    printf("  dvi start          - Start DVI output\r\n");
    printf("  dvi stop           - Stop DVI output and release hardware\r\n");
    printf("  dvi status         - Show DVI status\r\n");
}

int cmd_display_dvi(int argc, char *argv[]) {
    if (argc < 2) {
        dvi_usage();
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) {
        dvi_mode_t mode = DVI_MODE_320x240_60HZ;
        dvi_pixel_format_t format = DVI_PIXEL_RGB332;

        if (argc >= 3 && strcmp(argv[2], "640x480") == 0) {
            printf("640x480 requires 307 KB framebuffer — not supported with built-in buffer\r\n");
            printf("Using 320x240 (pixel-doubled to 640x480 output)\r\n");
        }

        /* Stop any existing output first */
        hstx_dvi_stop();

        int ret = hstx_dvi_init(mode, format);
        if (ret < 0) {
            printf("Failed to initialize DVI\r\n");
            return -1;
        }

        /* Clear framebuffer */
        memset(s_dvi_fb, 0, sizeof(s_dvi_fb));

        ret = hstx_dvi_set_framebuffer(s_dvi_fb, sizeof(s_dvi_fb));
        if (ret < 0) {
            printf("Failed to set framebuffer\r\n");
            hstx_dvi_stop();
            return -1;
        }

        printf("DVI initialized: 320x240 RGB332 (pixel-doubled to 640x480 output)\r\n");
        return 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        dvi_status_t st;
        if (hstx_dvi_get_status(&st) < 0 || !st.width) {
            printf("DVI not initialized. Run 'dvi init' first.\r\n");
            return -1;
        }
        hstx_dvi_test_pattern(s_dvi_fb, st.width, st.height, st.format);
        printf("Test pattern drawn (%ux%u)\r\n", st.width, st.height);

        if (!st.active) {
            hstx_dvi_start();
            printf("DVI output started\r\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "fill") == 0) {
        if (argc < 3) {
            printf("Usage: dvi fill <color>  (0x00-0xFF for RGB332)\r\n");
            printf("  RGB332: RRRGGGBB  e.g. 0xE0=red, 0x1C=green, 0x03=blue\r\n");
            return -1;
        }
        uint32_t color = (uint32_t)strtoul(argv[2], NULL, 0);
        dvi_status_t st;
        if (hstx_dvi_get_status(&st) < 0 || !st.width) {
            printf("DVI not initialized\r\n");
            return -1;
        }
        uint32_t fb_size = (uint32_t)st.width * st.height * (st.format == DVI_PIXEL_RGB565 ? 2 : 1);
        hstx_dvi_fill(s_dvi_fb, fb_size, color);
        printf("Filled with color 0x%02lx\r\n", (unsigned long)(color & 0xFF));
        return 0;
    }

    if (strcmp(argv[1], "start") == 0) {
        int ret = hstx_dvi_start();
        if (ret < 0) {
            printf("Failed to start DVI output\r\n");
            return -1;
        }
        printf("DVI output started\r\n");
        return 0;
    }

    if (strcmp(argv[1], "stop") == 0) {
        hstx_dvi_stop();
        printf("DVI output stopped\r\n");
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        dvi_status_t st;
        if (hstx_dvi_get_status(&st) < 0) {
            printf("DVI not initialized\r\n");
            return -1;
        }
        printf("DVI Status:\r\n");
        printf("  Active:     %s\r\n", st.active ? "YES" : "NO");
        printf("  Framebuf:   %ux%u\r\n", st.width, st.height);
        printf("  Output:     640x480 @ 60 Hz\r\n");
        printf("  Format:     %s\r\n",
               st.format == DVI_PIXEL_RGB332 ? "RGB332 (8-bit)" : "RGB565 (16-bit)");
        printf("  Frames:     %lu\r\n", (unsigned long)st.frame_count);
        printf("  DMA:        ping-pong (IRQ-driven)\r\n");
        printf("  GPIO:       12-19 (HSTX)\r\n");
        return 0;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    dvi_usage();
    return -1;
}

#else /* !BOARD_HAS_HSTX */

int cmd_display_dvi(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("DVI output not available on this board\r\n");
    printf("  Requires RP2350 with HSTX peripheral (Pico 2, Feather RP2350)\r\n");
    return -1;
}

#endif /* BOARD_HAS_HSTX */
