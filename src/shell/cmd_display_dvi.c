/* cmd_display_dvi.c - DVI display shell command for HSTX output */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal/hstx_dvi.h"
#include "board/board_config.h"

#if BOARD_HAS_HSTX

/* Static framebuffer for 320x240 RGB332 (76.8 KB) */
static uint8_t s_dvi_fb[320 * 240];

static void dvi_usage(void) {
    printf("DVI Display (HSTX output)\r\n");
    printf("Usage:\r\n");
    printf("  dvi init [mode]    - Initialize (mode: 320x240, 640x480)\r\n");
    printf("  dvi test           - Show test pattern\r\n");
    printf("  dvi fill <color>   - Fill with color (0-255 RGB332)\r\n");
    printf("  dvi start          - Start DVI output\r\n");
    printf("  dvi stop           - Stop DVI output\r\n");
    printf("  dvi status         - Show DVI status\r\n");
}

int cmd_display_dvi(int argc, char *argv[]) {
    if (argc < 2) {
        dvi_usage();
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) {
        dvi_mode_t mode = DVI_MODE_320x240_60HZ;
        if (argc >= 3 && strcmp(argv[2], "640x480") == 0) {
            mode = DVI_MODE_640x480_60HZ;
        }

        int ret = hstx_dvi_init(mode, DVI_PIXEL_RGB332);
        if (ret < 0) {
            printf("Failed to initialize DVI\r\n");
            return -1;
        }

        uint32_t fb_size = (mode == DVI_MODE_320x240_60HZ)
            ? 320 * 240 : 640 * 480;

        if (fb_size > sizeof(s_dvi_fb)) {
            printf("Framebuffer too large for 640x480, using 320x240\r\n");
            fb_size = sizeof(s_dvi_fb);
        }

        hstx_dvi_set_framebuffer(s_dvi_fb, fb_size);
        printf("DVI initialized\r\n");
        return 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        dvi_status_t st;
        hstx_dvi_get_status(&st);
        if (!st.width) {
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
            printf("Usage: dvi fill <color>\r\n");
            return -1;
        }
        uint32_t color = (uint32_t)strtoul(argv[2], NULL, 0);
        dvi_status_t st;
        hstx_dvi_get_status(&st);
        if (!st.width) {
            printf("DVI not initialized\r\n");
            return -1;
        }
        hstx_dvi_fill(s_dvi_fb, (uint32_t)st.width * st.height, color);
        printf("Filled with color 0x%02lx\r\n", (unsigned long)color);
        return 0;
    }

    if (strcmp(argv[1], "start") == 0) {
        return hstx_dvi_start();
    }

    if (strcmp(argv[1], "stop") == 0) {
        return hstx_dvi_stop();
    }

    if (strcmp(argv[1], "status") == 0) {
        dvi_status_t st;
        if (hstx_dvi_get_status(&st) < 0) {
            printf("DVI not initialized\r\n");
            return -1;
        }
        printf("DVI Status:\r\n");
        printf("  Active:  %s\r\n", st.active ? "YES" : "NO");
        printf("  Mode:    %ux%u\r\n", st.width, st.height);
        printf("  Format:  %s\r\n",
               st.format == DVI_PIXEL_RGB332 ? "RGB332" : "RGB565");
        printf("  Frames:  %lu\r\n", (unsigned long)st.frame_count);
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
    printf("  Requires RP2350 with HSTX peripheral\r\n");
    return -1;
}

#endif /* BOARD_HAS_HSTX */
