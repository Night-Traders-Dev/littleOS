/* usb_host.h - USB Host Mode (HID Keyboard) for littleOS
 *
 * Enables USB host mode to accept keyboard input from a standard
 * USB keyboard plugged into the Pico's USB port.
 *
 * Note: USB host mode and USB device mode (CDC/HID/MSC) are mutually
 * exclusive — the RP2040/RP2350 has a single USB controller.
 * When USB host is enabled, USB stdio and device features are unavailable.
 *
 * The keyboard driver provides characters to the shell via a ring buffer
 * that integrates with the Pico SDK's stdio system.
 */
#ifndef LITTLEOS_HAL_USB_HOST_H
#define LITTLEOS_HAL_USB_HOST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize USB host mode and start listening for keyboards.
 * Returns 0 on success, -1 on failure. */
int usb_host_init(void);

/* Shut down USB host mode. */
void usb_host_deinit(void);

/* Process USB host tasks. Must be called periodically (from main loop).
 * This processes USB events and delivers keystrokes to the input buffer. */
void usb_host_task(void);

/* Check if a keyboard is connected. */
bool usb_host_keyboard_connected(void);

/* Check if USB host mode is active. */
bool usb_host_is_active(void);

/* Install the USB keyboard as a stdio input source.
 * When installed, getchar_timeout_us() will also check the USB keyboard. */
void usb_host_install_stdio(void);

/* Remove the USB keyboard from stdio. */
void usb_host_remove_stdio(void);

#ifdef __cplusplus
}
#endif
#endif /* LITTLEOS_HAL_USB_HOST_H */
