/* cmd_logcat.c - Shell commands for the structured logging system */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logcat.h"

/* ============================================================================
 * Usage / Help
 * ========================================================================== */

static void cmd_logcat_usage(void)
{
    printf("Logcat (structured logging) commands:\r\n");
    printf("  logcat                - Show recent logs (last 20)\r\n");
    printf("  logcat tail           - Show recent logs (last 20)\r\n");
    printf("  logcat all            - Show all log entries\r\n");
    printf("  logcat clear          - Clear the log buffer\r\n");
    printf("  logcat level [LEVEL]  - Get/set minimum log level\r\n");
    printf("  logcat filter TAG     - Show entries matching TAG\r\n");
    printf("  logcat count          - Show log entry count\r\n");
    printf("  logcat test           - Inject test entries at each level\r\n");
    printf("\r\n");
    printf("Levels: VERBOSE, DEBUG, INFO, WARN, ERROR, FATAL\r\n");
    printf("        (or V, D, I, W, E, F)\r\n");
}

/* ============================================================================
 * Internal Helpers
 * ========================================================================== */

/* Shared dump buffer - avoids malloc, sized for typical shell output */
#define CMD_LOGCAT_BUFSIZE  4096
static char dump_buf[CMD_LOGCAT_BUFSIZE];

/* Print the tail (last N) of the log buffer by dumping all and trimming */
static void cmd_logcat_tail(int tail_count)
{
    int total = logcat_dump(dump_buf, sizeof(dump_buf), LOG_VERBOSE, NULL);
    if (total == 0) {
        printf("(no log entries)\r\n");
        return;
    }

    /* Count the lines and find where to start printing */
    int lines = 0;
    const char *p = dump_buf;
    const char *line_starts[LOGCAT_MAX_ENTRIES];

    while (*p) {
        if (lines < LOGCAT_MAX_ENTRIES)
            line_starts[lines] = p;
        lines++;
        /* Advance to next line */
        const char *next = strstr(p, "\r\n");
        if (!next)
            break;
        p = next + 2;
    }

    int skip = (lines > tail_count) ? lines - tail_count : 0;
    for (int i = skip; i < lines; i++) {
        /* Print line by line */
        const char *start = line_starts[i];
        const char *end = strstr(start, "\r\n");
        if (end) {
            /* Print the line including \r\n */
            printf("%.*s\r\n", (int)(end - start), start);
        } else {
            printf("%s\r\n", start);
        }
    }
}

/* ============================================================================
 * Sub-commands
 * ========================================================================== */

static int cmd_logcat_show_all(void)
{
    int count = logcat_dump(dump_buf, sizeof(dump_buf), LOG_VERBOSE, NULL);
    if (count == 0) {
        printf("(no log entries)\r\n");
    } else {
        printf("%s", dump_buf);
    }
    return 0;
}

static int cmd_logcat_do_clear(void)
{
    logcat_clear();
    printf("Log buffer cleared.\r\n");
    return 0;
}

static int cmd_logcat_do_level(int argc, char *argv[])
{
    if (argc < 3) {
        /* Show current level */
        log_level_t cur = logcat_get_level();
        printf("Current minimum level: %s\r\n", logcat_level_str(cur));
        return 0;
    }

    log_level_t new_level = logcat_parse_level(argv[2]);
    if (logcat_set_level(new_level) == 0) {
        printf("Log level set to: %s\r\n", logcat_level_str(new_level));
    } else {
        printf("Invalid log level.\r\n");
        return -1;
    }
    return 0;
}

static int cmd_logcat_do_filter(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: logcat filter TAG\r\n");
        return -1;
    }

    int count = logcat_dump(dump_buf, sizeof(dump_buf), LOG_VERBOSE, argv[2]);
    if (count == 0) {
        printf("(no entries matching '%s')\r\n", argv[2]);
    } else {
        printf("%s", dump_buf);
    }
    return 0;
}

static int cmd_logcat_do_count(void)
{
    printf("Log entries: %d / %d\r\n", logcat_get_count(), LOGCAT_MAX_ENTRIES);
    return 0;
}

static int cmd_logcat_do_test(void)
{
    printf("Injecting test log entries...\r\n");
    logcat_log(LOG_VERBOSE, "test", "Verbose test message");
    logcat_log(LOG_DEBUG,   "test", "Debug test message");
    logcat_log(LOG_INFO,    "test", "Info test message");
    logcat_log(LOG_WARN,    "test", "Warning test message");
    logcat_log(LOG_ERROR,   "test", "Error test message");
    logcat_log(LOG_FATAL,   "test", "Fatal test message");
    printf("Injected 6 test entries (tag: test).\r\n");
    return 0;
}

/* ============================================================================
 * Main Command Entry Point
 * ========================================================================== */

int cmd_logcat(int argc, char *argv[])
{
    /* No arguments or "tail" => show last 20 */
    if (argc < 2 || strcmp(argv[1], "tail") == 0) {
        cmd_logcat_tail(20);
        return 0;
    }

    if (strcmp(argv[1], "all") == 0)
        return cmd_logcat_show_all();

    if (strcmp(argv[1], "clear") == 0)
        return cmd_logcat_do_clear();

    if (strcmp(argv[1], "level") == 0)
        return cmd_logcat_do_level(argc, argv);

    if (strcmp(argv[1], "filter") == 0)
        return cmd_logcat_do_filter(argc, argv);

    if (strcmp(argv[1], "count") == 0)
        return cmd_logcat_do_count();

    if (strcmp(argv[1], "test") == 0)
        return cmd_logcat_do_test();

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
        cmd_logcat_usage();
        return 0;
    }

    printf("Unknown logcat subcommand: %s\r\n", argv[1]);
    cmd_logcat_usage();
    return -1;
}
