/* tusb_config.h - TinyUSB configuration for USB Host mode
 *
 * This file is used when LITTLEOS_USB_HOST is enabled.
 * USB host mode allows connecting a standard USB keyboard.
 * It is mutually exclusive with USB device mode (CDC/HID/MSC).
 *
 * Note: This file must be named tusb_config.h when installed via
 * CMake target_include_directories so TinyUSB can find it.
 */
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Common ---- */
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS             OPT_OS_NONE
#endif
#define CFG_TUSB_DEBUG          0

#ifndef CFG_TUH_MEM_SECTION
#define CFG_TUH_MEM_SECTION
#endif

#ifndef CFG_TUH_MEM_ALIGN
#define CFG_TUH_MEM_ALIGN      __attribute__((aligned(4)))
#endif

/* ---- Host Configuration ---- */
#define CFG_TUH_ENABLED        1
#define CFG_TUD_ENABLED        0     /* No device mode */

#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT       0
#endif

#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED    OPT_MODE_DEFAULT_SPEED
#endif

#define CFG_TUH_MAX_SPEED      BOARD_TUH_MAX_SPEED

/* ---- Driver Configuration ---- */
#define CFG_TUH_ENUMERATION_BUFSIZE  256

#define CFG_TUH_HUB            1     /* Support USB hubs (keyboard via hub) */
#define CFG_TUH_CDC            0
#define CFG_TUH_MSC            0
#define CFG_TUH_VENDOR         0
#define CFG_TUH_HID            4     /* Up to 4 HID interfaces */

#define CFG_TUH_DEVICE_MAX     (3 * CFG_TUH_HUB + 1)

/* ---- HID ---- */
#define CFG_TUH_HID_EPIN_BUFSIZE   64
#define CFG_TUH_HID_EPOUT_BUFSIZE  64

#ifdef __cplusplus
}
#endif
#endif /* _TUSB_CONFIG_H_ */
