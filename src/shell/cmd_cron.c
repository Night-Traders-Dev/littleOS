/* cmd_cron.c - Shell commands for the cron/scheduled tasks system */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cron.h"

/* ============================================================================
 * Usage / Help
 * ========================================================================== */

static void cmd_cron_usage(void)
{
    printf("Cron (scheduled tasks) commands:\r\n");
    printf("  cron list                            - List all cron jobs\r\n");
    printf("  cron add <name> <interval_sec> <cmd> - Add repeating job\r\n");
    printf("  cron once <name> <delay_sec> <cmd>   - Add one-shot job\r\n");
    printf("  cron remove <name>                   - Remove a job\r\n");
    printf("  cron enable <name>                   - Enable a job\r\n");
    printf("  cron disable <name>                  - Disable a job\r\n");
    printf("  cron status                          - Cron system summary\r\n");
    printf("\r\n");
    printf("Examples:\r\n");
    printf("  cron add temp 60 \"sensor read temp\"\r\n");
    printf("  cron once boot_msg 5 \"echo boot complete\"\r\n");
}

/* ============================================================================
 * Sub-commands
 * ========================================================================== */

static int cmd_cron_list(void)
{
    char buf[1024];
    cron_list(buf, sizeof(buf));
    printf("%s", buf);
    return 0;
}

static int cmd_cron_add(int argc, char *argv[])
{
    /* cron add <name> <interval_sec> <command...> */
    if (argc < 5) {
        printf("Usage: cron add <name> <interval_sec> <command>\r\n");
        return -1;
    }

    const char *name = argv[2];
    uint32_t interval_sec = (uint32_t)atoi(argv[3]);
    if (interval_sec == 0) {
        printf("Error: interval must be > 0 seconds\r\n");
        return -1;
    }

    /* Reconstruct command from remaining args (handles quoted or unquoted) */
    char command[128];
    command[0] = '\0';
    for (int i = 4; i < argc; i++) {
        if (i > 4)
            strncat(command, " ", sizeof(command) - strlen(command) - 1);
        strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }

    uint32_t interval_ms = interval_sec * 1000;
    int rc = cron_add(name, command, interval_ms);
    if (rc == 0) {
        printf("Cron job '%s' added: every %lus -> \"%s\"\r\n",
               name, (unsigned long)interval_sec, command);
    } else {
        printf("Error: could not add job '%s' (duplicate name or table full)\r\n", name);
    }
    return rc;
}

static int cmd_cron_once(int argc, char *argv[])
{
    /* cron once <name> <delay_sec> <command...> */
    if (argc < 5) {
        printf("Usage: cron once <name> <delay_sec> <command>\r\n");
        return -1;
    }

    const char *name = argv[2];
    uint32_t delay_sec = (uint32_t)atoi(argv[3]);
    if (delay_sec == 0) {
        printf("Error: delay must be > 0 seconds\r\n");
        return -1;
    }

    char command[128];
    command[0] = '\0';
    for (int i = 4; i < argc; i++) {
        if (i > 4)
            strncat(command, " ", sizeof(command) - strlen(command) - 1);
        strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }

    uint32_t delay_ms = delay_sec * 1000;
    int rc = cron_add_oneshot(name, command, delay_ms);
    if (rc == 0) {
        printf("One-shot job '%s' added: in %lus -> \"%s\"\r\n",
               name, (unsigned long)delay_sec, command);
    } else {
        printf("Error: could not add job '%s' (duplicate name or table full)\r\n", name);
    }
    return rc;
}

static int cmd_cron_remove(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: cron remove <name>\r\n");
        return -1;
    }

    int rc = cron_remove(argv[2]);
    if (rc == 0)
        printf("Cron job '%s' removed\r\n", argv[2]);
    else
        printf("Error: job '%s' not found\r\n", argv[2]);
    return rc;
}

static int cmd_cron_enable(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: cron enable <name>\r\n");
        return -1;
    }

    int rc = cron_enable(argv[2]);
    if (rc == 0)
        printf("Cron job '%s' enabled\r\n", argv[2]);
    else
        printf("Error: job '%s' not found\r\n", argv[2]);
    return rc;
}

static int cmd_cron_disable(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: cron disable <name>\r\n");
        return -1;
    }

    int rc = cron_disable(argv[2]);
    if (rc == 0)
        printf("Cron job '%s' disabled\r\n", argv[2]);
    else
        printf("Error: job '%s' not found\r\n", argv[2]);
    return rc;
}

static int cmd_cron_status(void)
{
    int count = cron_get_count();
    printf("\r\n=== littleOS Cron System ===\r\n");
    printf("Jobs: %d / %d\r\n", count, CRON_MAX_JOBS);
    printf("===========================\r\n\r\n");

    if (count > 0)
        cmd_cron_list();

    return 0;
}

/* ============================================================================
 * Main Command Dispatcher
 * ========================================================================== */

int cmd_cron(int argc, char *argv[])
{
    if (argc < 2) {
        cmd_cron_usage();
        return 0;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "list") == 0)
        return cmd_cron_list();
    else if (strcmp(sub, "add") == 0)
        return cmd_cron_add(argc, argv);
    else if (strcmp(sub, "once") == 0)
        return cmd_cron_once(argc, argv);
    else if (strcmp(sub, "remove") == 0)
        return cmd_cron_remove(argc, argv);
    else if (strcmp(sub, "enable") == 0)
        return cmd_cron_enable(argc, argv);
    else if (strcmp(sub, "disable") == 0)
        return cmd_cron_disable(argc, argv);
    else if (strcmp(sub, "status") == 0)
        return cmd_cron_status();
    else if (strcmp(sub, "help") == 0) {
        cmd_cron_usage();
        return 0;
    } else {
        printf("Unknown subcommand: %s\r\n", sub);
        cmd_cron_usage();
        return -1;
    }
}
