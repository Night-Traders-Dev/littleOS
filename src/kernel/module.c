/* module.c - Kernel Module Framework for littleOS
 *
 * Manages registration, loading, and unloading of kernel modules.
 */
#include <stdio.h>
#include <string.h>
#include "module.h"
#include "dmesg.h"

/* ================================================================
 * Module registry
 * ================================================================ */

static module_t *registry[MODULE_MAX_REGISTERED];
static int        registry_count = 0;
static bool       subsys_ready   = false;

void module_subsys_init(void) {
    registry_count = 0;
    memset(registry, 0, sizeof(registry));
    subsys_ready = true;

    /* Register all built-in modules */
    module_register_builtins();

    dmesg_info("Module subsystem initialized (%d built-in)", registry_count);
}

int module_register(module_t *mod) {
    if (!mod || !mod->name) return -1;
    if (registry_count >= MODULE_MAX_REGISTERED) {
        dmesg_warn("Module registry full, cannot register '%s'", mod->name);
        return -1;
    }

    /* Check for duplicate */
    for (int i = 0; i < registry_count; i++) {
        if (registry[i] && strcmp(registry[i]->name, mod->name) == 0) {
            dmesg_warn("Module '%s' already registered", mod->name);
            return -1;
        }
    }

    mod->state = MODULE_STATE_REGISTERED;
    mod->priv  = NULL;
    registry[registry_count++] = mod;
    return 0;
}

int module_unregister(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < registry_count; i++) {
        if (registry[i] && strcmp(registry[i]->name, name) == 0) {
            if (registry[i]->state == MODULE_STATE_LOADED) {
                return -1;  /* Must unload first */
            }
            /* Shift remaining entries */
            for (int j = i; j < registry_count - 1; j++)
                registry[j] = registry[j + 1];
            registry[--registry_count] = NULL;
            return 0;
        }
    }
    return -1;
}

module_t *module_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < registry_count; i++) {
        if (registry[i] && strcmp(registry[i]->name, name) == 0)
            return registry[i];
    }
    return NULL;
}

int module_load(const char *name) {
    module_t *mod = module_find(name);
    if (!mod) {
        printf("Module '%s' not found\r\n", name);
        return -1;
    }

    if (mod->state == MODULE_STATE_LOADED) {
        printf("Module '%s' already loaded\r\n", name);
        return 0;
    }

    if (!mod->ops || !mod->ops->init) {
        printf("Module '%s' has no init function\r\n", name);
        return -1;
    }

    int ret = mod->ops->init(mod);
    if (ret == 0) {
        mod->state = MODULE_STATE_LOADED;
        dmesg_info("Module '%s' loaded", name);
    } else {
        mod->state = MODULE_STATE_ERROR;
        dmesg_warn("Module '%s' init failed (%d)", name, ret);
    }
    return ret;
}

int module_unload(const char *name) {
    module_t *mod = module_find(name);
    if (!mod) {
        printf("Module '%s' not found\r\n", name);
        return -1;
    }

    if (mod->state != MODULE_STATE_LOADED) {
        printf("Module '%s' not loaded\r\n", name);
        return -1;
    }

    if (mod->ops && mod->ops->deinit)
        mod->ops->deinit(mod);

    mod->state = MODULE_STATE_REGISTERED;
    mod->priv  = NULL;
    dmesg_info("Module '%s' unloaded", name);
    return 0;
}

int module_count(void) {
    return registry_count;
}

module_t *module_get(int index) {
    if (index < 0 || index >= registry_count)
        return NULL;
    return registry[index];
}

static const char *type_str(module_type_t t) {
    switch (t) {
        case MODULE_TYPE_DRIVER:   return "driver";
        case MODULE_TYPE_PROTOCOL: return "protocol";
        case MODULE_TYPE_SERVICE:  return "service";
        default:                   return "other";
    }
}

static const char *state_str(module_state_t s) {
    switch (s) {
        case MODULE_STATE_REGISTERED: return "registered";
        case MODULE_STATE_LOADED:     return "loaded";
        case MODULE_STATE_ERROR:      return "error";
        default:                      return "?";
    }
}

void module_list_all(void) {
    if (registry_count == 0) {
        printf("No modules registered\r\n");
        return;
    }

    printf("%-16s %-10s %-8s %s\r\n", "MODULE", "TYPE", "STATE", "DESCRIPTION");
    printf("%-16s %-10s %-8s %s\r\n", "------", "----", "-----", "-----------");

    for (int i = 0; i < registry_count; i++) {
        module_t *m = registry[i];
        if (!m) continue;
        printf("%-16s %-10s %-8s %s\r\n",
               m->name,
               type_str(m->type),
               state_str(m->state),
               m->description ? m->description : "");
    }

    printf("\r\n%d module(s) registered\r\n", registry_count);
}
