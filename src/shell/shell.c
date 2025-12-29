#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "watchdog.h"
#include "supervisor.h"
#include "dmesg.h"
#include "littlefetch.h"

// Forward declarations
extern int  cmd_sage(int argc, char* argv[]);
extern int  cmd_script(int argc, char* argv[]);
extern void cmd_health(int argc, char** argv);
extern void cmd_stats(int argc, char** argv);
extern void cmd_supervisor(int argc, char** argv);
extern void cmd_dmesg(int argc, char** argv);
extern int  cmd_users(int argc, char *argv[]);
extern int  cmd_perms(int argc, char *argv[]);
extern int  cmd_tasks(int argc, char *argv[]);
extern int  cmd_memory(int argc, char *argv[]);
extern int  cmd_fs(int argc, char *argv[]);

// Command history settings
#define HISTORY_SIZE 20
#define MAX_CMD_LEN  512

static char history[HISTORY_SIZE][MAX_CMD_LEN];
static int  history_count = 0;
static int  history_pos   = 0;

// Simple argument parsing helper
static int parse_args(char* buffer, char* argv[], int max_args) {
    int   argc  = 0;
    char* token = strtok(buffer, " ");

    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token        = strtok(NULL, " ");
    }

    return argc;
}

// Add command to history
static void add_to_history(const char* cmd) {
    // Don't add empty commands or duplicates of the last command
    if (cmd[0] == '\0') return;
    if (history_count > 0 &&
        strcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0) {
        return;
    }

    // Add to circular buffer
    strncpy(history[history_count % HISTORY_SIZE], cmd, MAX_CMD_LEN - 1);
    history[history_count % HISTORY_SIZE][MAX_CMD_LEN - 1] = '\0';
    history_count++;

    // Reset history position
    history_pos = history_count;
}

// Get command from history
static const char* get_history(int offset) {
    if (history_count == 0) return NULL;

    int pos = history_pos + offset;

    // Clamp to valid range
    if (pos < 0)            pos = 0;
    if (pos > history_count) pos = history_count;

    history_pos = pos;

    // If at the end, return empty
    if (pos == history_count) return "";

    // Get from circular buffer
    int actual_pos = pos % HISTORY_SIZE;
    if (pos < history_count - HISTORY_SIZE) {
        // Wrapped around, adjust
        actual_pos =
            (history_count - HISTORY_SIZE + (pos % HISTORY_SIZE)) % HISTORY_SIZE;
    }

    return history[actual_pos];
}

// Clear current line in terminal
static void clear_line(int len) {
    // Move cursor back
    for (int i = 0; i < len; i++) {
        printf("\b");
    }

    // Clear with spaces
    for (int i = 0; i < len; i++) {
        printf(" ");
    }

    // Move cursor back again
    for (int i = 0; i < len; i++) {
        printf("\b");
    }

    fflush(stdout);
}

void shell_run() {
    char buffer[MAX_CMD_LEN];
    int  idx = 0;

    // ANSI escape sequence state machine
    enum {
        STATE_NORMAL,
        STATE_ESC,
        STATE_CSI
    } escape_state = STATE_NORMAL;

    // Track time for periodic watchdog feeding and supervisor heartbeat
    uint32_t last_wdt_feed =
        to_ms_since_boot(get_absolute_time());
    uint32_t last_heartbeat = last_wdt_feed;

    // Note: Welcome message is now displayed in kernel.c after boot
    printf(">");
    fflush(stdout);

    while (1) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Feed watchdog every 1 second while in shell loop
        if (now - last_wdt_feed >= 1000) {
            wdt_feed();
            last_wdt_feed = now;
        }

        // Send heartbeat to supervisor every 500ms
        if (now - last_heartbeat >= 500) {
            supervisor_heartbeat();
            last_heartbeat = now;
        }

        int c = getchar_timeout_us(0); // Non-blocking read
        if (c == PICO_ERROR_TIMEOUT) {
            sleep_ms(10);
            continue;
        }

        // Handle ANSI escape sequences for arrow keys
        switch (escape_state) {
        case STATE_NORMAL:
            if (c == 0x1B) { // ESC
                escape_state = STATE_ESC;
                continue;
            }
            break;

        case STATE_ESC:
            if (c == '[') {
                escape_state = STATE_CSI;
                continue;
            } else {
                escape_state = STATE_NORMAL;
            }
            break;

        case STATE_CSI:
            escape_state = STATE_NORMAL;
            if (c == 'A') { // Up arrow
                const char* cmd = get_history(-1);
                if (cmd != NULL) {
                    clear_line(idx);
                    strncpy(buffer, cmd, MAX_CMD_LEN - 1);
                    buffer[MAX_CMD_LEN - 1] = '\0';
                    idx = (int)strlen(buffer);
                    printf("%s", buffer);
                    fflush(stdout);
                }
                continue;
            } else if (c == 'B') { // Down arrow
                const char* cmd = get_history(1);
                if (cmd != NULL) {
                    clear_line(idx);
                    strncpy(buffer, cmd, MAX_CMD_LEN - 1);
                    buffer[MAX_CMD_LEN - 1] = '\0';
                    idx = (int)strlen(buffer);
                    printf("%s", buffer);
                    fflush(stdout);
                }
                continue;
            } else if (c == 'C') { // Right arrow
                continue;
            } else if (c == 'D') { // Left arrow
                continue;
            }
            break;
        }

        // Normal character processing
        if (c == '\r' || c == '\n') { // Enter key
            printf("\r\n");
            buffer[idx] = '\0';

            // Add to history
            add_to_history(buffer);

            // Parse command and arguments
            char* argv[32];

            // Make a copy for parsing since strtok modifies the string
            char buffer_copy[MAX_CMD_LEN];
            strncpy(buffer_copy, buffer, MAX_CMD_LEN);
            buffer_copy[MAX_CMD_LEN - 1] = '\0';

            int argc = parse_args(buffer_copy, argv, 32);

            if (argc > 0) {
                // Feed watchdog and send heartbeat before executing command
                wdt_feed();
                supervisor_heartbeat();

                if (strcmp(argv[0], "help") == 0) {
                    printf("Available commands:\r\n");
                    printf("  help        - Show this help message\r\n");
                    printf("  version     - Show OS version\r\n");
                    printf("  clear       - Clear the screen\r\n");
                    printf("  reboot      - Reboot the system\r\n");
                    printf("  history     - Show command history\r\n");
                    printf("  health      - Quick system health check\r\n");
                    printf("  stats       - Detailed system statistics\r\n");
                    printf("  supervisor  - Supervisor control (start/stop/status/alerts)\r\n");
                    printf("  dmesg       - View kernel message buffer (type 'dmesg --help')\r\n");
                    printf("  sage        - SageLang interpreter (type 'sage --help')\r\n");
                    printf("  script      - Script management (type 'script' for help)\r\n");
                    printf("  users       - User account management\r\n");
                    printf("  perms       - Permission and access control\r\n");
                    printf("  tasks       - Task management (scheduler)\r\n");
                    printf("  memory      - Memory diagnostics and tests\r\n");
                    printf("  fs          - RAM filesystem tools (type 'fs' for usage)\r\n");
                    printf("\r\nUse UP/DOWN arrows to navigate command history\r\n");
                } else if (strcmp(argv[0], "version") == 0) {
                    printf("littleOS v0.3.0 - RP2040\r\n");
                    printf("With SageLang v0.8.0\r\n");
                    printf("Supervisor: %s\r\n",
                           supervisor_is_running() ? "Active" : "Inactive");
                } else if (strcmp(argv[0], "clear") == 0) {
                    printf("\033[2J");
                    printf("\033[H");
                    printf(">");
                    fflush(stdout);
                    idx = 0;
                    continue;
                } else if (strcmp(argv[0], "history") == 0) {
                    printf("Command history:\r\n");
                    int start =
                        (history_count > HISTORY_SIZE)
                        ? history_count - HISTORY_SIZE
                        : 0;
                    for (int i = start; i < history_count; i++) {
                        printf(" %d: %s\r\n",
                               i + 1,
                               history[i % HISTORY_SIZE]);
                    }
                } else if (strcmp(argv[0], "health") == 0) {
                    cmd_health(argc, argv);
                } else if (strcmp(argv[0], "stats") == 0) {
                    cmd_stats(argc, argv);
                } else if (strcmp(argv[0], "supervisor") == 0) {
                    cmd_supervisor(argc, argv);
                } else if (strcmp(argv[0], "dmesg") == 0) {
                    cmd_dmesg(argc, argv);
                } else if (strcmp(argv[0], "users") == 0) {
                    cmd_users(argc, argv);
                } else if (strcmp(argv[0], "perms") == 0) {
                    cmd_perms(argc, argv);
                } else if (strcmp(argv[0], "tasks") == 0) {
                    cmd_tasks(argc, argv);
                } else if (strcmp(argv[0], "memory") == 0) {
                    cmd_memory(argc, argv);
                } else if (strcmp(argv[0], "fs") == 0) {
                    cmd_fs(argc, argv);
                } else if (strcmp(argv[0], "fetch") == 0) {
                    littlefetch();
                    return;
                } else if (strcmp(argv[0], "reboot") == 0) {
                    printf("Rebooting system...\r\n");
                    dmesg_info("System reboot requested by user");
                    sleep_ms(500);
                    watchdog_enable(1, 1);
                    while (1) {
                    }
                } else if (strcmp(argv[0], "sage") == 0) {
                    cmd_sage(argc, argv);
                } else if (strcmp(argv[0], "script") == 0) {
                    cmd_script(argc, argv);
                } else {
                    printf("Unknown command: %s\r\n", argv[0]);
                    printf("Type 'help' for available commands\r\n");
                }

                wdt_feed();
                supervisor_heartbeat();
            }

            idx = 0;
            printf(">");
            fflush(stdout);
        } else if (c == '\b' || c == 0x7F) { // Backspace
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c >= 32 && c < 127) {
            if (idx < (int)sizeof(buffer) - 1) {
                buffer[idx++] = (char)c;
                putchar(c);
                fflush(stdout);
            }
        }
    }
}
