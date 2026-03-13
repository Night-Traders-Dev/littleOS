/* module_builtins.c - Built-in module registration for littleOS
 *
 * Each built-in driver provides a registration function that calls
 * module_register(). Add new built-in modules here.
 */
#include "module.h"

/* ---- Built-in module registration functions ---- */
extern void ssd1306_module_register(void);
extern void sh1107_module_register(void);
#if LITTLEOS_HAS_HSTX
extern void dvi_console_module_register(void);
#endif
#ifdef LITTLEOS_USB_HOST
extern void usb_host_module_register(void);
#endif

void module_register_builtins(void) {
    /* Display drivers */
    ssd1306_module_register();
    sh1107_module_register();

#if LITTLEOS_HAS_HSTX
    /* DVI text console (RP2350 HSTX) */
    dvi_console_module_register();
#endif

#ifdef LITTLEOS_USB_HOST
    /* USB HID keyboard (host mode) */
    usb_host_module_register();
#endif
}
