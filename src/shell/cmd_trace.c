/* cmd_trace.c - Shell commands for the execution trace system */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "trace.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/* ============================================================================
 * Usage / Help
 * ========================================================================== */

static void cmd_trace_usage(void)
{
    printf("Execution trace commands:\r\n");
    printf("  trace enable        - Enable trace recording\r\n");
    printf("  trace disable       - Disable trace recording\r\n");
    printf("  trace status        - Show trace status\r\n");
    printf("  trace dump          - Dump all trace entries\r\n");
    printf("  trace clear         - Clear trace buffer\r\n");
    printf("  trace test          - Inject sample trace events\r\n");
}

/* ============================================================================
 * Sub-commands
 * ========================================================================== */

static int cmd_trace_enable(void)
{
    trace_enable(true);
    printf("Trace recording enabled.\r\n");
    return 0;
}

static int cmd_trace_disable(void)
{
    trace_enable(false);
    printf("Trace recording disabled.\r\n");
    return 0;
}

static int cmd_trace_status(void)
{
    printf("Trace status:\r\n");
    printf("  Enabled:  %s\r\n", trace_is_enabled() ? "yes" : "no");
    printf("  Entries:  %d / %d\r\n", trace_get_count(), TRACE_MAX_ENTRIES);
    return 0;
}

static int cmd_trace_dump(void)
{
    int count = trace_get_count();
    if (count == 0) {
        printf("Trace buffer is empty.\r\n");
        return 0;
    }

    /* Allocate a static buffer for dump output */
    static char dump_buf[4096];
    int n = trace_dump(dump_buf, sizeof(dump_buf));
    if (n > 0) {
        printf("%s", dump_buf);
    }
    printf("(%d entries)\r\n", count);
    return 0;
}

static int cmd_trace_clear(void)
{
    trace_clear();
    printf("Trace buffer cleared.\r\n");
    return 0;
}

static int cmd_trace_test(void)
{
    bool was_enabled = trace_is_enabled();
    trace_enable(true);

    trace_record(TRACE_ENTER, "main", 0);
    trace_record(TRACE_SCHED, "task_idle", 0);
    trace_record(TRACE_SCHED, "task_shell", 1);
    trace_record(TRACE_ISR,   "UART0_IRQ", 20);
    trace_record(TRACE_EVENT, "mem_alloc", 256);
    trace_record(TRACE_ENTER, "shell_exec", 0);
    trace_record(TRACE_EXIT,  "shell_exec", 0);
    trace_record(TRACE_EVENT, "gpio_set", 25);
    trace_record(TRACE_EXIT,  "main", 0);

    trace_enable(was_enabled);

    printf("Injected 9 sample trace events.\r\n");
    return 0;
}

/* ============================================================================
 * Main command dispatcher
 * ========================================================================== */

int cmd_trace(int argc, char *argv[])
{
    if (argc < 2) {
        cmd_trace_usage();
        return 0;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "enable") == 0)       return cmd_trace_enable();
    else if (strcmp(sub, "disable") == 0) return cmd_trace_disable();
    else if (strcmp(sub, "status") == 0)  return cmd_trace_status();
    else if (strcmp(sub, "dump") == 0)    return cmd_trace_dump();
    else if (strcmp(sub, "clear") == 0)   return cmd_trace_clear();
    else if (strcmp(sub, "test") == 0)    return cmd_trace_test();
    else {
        printf("Unknown trace subcommand: %s\r\n", sub);
        cmd_trace_usage();
        return -1;
    }
}
