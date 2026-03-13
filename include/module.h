/* module.h - Kernel Module Framework for littleOS
 *
 * Provides a unified interface for loadable/built-in kernel modules.
 * Modules can be drivers, protocols, services, or any extensible
 * component that follows the standard lifecycle.
 *
 * Usage:
 *   1. Define a module_t with callbacks and metadata
 *   2. Register with module_register() during init, or
 *      use MODULE_BUILTIN() to auto-register at boot
 *   3. Load/unload via shell: mod load <name>, mod unload <name>
 */
#ifndef LITTLEOS_MODULE_H
#define LITTLEOS_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum modules the registry can hold */
#define MODULE_MAX_REGISTERED   32

/* Module categories */
typedef enum {
    MODULE_TYPE_DRIVER    = 0,   /* Hardware driver (display, sensor, etc.) */
    MODULE_TYPE_PROTOCOL  = 1,   /* Communication protocol */
    MODULE_TYPE_SERVICE   = 2,   /* System service */
    MODULE_TYPE_OTHER     = 3,
} module_type_t;

/* Module lifecycle state */
typedef enum {
    MODULE_STATE_REGISTERED = 0, /* Known to registry, not loaded */
    MODULE_STATE_LOADED     = 1, /* init() called successfully */
    MODULE_STATE_ERROR      = 2, /* init() failed or runtime error */
} module_state_t;

/* Forward declaration */
typedef struct module module_t;

/* Module operations — all optional (NULL = not supported) */
typedef struct {
    /* Lifecycle */
    int  (*init)(module_t *mod);           /* Probe/init hardware */
    void (*deinit)(module_t *mod);         /* Release resources */

    /* Generic I/O (for drivers) */
    int  (*read)(module_t *mod, void *buf, size_t len);
    int  (*write)(module_t *mod, const void *buf, size_t len);
    int  (*ioctl)(module_t *mod, int cmd, void *arg);

    /* Status/info */
    void (*status)(module_t *mod);         /* Print human-readable status */
} module_ops_t;

/* Module descriptor */
struct module {
    /* Identity (set by module author, immutable after registration) */
    const char      *name;          /* Short name, e.g. "ssd1306" */
    const char      *description;   /* Human-readable, e.g. "SSD1306 128x64 I2C OLED" */
    const char      *version;       /* e.g. "1.0.0" */
    module_type_t    type;

    /* Operations */
    const module_ops_t *ops;

    /* Runtime state (managed by framework) */
    module_state_t   state;
    void            *priv;          /* Module-private data (allocated by init) */
};

/* ---- Registry API ---- */

/* Initialize the module subsystem. Call once at boot. */
void module_subsys_init(void);

/* Register a module with the kernel. Returns 0 on success. */
int module_register(module_t *mod);

/* Unregister a module by name. Must be unloaded first.
 * Returns 0 on success, -1 if not found or still loaded. */
int module_unregister(const char *name);

/* Find a registered module by name. Returns NULL if not found. */
module_t *module_find(const char *name);

/* Load (init) a module. Returns 0 on success. */
int module_load(const char *name);

/* Unload (deinit) a module. Returns 0 on success. */
int module_unload(const char *name);

/* Get count of registered modules */
int module_count(void);

/* Get module at index (for iteration). Returns NULL if out of range. */
module_t *module_get(int index);

/* Print all registered modules and their status */
void module_list_all(void);

/* ---- Built-in module registration ---- */

/* Each built-in driver calls module_register() from its own
 * registration function. All registration functions are called
 * from module_register_builtins() during module_subsys_init().
 *
 * To add a new built-in module:
 *   1. Declare its registration function in module_builtins.h
 *   2. Call it from module_register_builtins()
 *   3. Add the source file to CMakeLists.txt
 */
void module_register_builtins(void);

#ifdef __cplusplus
}
#endif
#endif /* LITTLEOS_MODULE_H */
