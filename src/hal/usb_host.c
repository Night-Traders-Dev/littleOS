/* usb_host.c - USB Host HID Keyboard Driver for littleOS
 *
 * Enables USB host mode to accept a standard USB keyboard.
 * Keystrokes are delivered to the shell via the Pico SDK's stdio
 * driver system (integrates with getchar_timeout_us).
 *
 * Based on TinyUSB host HID example, adapted for littleOS.
 *
 * Build with -DLITTLEOS_USB_HOST=ON (mutually exclusive with USB stdio).
 */
#include "hal/usb_host.h"
#include "module.h"
#include "dmesg.h"

#include <stdio.h>
#include <string.h>

#ifdef LITTLEOS_USB_HOST

#include "pico/stdlib.h"
#include "pico/stdio/driver.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "class/hid/hid_host.h"

/* Provide tusb_time_millis_api required by TinyUSB host stack.
 * Normally this comes from tinyusb_board (bsp/board.c), but we don't
 * link that target since its board_init() conflicts with our setup. */
uint32_t tusb_time_millis_api(void) {
    return to_ms_since_boot(get_absolute_time());
}

/* ================================================================
 * Keyboard Input Ring Buffer
 * ================================================================ */

#define KBD_BUF_SIZE 64

static struct {
    bool        active;
    bool        keyboard_connected;
    uint8_t     dev_addr;
    uint8_t     instance;

    /* Ring buffer for converted ASCII characters */
    volatile char     buf[KBD_BUF_SIZE];
    volatile uint8_t  buf_head;
    volatile uint8_t  buf_tail;
} s_kbd;

static void kbd_buf_push(char c) {
    uint8_t next = (s_kbd.buf_head + 1) % KBD_BUF_SIZE;
    if (next != s_kbd.buf_tail) {
        s_kbd.buf[s_kbd.buf_head] = c;
        s_kbd.buf_head = next;
    }
}

static int kbd_buf_pop(void) {
    if (s_kbd.buf_head == s_kbd.buf_tail) return -1;
    char c = s_kbd.buf[s_kbd.buf_tail];
    s_kbd.buf_tail = (s_kbd.buf_tail + 1) % KBD_BUF_SIZE;
    return (unsigned char)c;
}

/* ================================================================
 * HID Keycode to ASCII conversion
 * ================================================================ */

/* Uses TinyUSB's built-in keycode-to-ASCII mapping */
static uint8_t const keycode2ascii[128][2] = { HID_KEYCODE_TO_ASCII };

/* Previous report for detecting new key presses */
static hid_keyboard_report_t prev_report = { 0, 0, {0} };

static bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keycode[i] == keycode) return true;
    }
    return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report) {
    bool is_shift = report->modifier &
        (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
    bool is_ctrl = report->modifier &
        (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL);

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t keycode = report->keycode[i];
        if (keycode == 0) continue;

        if (find_key_in_report(&prev_report, keycode))
            continue;  /* Key held, not new press */

        /* Special keys → ANSI escape sequences */
        if (keycode == HID_KEY_ARROW_UP)    { kbd_buf_push(0x1B); kbd_buf_push('['); kbd_buf_push('A'); continue; }
        if (keycode == HID_KEY_ARROW_DOWN)  { kbd_buf_push(0x1B); kbd_buf_push('['); kbd_buf_push('B'); continue; }
        if (keycode == HID_KEY_ARROW_RIGHT) { kbd_buf_push(0x1B); kbd_buf_push('['); kbd_buf_push('C'); continue; }
        if (keycode == HID_KEY_ARROW_LEFT)  { kbd_buf_push(0x1B); kbd_buf_push('['); kbd_buf_push('D'); continue; }
        if (keycode == HID_KEY_TAB)         { kbd_buf_push('\t'); continue; }
        if (keycode == HID_KEY_ESCAPE)      { kbd_buf_push(0x1B); continue; }
        if (keycode == HID_KEY_BACKSPACE)   { kbd_buf_push(0x7F); continue; }
        if (keycode == HID_KEY_DELETE)       { kbd_buf_push(0x1B); kbd_buf_push('['); kbd_buf_push('3'); kbd_buf_push('~'); continue; }

        /* Ctrl+key combinations */
        if (is_ctrl && keycode >= HID_KEY_A && keycode <= HID_KEY_Z) {
            kbd_buf_push((char)(keycode - HID_KEY_A + 1));  /* Ctrl+A=1, Ctrl+C=3, etc. */
            continue;
        }

        /* Regular ASCII key */
        if (keycode < 128) {
            uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];
            if (ch) kbd_buf_push((char)ch);
        }
    }

    prev_report = *report;
}

/* ================================================================
 * TinyUSB Host Callbacks (called by tuh_task)
 * ================================================================ */

/* HID report info for non-boot-protocol devices */
#define MAX_REPORT 4
static struct {
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                       uint8_t const *desc_report, uint16_t desc_len) {
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        s_kbd.keyboard_connected = true;
        s_kbd.dev_addr = dev_addr;
        s_kbd.instance = instance;
        dmesg_info("USB keyboard connected (addr=%d)", dev_addr);
    }

    if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(
            hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    }

    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (dev_addr == s_kbd.dev_addr) {
        s_kbd.keyboard_connected = false;
        dmesg_info("USB keyboard disconnected (addr=%d)", dev_addr);
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                  uint8_t const *report, uint16_t len) {
    (void)len;
    uint8_t itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        process_kbd_report((hid_keyboard_report_t const *)report);
    } else if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
        /* Generic report — check if it's a keyboard */
        uint8_t rpt_count = hid_info[instance].report_count;
        tuh_hid_report_info_t *rpt_arr = hid_info[instance].report_info;

        tuh_hid_report_info_t *rpt_info = NULL;
        if (rpt_count == 1 && rpt_arr[0].report_id == 0) {
            rpt_info = &rpt_arr[0];
        } else if (len > 0) {
            uint8_t rpt_id = report[0];
            for (uint8_t i = 0; i < rpt_count; i++) {
                if (rpt_id == rpt_arr[i].report_id) {
                    rpt_info = &rpt_arr[i];
                    break;
                }
            }
            report++;
        }

        if (rpt_info && rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP &&
            rpt_info->usage == HID_USAGE_DESKTOP_KEYBOARD) {
            process_kbd_report((hid_keyboard_report_t const *)report);
        }
    }

    tuh_hid_receive_report(dev_addr, instance);
}

/* ================================================================
 * Pico SDK stdio driver integration
 * ================================================================ */

static int kbd_stdio_in_chars(char *buf, int length) {
    int count = 0;
    while (count < length) {
        int c = kbd_buf_pop();
        if (c < 0) break;
        buf[count++] = (char)c;
    }
    return count ? count : PICO_ERROR_NO_DATA;
}

static stdio_driver_t usb_kbd_stdio_driver = {
    .out_chars = NULL,          /* No output — that's DVI/UART's job */
    .in_chars  = kbd_stdio_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = false,
#endif
};

/* ================================================================
 * Public API
 * ================================================================ */

int usb_host_init(void) {
    if (s_kbd.active) return 0;

    memset((void *)&s_kbd, 0, sizeof(s_kbd));
    memset(&prev_report, 0, sizeof(prev_report));

    tuh_init(BOARD_TUH_RHPORT);

    s_kbd.active = true;
    dmesg_info("USB host mode initialized (HID keyboard)");
    return 0;
}

void usb_host_deinit(void) {
    if (!s_kbd.active) return;
    usb_host_remove_stdio();
    s_kbd.active = false;
}

void usb_host_task(void) {
    if (!s_kbd.active) return;
    tuh_task();
}

bool usb_host_keyboard_connected(void) {
    return s_kbd.keyboard_connected;
}

bool usb_host_is_active(void) {
    return s_kbd.active;
}

void usb_host_install_stdio(void) {
    stdio_set_driver_enabled(&usb_kbd_stdio_driver, true);
}

void usb_host_remove_stdio(void) {
    stdio_set_driver_enabled(&usb_kbd_stdio_driver, false);
}

/* ================================================================
 * Kernel Module Interface
 * ================================================================ */

static int usb_host_mod_init(module_t *mod) {
    (void)mod;
    int ret = usb_host_init();
    if (ret == 0)
        usb_host_install_stdio();
    return ret;
}

static void usb_host_mod_deinit(module_t *mod) {
    (void)mod;
    usb_host_deinit();
}

static void usb_host_mod_status(module_t *mod) {
    (void)mod;
    printf("USB Host (HID Keyboard)\r\n");
    printf("  Active:    %s\r\n", s_kbd.active ? "yes" : "no");
    printf("  Keyboard:  %s\r\n", s_kbd.keyboard_connected ? "connected" : "not connected");
    if (s_kbd.keyboard_connected)
        printf("  Address:   %d\r\n", s_kbd.dev_addr);
}

static const module_ops_t usb_host_mod_ops = {
    .init   = usb_host_mod_init,
    .deinit = usb_host_mod_deinit,
    .status = usb_host_mod_status,
};

static module_t usb_host_module = {
    .name        = "usb_keyboard",
    .description = "USB HID keyboard input (host mode)",
    .version     = "1.0.0",
    .type        = MODULE_TYPE_DRIVER,
    .ops         = &usb_host_mod_ops,
};

void usb_host_module_register(void) {
    module_register(&usb_host_module);
}

#else /* !LITTLEOS_USB_HOST */

/* Stubs for builds without USB host */
int  usb_host_init(void) { return -1; }
void usb_host_deinit(void) {}
void usb_host_task(void) {}
bool usb_host_keyboard_connected(void) { return false; }
bool usb_host_is_active(void) { return false; }
void usb_host_install_stdio(void) {}
void usb_host_remove_stdio(void) {}
void usb_host_module_register(void) {}

#endif /* LITTLEOS_USB_HOST */
