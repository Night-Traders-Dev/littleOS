/* usb_device.c - USB Device Mode HAL Implementation for littleOS (RP2040) */

#include "hal/usb_device.h"
#include "dmesg.h"
#include <string.h>

/* USB device mode is mutually exclusive with USB host mode */
#if defined(PICO_BUILD) && !defined(LITTLEOS_USB_HOST)
#if __has_include("tusb.h")
#include "tusb.h"
#include "bsp/board.h"
#define HAS_TINYUSB 1
#else
#define HAS_TINYUSB 0
#endif
#else
#define HAS_TINYUSB 0
#endif

/* ------------------------------------------------------------------ */
/*  Internal state                                                     */
/* ------------------------------------------------------------------ */

static usb_device_mode_t usb_current_mode  = USB_MODE_NONE;
static bool              usb_initialized   = false;
static bool              usb_connected     = false;
static bool              usb_mounted       = false;
static bool              usb_suspended     = false;
static uint32_t          usb_tx_bytes      = 0;
static uint32_t          usb_rx_bytes      = 0;

/* MSC backing storage */
static const uint8_t    *msc_storage_data  = NULL;
static uint32_t          msc_block_count   = 0;
static uint16_t          msc_block_size    = 512;
static bool              msc_ejected       = false;

/* USB VID/PID defaults (littleOS custom) */
#define LITTLEOS_USB_VID  0xCAFE
#define LITTLEOS_USB_PID  0x4010

/* ------------------------------------------------------------------ */
/*  Core functions                                                     */
/* ------------------------------------------------------------------ */

int usb_device_init(void) {
    if (usb_initialized) {
        dmesg_warn("usb: already initialized");
        return 0;
    }

#if HAS_TINYUSB
    board_init();
    tusb_init();
#endif

    usb_initialized = true;
    usb_connected   = false;
    usb_mounted     = false;
    usb_suspended   = false;
    usb_tx_bytes    = 0;
    usb_rx_bytes    = 0;

    dmesg_info("usb: device subsystem initialized, mode=%d", usb_current_mode);
    return 0;
}

int usb_device_set_mode(usb_device_mode_t mode) {
    if (mode > USB_MODE_MSC) {
        dmesg_err("usb: invalid mode %d", mode);
        return -1;
    }

    usb_current_mode = mode;

    const char *mode_str = "none";
    switch (mode) {
        case USB_MODE_CDC: mode_str = "CDC";  break;
        case USB_MODE_HID: mode_str = "HID";  break;
        case USB_MODE_MSC: mode_str = "MSC";  break;
        default: break;
    }

    dmesg_info("usb: mode set to %s", mode_str);
    return 0;
}

int usb_device_get_status(usb_device_status_t *status) {
    if (!status) return -1;

#if HAS_TINYUSB
    usb_connected = tud_connected();
    usb_mounted   = tud_mounted();
    usb_suspended = tud_suspended();
#endif

    status->mode      = usb_current_mode;
    status->connected = usb_connected;
    status->mounted   = usb_mounted;
    status->suspended = usb_suspended;
    status->tx_bytes  = usb_tx_bytes;
    status->rx_bytes  = usb_rx_bytes;
    status->vid       = LITTLEOS_USB_VID;
    status->pid       = LITTLEOS_USB_PID;

    return 0;
}

void usb_device_task(void) {
    if (!usb_initialized) return;

#if HAS_TINYUSB
    tud_task();
#endif
}

/* ------------------------------------------------------------------ */
/*  CDC (Virtual Serial Port) functions                                */
/* ------------------------------------------------------------------ */

int usb_cdc_write(const uint8_t *data, size_t len) {
    if (!data || len == 0) return -1;
    if (usb_current_mode != USB_MODE_CDC) return -1;

#if HAS_TINYUSB
    if (!tud_cdc_connected()) return -1;

    uint32_t written = 0;
    while (written < len) {
        uint32_t avail = tud_cdc_write_available();
        if (avail == 0) {
            tud_cdc_write_flush();
            tud_task();
            continue;
        }
        uint32_t chunk = len - written;
        if (chunk > avail) chunk = avail;
        uint32_t n = tud_cdc_write(data + written, chunk);
        written += n;
    }
    tud_cdc_write_flush();
    usb_tx_bytes += written;
    return (int)written;
#else
    usb_tx_bytes += len;
    return (int)len;
#endif
}

int usb_cdc_read(uint8_t *data, size_t max_len) {
    if (!data || max_len == 0) return -1;
    if (usb_current_mode != USB_MODE_CDC) return -1;

#if HAS_TINYUSB
    if (!tud_cdc_available()) return 0;

    uint32_t count = tud_cdc_read(data, max_len);
    usb_rx_bytes += count;
    return (int)count;
#else
    return 0;
#endif
}

int usb_cdc_available(void) {
    if (usb_current_mode != USB_MODE_CDC) return -1;

#if HAS_TINYUSB
    return (int)tud_cdc_available();
#else
    return 0;
#endif
}

int usb_cdc_write_str(const char *str) {
    if (!str) return -1;
    return usb_cdc_write((const uint8_t *)str, strlen(str));
}

/* ------------------------------------------------------------------ */
/*  HID Keyboard functions                                             */
/* ------------------------------------------------------------------ */

#if HAS_TINYUSB
/* ASCII to HID keycode mapping table.
 * Each entry: { keycode, modifier }.
 * modifier bit 0x02 = left shift. */
static const uint8_t ascii_to_hid[][2] = {
    [' ']  = { HID_KEY_SPACE,          0 },
    ['!']  = { HID_KEY_1,              0x02 },
    ['"']  = { HID_KEY_APOSTROPHE,     0x02 },
    ['#']  = { HID_KEY_3,              0x02 },
    ['$']  = { HID_KEY_4,              0x02 },
    ['%']  = { HID_KEY_5,              0x02 },
    ['&']  = { HID_KEY_7,              0x02 },
    ['\''] = { HID_KEY_APOSTROPHE,     0 },
    ['(']  = { HID_KEY_9,              0x02 },
    [')']  = { HID_KEY_0,              0x02 },
    ['*']  = { HID_KEY_8,              0x02 },
    ['+']  = { HID_KEY_EQUAL,          0x02 },
    [',']  = { HID_KEY_COMMA,          0 },
    ['-']  = { HID_KEY_MINUS,          0 },
    ['.']  = { HID_KEY_PERIOD,         0 },
    ['/']  = { HID_KEY_SLASH,          0 },
    ['0']  = { HID_KEY_0,              0 },
    ['1']  = { HID_KEY_1,              0 },
    ['2']  = { HID_KEY_2,              0 },
    ['3']  = { HID_KEY_3,              0 },
    ['4']  = { HID_KEY_4,              0 },
    ['5']  = { HID_KEY_5,              0 },
    ['6']  = { HID_KEY_6,              0 },
    ['7']  = { HID_KEY_7,              0 },
    ['8']  = { HID_KEY_8,              0 },
    ['9']  = { HID_KEY_9,              0 },
    [':']  = { HID_KEY_SEMICOLON,      0x02 },
    [';']  = { HID_KEY_SEMICOLON,      0 },
    ['<']  = { HID_KEY_COMMA,          0x02 },
    ['=']  = { HID_KEY_EQUAL,          0 },
    ['>']  = { HID_KEY_PERIOD,         0x02 },
    ['?']  = { HID_KEY_SLASH,          0x02 },
    ['@']  = { HID_KEY_2,              0x02 },
    ['A']  = { HID_KEY_A,              0x02 },
    ['B']  = { HID_KEY_B,              0x02 },
    ['C']  = { HID_KEY_C,              0x02 },
    ['D']  = { HID_KEY_D,              0x02 },
    ['E']  = { HID_KEY_E,              0x02 },
    ['F']  = { HID_KEY_F,              0x02 },
    ['G']  = { HID_KEY_G,              0x02 },
    ['H']  = { HID_KEY_H,              0x02 },
    ['I']  = { HID_KEY_I,              0x02 },
    ['J']  = { HID_KEY_J,              0x02 },
    ['K']  = { HID_KEY_K,              0x02 },
    ['L']  = { HID_KEY_L,              0x02 },
    ['M']  = { HID_KEY_M,              0x02 },
    ['N']  = { HID_KEY_N,              0x02 },
    ['O']  = { HID_KEY_O,              0x02 },
    ['P']  = { HID_KEY_P,              0x02 },
    ['Q']  = { HID_KEY_Q,              0x02 },
    ['R']  = { HID_KEY_R,              0x02 },
    ['S']  = { HID_KEY_S,              0x02 },
    ['T']  = { HID_KEY_T,              0x02 },
    ['U']  = { HID_KEY_U,              0x02 },
    ['V']  = { HID_KEY_V,              0x02 },
    ['W']  = { HID_KEY_W,              0x02 },
    ['X']  = { HID_KEY_X,              0x02 },
    ['Y']  = { HID_KEY_Y,              0x02 },
    ['Z']  = { HID_KEY_Z,              0x02 },
    ['[']  = { HID_KEY_BRACKET_LEFT,   0 },
    ['\\'] = { HID_KEY_BACKSLASH,      0 },
    [']']  = { HID_KEY_BRACKET_RIGHT,  0 },
    ['^']  = { HID_KEY_6,              0x02 },
    ['_']  = { HID_KEY_MINUS,          0x02 },
    ['`']  = { HID_KEY_GRAVE,          0 },
    ['a']  = { HID_KEY_A,              0 },
    ['b']  = { HID_KEY_B,              0 },
    ['c']  = { HID_KEY_C,              0 },
    ['d']  = { HID_KEY_D,              0 },
    ['e']  = { HID_KEY_E,              0 },
    ['f']  = { HID_KEY_F,              0 },
    ['g']  = { HID_KEY_G,              0 },
    ['h']  = { HID_KEY_H,              0 },
    ['i']  = { HID_KEY_I,              0 },
    ['j']  = { HID_KEY_J,              0 },
    ['k']  = { HID_KEY_K,              0 },
    ['l']  = { HID_KEY_L,              0 },
    ['m']  = { HID_KEY_M,              0 },
    ['n']  = { HID_KEY_N,              0 },
    ['o']  = { HID_KEY_O,              0 },
    ['p']  = { HID_KEY_P,              0 },
    ['q']  = { HID_KEY_Q,              0 },
    ['r']  = { HID_KEY_R,              0 },
    ['s']  = { HID_KEY_S,              0 },
    ['t']  = { HID_KEY_T,              0 },
    ['u']  = { HID_KEY_U,              0 },
    ['v']  = { HID_KEY_V,              0 },
    ['w']  = { HID_KEY_W,              0 },
    ['x']  = { HID_KEY_X,              0 },
    ['y']  = { HID_KEY_Y,              0 },
    ['z']  = { HID_KEY_Z,              0 },
    ['{']  = { HID_KEY_BRACKET_LEFT,   0x02 },
    ['|']  = { HID_KEY_BACKSLASH,      0x02 },
    ['}']  = { HID_KEY_BRACKET_RIGHT,  0x02 },
    ['~']  = { HID_KEY_GRAVE,          0x02 },
};

#define ASCII_TO_HID_SIZE (sizeof(ascii_to_hid) / sizeof(ascii_to_hid[0]))

/* Wait until HID report is sent, with timeout */
static bool hid_wait_ready(uint32_t timeout_ms) {
    uint32_t start = board_millis();
    while (!tud_hid_ready()) {
        tud_task();
        if ((board_millis() - start) > timeout_ms) return false;
    }
    return true;
}
#endif /* PICO_BUILD */

int usb_hid_keyboard_press(uint8_t keycode, uint8_t modifier) {
    if (usb_current_mode != USB_MODE_HID) return -1;

#if HAS_TINYUSB
    if (!tud_mounted()) return -1;
    if (!hid_wait_ready(100)) return -1;

    uint8_t keycodes[6] = { keycode, 0, 0, 0, 0, 0 };
    tud_hid_keyboard_report(0, modifier, keycodes);
    usb_tx_bytes += 8;
    return 0;
#else
    (void)keycode;
    (void)modifier;
    return 0;
#endif
}

int usb_hid_keyboard_release(void) {
    if (usb_current_mode != USB_MODE_HID) return -1;

#if HAS_TINYUSB
    if (!hid_wait_ready(100)) return -1;

    tud_hid_keyboard_report(0, 0, NULL);
    return 0;
#else
    return 0;
#endif
}

int usb_hid_keyboard_type(const char *str) {
    if (!str) return -1;
    if (usb_current_mode != USB_MODE_HID) return -1;

#if HAS_TINYUSB
    if (!tud_mounted()) return -1;

    int typed = 0;
    for (const char *p = str; *p != '\0'; p++) {
        uint8_t ch = (uint8_t)*p;

        /* Handle newline as Enter key */
        uint8_t keycode  = 0;
        uint8_t modifier = 0;

        if (ch == '\n') {
            keycode = HID_KEY_ENTER;
        } else if (ch == '\t') {
            keycode = HID_KEY_TAB;
        } else if (ch < ASCII_TO_HID_SIZE && ascii_to_hid[ch][0] != 0) {
            keycode  = ascii_to_hid[ch][0];
            modifier = ascii_to_hid[ch][1];
        } else {
            continue;  /* Skip unmapped characters */
        }

        /* Press key */
        if (!hid_wait_ready(200)) return typed;
        uint8_t keycodes[6] = { keycode, 0, 0, 0, 0, 0 };
        tud_hid_keyboard_report(0, modifier, keycodes);

        /* Small delay between press and release */
        uint32_t start = board_millis();
        while ((board_millis() - start) < 10) {
            tud_task();
        }

        /* Release key */
        if (!hid_wait_ready(200)) return typed;
        tud_hid_keyboard_report(0, 0, NULL);

        /* Inter-key delay */
        start = board_millis();
        while ((board_millis() - start) < 10) {
            tud_task();
        }

        typed++;
    }

    usb_tx_bytes += typed * 8;
    return typed;
#else
    return (int)strlen(str);
#endif
}

/* ------------------------------------------------------------------ */
/*  HID Mouse functions                                                */
/* ------------------------------------------------------------------ */

int usb_hid_mouse_move(int8_t x, int8_t y, int8_t wheel) {
    if (usb_current_mode != USB_MODE_HID) return -1;

#if HAS_TINYUSB
    if (!tud_mounted()) return -1;
    if (!hid_wait_ready(100)) return -1;

    tud_hid_mouse_report(0, 0, x, y, wheel, 0);
    usb_tx_bytes += 5;
    return 0;
#else
    (void)x; (void)y; (void)wheel;
    return 0;
#endif
}

int usb_hid_mouse_click(uint8_t buttons) {
    if (usb_current_mode != USB_MODE_HID) return -1;

#if HAS_TINYUSB
    if (!tud_mounted()) return -1;

    /* Press */
    if (!hid_wait_ready(100)) return -1;
    tud_hid_mouse_report(0, buttons, 0, 0, 0, 0);

    /* Brief hold */
    uint32_t start = board_millis();
    while ((board_millis() - start) < 20) {
        tud_task();
    }

    /* Release */
    if (!hid_wait_ready(100)) return -1;
    tud_hid_mouse_report(0, 0, 0, 0, 0, 0);

    usb_tx_bytes += 10;
    return 0;
#else
    (void)buttons;
    return 0;
#endif
}

/* ------------------------------------------------------------------ */
/*  MSC (Mass Storage) functions                                       */
/* ------------------------------------------------------------------ */

int usb_msc_set_storage(const uint8_t *data, uint32_t block_count,
                        uint16_t block_size) {
    if (!data || block_count == 0 || block_size == 0) return -1;

    msc_storage_data = data;
    msc_block_count  = block_count;
    msc_block_size   = block_size;
    msc_ejected      = false;

    dmesg_info("usb: MSC storage set, %lu blocks x %u bytes",
               (unsigned long)block_count, block_size);
    return 0;
}

bool usb_msc_is_ejected(void) {
    return msc_ejected;
}

/* ------------------------------------------------------------------ */
/*  TinyUSB MSC callbacks (PICO_BUILD only)                            */
/* ------------------------------------------------------------------ */

#if HAS_TINYUSB

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id,   "littleOS",  8);
    memcpy(product_id,  "Flash Storage   ", 16);
    memcpy(product_rev, "0.4 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    if (msc_ejected) return false;
    return (msc_storage_data != NULL);
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
    (void)lun;
    *block_count = msc_block_count;
    *block_size  = msc_block_size;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                           bool start, bool load_eject) {
    (void)lun;
    (void)power_condition;

    if (load_eject) {
        if (!start) {
            msc_ejected = true;
            dmesg_info("usb: MSC ejected by host");
        }
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
    (void)lun;

    if (!msc_storage_data) return -1;
    if (lba >= msc_block_count) return -1;

    uint32_t addr = lba * msc_block_size + offset;
    memcpy(buffer, msc_storage_data + addr, bufsize);
    usb_tx_bytes += bufsize;

    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
    (void)lun;

    if (!msc_storage_data) return -1;
    if (lba >= msc_block_count) return -1;

    /* Write into the storage buffer (cast away const for writable storage) */
    uint32_t addr = lba * msc_block_size + offset;
    memcpy((uint8_t *)msc_storage_data + addr, buffer, bufsize);
    usb_rx_bytes += bufsize;

    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void *buffer, uint16_t bufsize) {
    (void)lun;
    (void)buffer;
    (void)bufsize;

    /* Handle only mandatory SCSI commands; reject everything else */
    switch (scsi_cmd[0]) {
        case 0x1E: /* Prevent/Allow Medium Removal */
            return 0;
        default:
            /* Unsupported command */
            tud_msc_set_sense(lun, 0x05, 0x20, 0x00); /* Illegal Request */
            return -1;
    }
}

/* ------------------------------------------------------------------ */
/*  TinyUSB HID callback (required by TinyUSB)                         */
/* ------------------------------------------------------------------ */

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/* ------------------------------------------------------------------ */
/*  TinyUSB device callbacks                                           */
/* ------------------------------------------------------------------ */

void tud_mount_cb(void) {
    usb_mounted   = true;
    usb_connected = true;
    dmesg_info("usb: device mounted");
}

void tud_umount_cb(void) {
    usb_mounted = false;
    dmesg_info("usb: device unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    usb_suspended = true;
    dmesg_info("usb: device suspended");
}

void tud_resume_cb(void) {
    usb_suspended = false;
    dmesg_info("usb: device resumed");
}

#endif /* PICO_BUILD */
