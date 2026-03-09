/* cmd_usb.c - USB device shell commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal/usb_device.h"

static void cmd_usb_usage(void) {
    printf("USB device commands:\r\n");
    printf("  usb status                  - Show USB device status\r\n");
    printf("  usb mode cdc|hid|msc        - Set USB device mode\r\n");
    printf("  usb cdc send <text>          - Send text over CDC serial\r\n");
    printf("  usb hid type <text>          - Type text as USB keyboard\r\n");
    printf("  usb hid mouse <x> <y>        - Move mouse by (x,y)\r\n");
    printf("  usb hid click [left|right|middle] - Mouse click\r\n");
    printf("  usb msc mount                - Expose FS as USB mass storage\r\n");
}

static int cmd_usb_status(void) {
    usb_device_status_t status;
    int r = usb_device_get_status(&status);
    if (r != 0) {
        printf("Failed to get USB status\r\n");
        return r;
    }

    const char *mode_str = "NONE";
    switch (status.mode) {
        case USB_MODE_CDC: mode_str = "CDC (Virtual Serial)"; break;
        case USB_MODE_HID: mode_str = "HID (Keyboard/Mouse)"; break;
        case USB_MODE_MSC: mode_str = "MSC (Mass Storage)";   break;
        default: break;
    }

    printf("USB Device Status:\r\n");
    printf("  Mode:       %s\r\n", mode_str);
    printf("  Connected:  %s\r\n", status.connected ? "yes" : "no");
    printf("  Mounted:    %s\r\n", status.mounted   ? "yes" : "no");
    printf("  Suspended:  %s\r\n", status.suspended ? "yes" : "no");
    printf("  TX bytes:   %lu\r\n", (unsigned long)status.tx_bytes);
    printf("  RX bytes:   %lu\r\n", (unsigned long)status.rx_bytes);
    printf("  VID:PID:    %04X:%04X\r\n", status.vid, status.pid);

    return 0;
}

static int cmd_usb_mode(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: usb mode cdc|hid|msc\r\n");
        return -1;
    }

    usb_device_mode_t mode = USB_MODE_NONE;

    if (strcmp(argv[2], "cdc") == 0) {
        mode = USB_MODE_CDC;
    } else if (strcmp(argv[2], "hid") == 0) {
        mode = USB_MODE_HID;
    } else if (strcmp(argv[2], "msc") == 0) {
        mode = USB_MODE_MSC;
    } else {
        printf("Unknown mode '%s'. Use: cdc, hid, or msc\r\n", argv[2]);
        return -1;
    }

    int r = usb_device_set_mode(mode);
    if (r != 0) {
        printf("Failed to set USB mode\r\n");
        return r;
    }

    /* Initialize USB after setting mode */
    r = usb_device_init();
    if (r != 0) {
        printf("Failed to initialize USB device\r\n");
        return r;
    }

    printf("USB mode set to %s, device initialized\r\n", argv[2]);
    return 0;
}

static int cmd_usb_cdc(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: usb cdc send <text>\r\n");
        return -1;
    }

    if (strcmp(argv[2], "send") != 0) {
        printf("Usage: usb cdc send <text>\r\n");
        return -1;
    }

    if (argc < 4) {
        printf("Usage: usb cdc send <text>\r\n");
        return -1;
    }

    /* Concatenate all remaining args with spaces to form the message */
    char buf[256];
    int  pos = 0;
    for (int i = 3; i < argc && pos < (int)sizeof(buf) - 1; i++) {
        if (i > 3 && pos < (int)sizeof(buf) - 1) {
            buf[pos++] = ' ';
        }
        int slen = (int)strlen(argv[i]);
        int copy = slen;
        if (pos + copy >= (int)sizeof(buf) - 1) {
            copy = (int)sizeof(buf) - 1 - pos;
        }
        memcpy(buf + pos, argv[i], copy);
        pos += copy;
    }
    buf[pos++] = '\n';
    buf[pos]   = '\0';

    int r = usb_cdc_write_str(buf);
    if (r < 0) {
        printf("CDC send failed (mode not CDC or not connected)\r\n");
        return r;
    }

    printf("CDC: sent %d bytes\r\n", r);
    return 0;
}

static int cmd_usb_hid(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage:\r\n");
        printf("  usb hid type <text>\r\n");
        printf("  usb hid mouse <x> <y>\r\n");
        printf("  usb hid click [left|right|middle]\r\n");
        return -1;
    }

    if (strcmp(argv[2], "type") == 0) {
        if (argc < 4) {
            printf("Usage: usb hid type <text>\r\n");
            return -1;
        }

        /* Concatenate remaining args */
        char buf[256];
        int  pos = 0;
        for (int i = 3; i < argc && pos < (int)sizeof(buf) - 1; i++) {
            if (i > 3 && pos < (int)sizeof(buf) - 1) {
                buf[pos++] = ' ';
            }
            int slen = (int)strlen(argv[i]);
            int copy = slen;
            if (pos + copy >= (int)sizeof(buf) - 1) {
                copy = (int)sizeof(buf) - 1 - pos;
            }
            memcpy(buf + pos, argv[i], copy);
            pos += copy;
        }
        buf[pos] = '\0';

        int r = usb_hid_keyboard_type(buf);
        if (r < 0) {
            printf("HID type failed (mode not HID or not mounted)\r\n");
            return r;
        }

        printf("HID: typed %d characters\r\n", r);
        return 0;
    }

    if (strcmp(argv[2], "mouse") == 0) {
        if (argc < 5) {
            printf("Usage: usb hid mouse <x> <y>\r\n");
            return -1;
        }

        int8_t x = (int8_t)atoi(argv[3]);
        int8_t y = (int8_t)atoi(argv[4]);

        int r = usb_hid_mouse_move(x, y, 0);
        if (r < 0) {
            printf("HID mouse move failed\r\n");
            return r;
        }

        printf("HID: mouse moved (%d, %d)\r\n", x, y);
        return 0;
    }

    if (strcmp(argv[2], "click") == 0) {
        uint8_t buttons = 0x01; /* Default: left click */

        if (argc >= 4) {
            if (strcmp(argv[3], "left") == 0) {
                buttons = 0x01;
            } else if (strcmp(argv[3], "right") == 0) {
                buttons = 0x02;
            } else if (strcmp(argv[3], "middle") == 0) {
                buttons = 0x04;
            } else {
                printf("Unknown button '%s'. Use: left, right, middle\r\n",
                       argv[3]);
                return -1;
            }
        }

        int r = usb_hid_mouse_click(buttons);
        if (r < 0) {
            printf("HID mouse click failed\r\n");
            return r;
        }

        const char *btn_str = (buttons == 0x01) ? "left" :
                              (buttons == 0x02) ? "right" : "middle";
        printf("HID: %s click sent\r\n", btn_str);
        return 0;
    }

    printf("Unknown HID subcommand '%s'\r\n", argv[2]);
    printf("Use: type, mouse, click\r\n");
    return -1;
}

static int cmd_usb_msc(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: usb msc mount\r\n");
        return -1;
    }

    if (strcmp(argv[2], "mount") != 0) {
        printf("Usage: usb msc mount\r\n");
        return -1;
    }

    /* Set mode to MSC and initialize */
    int r = usb_device_set_mode(USB_MODE_MSC);
    if (r != 0) {
        printf("Failed to set MSC mode\r\n");
        return r;
    }

    r = usb_device_init();
    if (r != 0) {
        printf("Failed to initialize USB\r\n");
        return r;
    }

    printf("USB MSC mode active. Connect USB cable to host.\r\n");
    printf("Device will appear as a removable drive.\r\n");

    if (usb_msc_is_ejected()) {
        printf("Note: device was previously ejected, re-initializing.\r\n");
    }

    return 0;
}

int cmd_usb(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_usb_usage();
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) return cmd_usb_status();
    if (strcmp(argv[1], "mode")   == 0) return cmd_usb_mode(argc, argv);
    if (strcmp(argv[1], "cdc")    == 0) return cmd_usb_cdc(argc, argv);
    if (strcmp(argv[1], "hid")    == 0) return cmd_usb_hid(argc, argv);
    if (strcmp(argv[1], "msc")    == 0) return cmd_usb_msc(argc, argv);

    printf("Unknown USB subcommand '%s'\r\n", argv[1]);
    cmd_usb_usage();
    return -1;
}
