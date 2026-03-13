/* cmd_mod.c - Module management shell commands for littleOS */
#include <stdio.h>
#include <string.h>
#include "module.h"

int cmd_mod(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: mod <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  list              - List all registered modules\r\n");
        printf("  load <name>       - Load (initialize) a module\r\n");
        printf("  unload <name>     - Unload a module\r\n");
        printf("  info <name>       - Show module details\r\n");
        printf("  status <name>     - Show module runtime status\r\n");
        return 0;
    }

    /* ---- list ---- */
    if (strcmp(argv[1], "list") == 0 || strcmp(argv[1], "ls") == 0) {
        module_list_all();
        return 0;
    }

    /* ---- load ---- */
    if (strcmp(argv[1], "load") == 0) {
        if (argc < 3) {
            printf("Usage: mod load <module_name>\r\n");
            return 1;
        }
        int ret = module_load(argv[2]);
        if (ret == 0)
            printf("Module '%s' loaded successfully\r\n", argv[2]);
        return ret;
    }

    /* ---- unload ---- */
    if (strcmp(argv[1], "unload") == 0) {
        if (argc < 3) {
            printf("Usage: mod unload <module_name>\r\n");
            return 1;
        }
        int ret = module_unload(argv[2]);
        if (ret == 0)
            printf("Module '%s' unloaded\r\n", argv[2]);
        return ret;
    }

    /* ---- info ---- */
    if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) {
            printf("Usage: mod info <module_name>\r\n");
            return 1;
        }
        module_t *mod = module_find(argv[2]);
        if (!mod) {
            printf("Module '%s' not found\r\n", argv[2]);
            return 1;
        }

        const char *type_names[] = { "driver", "protocol", "service", "other" };
        const char *state_names[] = { "registered", "loaded", "error" };

        printf("Module: %s\r\n", mod->name);
        printf("  Description: %s\r\n", mod->description ? mod->description : "(none)");
        printf("  Version:     %s\r\n", mod->version ? mod->version : "(none)");
        printf("  Type:        %s\r\n", type_names[mod->type]);
        printf("  State:       %s\r\n", state_names[mod->state]);
        printf("  Operations:  ");
        if (mod->ops) {
            if (mod->ops->init)   printf("init ");
            if (mod->ops->deinit) printf("deinit ");
            if (mod->ops->read)   printf("read ");
            if (mod->ops->write)  printf("write ");
            if (mod->ops->ioctl)  printf("ioctl ");
            if (mod->ops->status) printf("status ");
        }
        printf("\r\n");
        return 0;
    }

    /* ---- status ---- */
    if (strcmp(argv[1], "status") == 0) {
        if (argc < 3) {
            printf("Usage: mod status <module_name>\r\n");
            return 1;
        }
        module_t *mod = module_find(argv[2]);
        if (!mod) {
            printf("Module '%s' not found\r\n", argv[2]);
            return 1;
        }
        if (mod->ops && mod->ops->status) {
            mod->ops->status(mod);
        } else {
            printf("Module '%s' does not support status\r\n", mod->name);
        }
        return 0;
    }

    printf("Unknown command: mod %s\r\n", argv[1]);
    return 1;
}
