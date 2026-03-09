/* usb_device.h - USB Device Mode HAL for littleOS */
#ifndef LITTLEOS_HAL_USB_DEVICE_H
#define LITTLEOS_HAL_USB_DEVICE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* USB device modes */
typedef enum {
    USB_MODE_NONE = 0,
    USB_MODE_CDC,       /* Virtual serial port */
    USB_MODE_HID,       /* Keyboard/Mouse/Gamepad */
    USB_MODE_MSC,       /* Mass storage */
} usb_device_mode_t;

/* HID report types */
typedef enum {
    USB_HID_KEYBOARD = 0,
    USB_HID_MOUSE,
    USB_HID_GAMEPAD,
} usb_hid_type_t;

/* USB device status */
typedef struct {
    usb_device_mode_t mode;
    bool     connected;
    bool     mounted;
    bool     suspended;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    uint16_t vid;
    uint16_t pid;
} usb_device_status_t;

/* Initialize USB device subsystem */
int usb_device_init(void);

/* Set device mode (must be called before init, or requires re-init) */
int usb_device_set_mode(usb_device_mode_t mode);

/* Get current status */
int usb_device_get_status(usb_device_status_t *status);

/* Process USB tasks (call periodically from main loop) */
void usb_device_task(void);

/* CDC (Virtual Serial) functions */
int usb_cdc_write(const uint8_t *data, size_t len);
int usb_cdc_read(uint8_t *data, size_t max_len);
int usb_cdc_available(void);
int usb_cdc_write_str(const char *str);

/* HID functions */
int usb_hid_keyboard_press(uint8_t keycode, uint8_t modifier);
int usb_hid_keyboard_release(void);
int usb_hid_keyboard_type(const char *str);
int usb_hid_mouse_move(int8_t x, int8_t y, int8_t wheel);
int usb_hid_mouse_click(uint8_t buttons);

/* MSC functions - expose flash FS as USB drive */
int usb_msc_set_storage(const uint8_t *data, uint32_t block_count, uint16_t block_size);
bool usb_msc_is_ejected(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_USB_DEVICE_H */
