/* module_builtins.c - Built-in module registration for littleOS
 *
 * Each built-in driver provides a registration function that calls
 * module_register(). Add new built-in modules here.
 */
#include "module.h"

/* ---- Built-in module registration functions ---- */
extern void ssd1306_module_register(void);
extern void sh1107_module_register(void);

void module_register_builtins(void) {
    /* Display drivers */
    ssd1306_module_register();
    sh1107_module_register();

    /* Add new built-in modules here:
     *   my_driver_module_register();
     */
}
