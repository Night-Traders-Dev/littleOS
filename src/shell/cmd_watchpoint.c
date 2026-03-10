/* cmd_watchpoint.c - Shell commands for the memory watchpoint system */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "watchpoint.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/* ============================================================================
 * Usage / Help
 * ========================================================================== */

static void cmd_watchpoint_usage(void)
{
    printf("Memory watchpoint commands:\r\n");
    printf("  watchpoint add ADDR [SIZE] [LABEL]  - Add watchpoint (ADDR in hex)\r\n");
    printf("  watchpoint remove INDEX              - Remove watchpoint by index\r\n");
    printf("  watchpoint list                      - List all watchpoints\r\n");
    printf("  watchpoint check                     - Manually trigger value check\r\n");
    printf("  watchpoint clear                     - Remove all watchpoints\r\n");
    printf("\r\n");
    printf("Examples:\r\n");
    printf("  watchpoint add 0x20000000\r\n");
    printf("  watchpoint add 0x20000100 4 my_var\r\n");
    printf("  watchpoint remove 0\r\n");
}

/* ============================================================================
 * Sub-commands
 * ========================================================================== */

static int cmd_watchpoint_add(int argc, char *argv[])
{
    /* watchpoint add ADDR [SIZE] [LABEL] */
    if (argc < 3) {
        printf("Usage: watchpoint add ADDR [SIZE] [LABEL]\r\n");
        return -1;
    }

    uint32_t addr = (uint32_t)strtoul(argv[2], NULL, 0);
    uint32_t size = 4;  /* default 4 bytes */
    const char *label = NULL;

    if (argc >= 4)
        size = (uint32_t)strtoul(argv[3], NULL, 0);

    if (argc >= 5)
        label = argv[4];

    if (size != 1 && size != 2 && size != 4) {
        printf("Invalid size: %u (must be 1, 2, or 4)\r\n", (unsigned)size);
        return -1;
    }

    int slot = watchpoint_add(addr, size, WP_TYPE_VALUE, label);
    if (slot < 0) {
        printf("Failed to add watchpoint (table full or invalid params).\r\n");
        return -1;
    }

    printf("Watchpoint [%d] added: addr=0x%08X size=%u\r\n",
           slot, (unsigned)addr, (unsigned)size);
    return 0;
}

static int cmd_watchpoint_remove(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: watchpoint remove INDEX\r\n");
        return -1;
    }

    int index = atoi(argv[2]);
    if (watchpoint_remove(index) < 0) {
        printf("Failed to remove watchpoint %d.\r\n", index);
        return -1;
    }

    printf("Watchpoint [%d] removed.\r\n", index);
    return 0;
}

static int cmd_watchpoint_list(void)
{
    static char list_buf[1024];
    int n = watchpoint_list(list_buf, sizeof(list_buf));
    if (n > 0)
        printf("%s", list_buf);
    printf("(%d active watchpoint%s)\r\n",
           watchpoint_get_count(),
           watchpoint_get_count() == 1 ? "" : "s");
    return 0;
}

static int cmd_watchpoint_check(void)
{
    printf("Checking watchpoints...\r\n");
    watchpoint_check();
    printf("Check complete.\r\n");
    return 0;
}

static int cmd_watchpoint_clear(void)
{
    watchpoint_init();
    printf("All watchpoints cleared.\r\n");
    return 0;
}

/* ============================================================================
 * Main command dispatcher
 * ========================================================================== */

int cmd_watchpoint(int argc, char *argv[])
{
    if (argc < 2) {
        cmd_watchpoint_usage();
        return 0;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "add") == 0)          return cmd_watchpoint_add(argc, argv);
    else if (strcmp(sub, "remove") == 0)  return cmd_watchpoint_remove(argc, argv);
    else if (strcmp(sub, "list") == 0)    return cmd_watchpoint_list();
    else if (strcmp(sub, "check") == 0)   return cmd_watchpoint_check();
    else if (strcmp(sub, "clear") == 0)   return cmd_watchpoint_clear();
    else {
        printf("Unknown watchpoint subcommand: %s\r\n", sub);
        cmd_watchpoint_usage();
        return -1;
    }
}
