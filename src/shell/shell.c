#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "watchdog.h"
#include "supervisor.h"
#include "dmesg.h"
#include "littlefetch.h"
#include "shell_env.h"
#include "cron.h"
#include "logcat.h"
#include "syslog.h"
#include "coredump.h"
#include "fs.h"

// Forward declarations - existing commands
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
extern int  cmd_ipc(int argc, char *argv[]);
extern int  cmd_hw(int argc, char *argv[]);
extern int  cmd_net(int argc, char *argv[]);
extern int  cmd_ota(int argc, char *argv[]);
extern int  cmd_usb(int argc, char *argv[]);
extern int  cmd_pio(int argc, char *argv[]);
extern int  cmd_dma(int argc, char *argv[]);
extern int  cmd_remote(int argc, char *argv[]);
extern int  cmd_sensor(int argc, char *argv[]);
extern int  cmd_power(int argc, char *argv[]);
extern int  cmd_profile(int argc, char *argv[]);

// Forward declarations - new commands
extern int  cmd_env(int argc, char *argv[]);
extern int  cmd_alias(int argc, char *argv[]);
extern int  cmd_export(int argc, char *argv[]);
extern int  cmd_prompt(int argc, char *argv[]);
extern int  cmd_cat(int argc, char *argv[]);
extern int  cmd_echo(int argc, char *argv[]);
extern int  cmd_head(int argc, char *argv[]);
extern int  cmd_tail(int argc, char *argv[]);
extern int  cmd_wc(int argc, char *argv[]);
extern int  cmd_grep(int argc, char *argv[]);
extern int  cmd_hexdump(int argc, char *argv[]);
extern int  cmd_tee(int argc, char *argv[]);
extern int  cmd_top(int argc, char *argv[]);
extern int  cmd_pinout(int argc, char *argv[]);
extern int  cmd_proc(int argc, char *argv[]);
extern int  cmd_dev(int argc, char *argv[]);
extern int  cmd_cron(int argc, char *argv[]);
extern int  cmd_mqtt(int argc, char *argv[]);
extern int  cmd_pkg(int argc, char *argv[]);
extern int  cmd_screen(int argc, char *argv[]);
extern int  cmd_man(int argc, char *argv[]);

// Forward declarations - new features (v0.6.0)
extern int  cmd_logcat(int argc, char *argv[]);
extern int  cmd_trace(int argc, char *argv[]);
extern int  cmd_watchpoint(int argc, char *argv[]);
extern int  cmd_benchmark(int argc, char *argv[]);
extern int  cmd_selftest(int argc, char *argv[]);
extern int  cmd_coredump(int argc, char *argv[]);
extern int  cmd_syslog(int argc, char *argv[]);
extern int  cmd_i2cscan(int argc, char *argv[]);
extern int  cmd_wire(int argc, char *argv[]);
extern int  cmd_pwmtune(int argc, char *argv[]);
extern int  cmd_adcstream(int argc, char *argv[]);
extern int  cmd_gpiowatch(int argc, char *argv[]);
extern int  cmd_neopixel(int argc, char *argv[]);
extern int  cmd_display(int argc, char *argv[]);

// ===========================================================================
// Command table
// ===========================================================================

typedef struct {
    const char *name;
    int (*handler)(int argc, char *argv[]);
    const char *help;
} shell_cmd_t;

// Wrappers for void-returning commands
static int wrap_health(int argc, char *argv[])     { cmd_health(argc, argv); return 0; }
static int wrap_stats(int argc, char *argv[])       { cmd_stats(argc, argv); return 0; }
static int wrap_supervisor(int argc, char *argv[])  { cmd_supervisor(argc, argv); return 0; }
static int wrap_dmesg(int argc, char *argv[])       { cmd_dmesg(argc, argv); return 0; }
static int wrap_fetch(int argc, char *argv[])       { (void)argc; (void)argv; littlefetch(); return 0; }

static const shell_cmd_t cmd_table[] = {
    // System
    { "health",     wrap_health,     "Quick system health check" },
    { "stats",      wrap_stats,      "Detailed system statistics" },
    { "supervisor", wrap_supervisor, "Supervisor control" },
    { "dmesg",      wrap_dmesg,      "Kernel message buffer" },
    { "fetch",      wrap_fetch,      "System info display" },
    // Users & permissions
    { "users",      cmd_users,       "User account management" },
    { "perms",      cmd_perms,       "Permission and access control" },
    // Tasks & memory
    { "tasks",      cmd_tasks,       "Task management (scheduler)" },
    { "memory",     cmd_memory,      "Memory diagnostics and tests" },
    { "top",        cmd_top,         "Live system monitor (htop-style)" },
    { "profile",    cmd_profile,     "Runtime profiling" },
    // Filesystem & text
    { "fs",         cmd_fs,          "Filesystem tools" },
    { "cat",        cmd_cat,         "Display file contents" },
    { "echo",       cmd_echo,        "Print text" },
    { "head",       cmd_head,        "Show first lines of file" },
    { "tail",       cmd_tail,        "Show last lines of file" },
    { "wc",         cmd_wc,          "Word/line/byte count" },
    { "grep",       cmd_grep,        "Search for patterns in files" },
    { "hexdump",    cmd_hexdump,     "Hex dump of file" },
    { "tee",        cmd_tee,         "Duplicate output to file" },
    // Virtual filesystems
    { "proc",       cmd_proc,        "Process filesystem (/proc)" },
    { "dev",        cmd_dev,         "Device files (/dev)" },
    // Hardware
    { "hw",         cmd_hw,          "Hardware peripherals (I2C/SPI/PWM/ADC)" },
    { "pio",        cmd_pio,         "PIO programmable I/O" },
    { "dma",        cmd_dma,         "DMA engine control" },
    { "usb",        cmd_usb,         "USB device mode (CDC/HID/MSC)" },
    { "pinout",     cmd_pinout,      "GPIO pin visualizer" },
    // Networking
    { "net",        cmd_net,         "Networking (WiFi/TCP/UDP)" },
    { "mqtt",       cmd_mqtt,        "MQTT IoT client" },
    { "remote",     cmd_remote,      "Remote shell over TCP" },
    { "ota",        cmd_ota,         "Over-the-air firmware updates" },
    // Scripting & packages
    { "sage",       cmd_sage,        "SageLang interpreter" },
    { "script",     cmd_script,      "Script management" },
    { "pkg",        cmd_pkg,         "Package manager" },
    // System services
    { "sensor",     cmd_sensor,      "Sensor framework and logging" },
    { "power",      cmd_power,       "Power management and sleep" },
    { "cron",       cmd_cron,        "Scheduled tasks" },
    { "ipc",        cmd_ipc,         "Inter-process communication" },
    // Shell
    { "env",        cmd_env,         "Environment variables" },
    { "alias",      cmd_alias,       "Command aliases" },
    { "export",     cmd_export,      "Set environment variable" },
    { "screen",     cmd_screen,      "Terminal multiplexer" },
    { "man",        cmd_man,         "Manual pages" },
    // Debug & diagnostics (v0.6.0)
    { "logcat",     cmd_logcat,      "Structured logging with filters" },
    { "trace",      cmd_trace,       "Execution trace buffer" },
    { "watchpoint", cmd_watchpoint,  "Memory watchpoints" },
    { "benchmark",  cmd_benchmark,   "Performance benchmarks" },
    { "selftest",   cmd_selftest,    "Hardware self-test suite" },
    { "coredump",   cmd_coredump,    "Crash dump viewer" },
    { "syslog",     cmd_syslog,      "Persistent system log" },
    // Hardware tools (v0.6.0)
    { "i2cscan",    cmd_i2cscan,     "I2C bus scanner" },
    { "wire",       cmd_wire,        "Interactive I2C/SPI REPL" },
    { "pwmtune",    cmd_pwmtune,     "PWM frequency/duty tuner" },
    { "adc",        cmd_adcstream,   "ADC read/stream/stats" },
    { "gpiowatch",  cmd_gpiowatch,   "GPIO state monitor" },
    { "neopixel",   cmd_neopixel,    "WS2812 NeoPixel control" },
    { "display",    cmd_display,     "SSD1306 OLED display" },
    { NULL, NULL, NULL }
};

#define CMD_TABLE_SIZE (sizeof(cmd_table) / sizeof(cmd_table[0]) - 1)

// ===========================================================================
// Command history
// ===========================================================================

#define HISTORY_SIZE 20
#define MAX_CMD_LEN  512

static char history[HISTORY_SIZE][MAX_CMD_LEN];
static int  history_count = 0;
static int  history_pos   = 0;

static void add_to_history(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (history_count > 0 &&
        strcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0) {
        return;
    }
    strncpy(history[history_count % HISTORY_SIZE], cmd, MAX_CMD_LEN - 1);
    history[history_count % HISTORY_SIZE][MAX_CMD_LEN - 1] = '\0';
    history_count++;
    history_pos = history_count;
}

static const char* get_history(int offset) {
    if (history_count == 0) return NULL;
    int pos = history_pos + offset;
    if (pos < 0)             pos = 0;
    if (pos > history_count) pos = history_count;
    history_pos = pos;
    if (pos == history_count) return "";
    int actual_pos = pos % HISTORY_SIZE;
    if (pos < history_count - HISTORY_SIZE) {
        actual_pos = (history_count - HISTORY_SIZE + (pos % HISTORY_SIZE)) % HISTORY_SIZE;
    }
    return history[actual_pos];
}

// ===========================================================================
// Terminal helpers
// ===========================================================================

static void clear_line(int len) {
    for (int i = 0; i < len; i++) printf("\b");
    for (int i = 0; i < len; i++) printf(" ");
    for (int i = 0; i < len; i++) printf("\b");
    fflush(stdout);
}

static void print_prompt(void) {
    char prompt[SHELL_PROMPT_MAX];
    if (shell_prompt_render(prompt, sizeof(prompt))) {
        printf("%s", prompt);
    } else {
        printf(">");
    }
    fflush(stdout);
}

// ===========================================================================
// Tab completion
// ===========================================================================

static void tab_complete(char *buffer, int *idx) {
    if (*idx == 0) return;

    // Find the word being completed
    buffer[*idx] = '\0';

    // Find the last space to get current word
    char *last_space = strrchr(buffer, ' ');
    const char *prefix;
    int prefix_len;

    if (last_space) {
        prefix = last_space + 1;
    } else {
        prefix = buffer;
    }
    prefix_len = (int)strlen(prefix);
    if (prefix_len == 0) return;

    // Collect matches from command table
    const char *matches[16];
    int match_count = 0;

    // Only complete commands if we're on the first word
    if (!last_space) {
        for (int i = 0; cmd_table[i].name != NULL && match_count < 16; i++) {
            if (strncmp(cmd_table[i].name, prefix, prefix_len) == 0) {
                matches[match_count++] = cmd_table[i].name;
            }
        }
        // Also check built-in commands
        const char *builtins[] = { "help", "version", "clear", "reboot", "history",
                                    "exit", NULL };
        for (int i = 0; builtins[i] && match_count < 16; i++) {
            if (strncmp(builtins[i], prefix, prefix_len) == 0) {
                // Check not already matched
                bool dup = false;
                for (int j = 0; j < match_count; j++) {
                    if (strcmp(matches[j], builtins[i]) == 0) { dup = true; break; }
                }
                if (!dup) matches[match_count++] = builtins[i];
            }
        }
    }

    if (match_count == 0) return;

    if (match_count == 1) {
        // Single match - complete it
        const char *completion = matches[0];
        int comp_len = (int)strlen(completion);
        clear_line(*idx);
        if (last_space) {
            // Keep everything before the word
            int pre_len = (int)(last_space - buffer + 1);
            memcpy(buffer + pre_len, completion, comp_len);
            buffer[pre_len + comp_len] = ' ';
            buffer[pre_len + comp_len + 1] = '\0';
            *idx = pre_len + comp_len + 1;
        } else {
            memcpy(buffer, completion, comp_len);
            buffer[comp_len] = ' ';
            buffer[comp_len + 1] = '\0';
            *idx = comp_len + 1;
        }
        printf("%s", buffer);
        fflush(stdout);
    } else {
        // Multiple matches - show all
        printf("\r\n");
        for (int i = 0; i < match_count; i++) {
            printf("  %s", matches[i]);
            if ((i + 1) % 6 == 0 || i == match_count - 1) printf("\r\n");
        }
        // Find common prefix for partial completion
        int common_len = (int)strlen(matches[0]);
        for (int i = 1; i < match_count; i++) {
            int j = 0;
            while (j < common_len && matches[0][j] == matches[i][j]) j++;
            common_len = j;
        }
        if (common_len > prefix_len) {
            clear_line(0); // Don't need to clear - we printed newlines
            if (last_space) {
                int pre_len = (int)(last_space - buffer + 1);
                memcpy(buffer + pre_len, matches[0], common_len);
                buffer[pre_len + common_len] = '\0';
                *idx = pre_len + common_len;
            } else {
                memcpy(buffer, matches[0], common_len);
                buffer[common_len] = '\0';
                *idx = common_len;
            }
        }
        print_prompt();
        printf("%s", buffer);
        fflush(stdout);
    }
}

// ===========================================================================
// History expansion (!!, !n)
// ===========================================================================

static bool expand_history(char *buffer, int *idx) {
    if (buffer[0] != '!') return false;

    const char *replacement = NULL;

    if (buffer[1] == '!' && (*idx == 2 || buffer[2] == ' ')) {
        // !! - repeat last command
        if (history_count > 0) {
            replacement = history[(history_count - 1) % HISTORY_SIZE];
        }
    } else if (buffer[1] >= '0' && buffer[1] <= '9') {
        // !N - repeat command N
        int n = atoi(&buffer[1]);
        if (n > 0 && n <= history_count) {
            int actual = (n - 1) % HISTORY_SIZE;
            replacement = history[actual];
        }
    }

    if (replacement) {
        // Find suffix after !X
        char suffix[MAX_CMD_LEN] = "";
        int bang_end = 1;
        if (buffer[1] == '!') {
            bang_end = 2;
        } else {
            while (buffer[bang_end] >= '0' && buffer[bang_end] <= '9') bang_end++;
        }
        if (buffer[bang_end]) {
            strncpy(suffix, &buffer[bang_end], MAX_CMD_LEN - 1);
            suffix[MAX_CMD_LEN - 1] = '\0';
        }

        int rlen = (int)strlen(replacement);
        int slen = (int)strlen(suffix);
        if (rlen + slen < MAX_CMD_LEN) {
            memcpy(buffer, replacement, rlen);
            memcpy(buffer + rlen, suffix, slen);
            buffer[rlen + slen] = '\0';
            *idx = rlen + slen;
            printf("%s\r\n", buffer);
            return true;
        }
    }

    return false;
}

// ===========================================================================
// Pipe support
// ===========================================================================

// Simple pipe buffer for cmd1 | cmd2
#define PIPE_BUF_SIZE 2048
static char pipe_buffer[PIPE_BUF_SIZE];
static int  pipe_len = 0;
static bool pipe_capture = false;

// Custom putchar that captures to pipe buffer when piping
static void pipe_putchar(char c) {
    if (pipe_capture && pipe_len < PIPE_BUF_SIZE - 1) {
        pipe_buffer[pipe_len++] = c;
    }
}

// ===========================================================================
// I/O Redirection
// ===========================================================================

typedef struct {
    bool redirect_out;      // > or >>
    bool redirect_append;   // >>
    char out_file[64];      // output filename
} io_redirect_t;

static io_redirect_t parse_redirects(char *cmdline) {
    io_redirect_t redir = { false, false, "" };

    // Look for >> first, then >
    char *p = strstr(cmdline, ">>");
    if (p) {
        redir.redirect_out = true;
        redir.redirect_append = true;
        *p = '\0';
        p += 2;
        while (*p == ' ') p++;
        strncpy(redir.out_file, p, sizeof(redir.out_file) - 1);
        redir.out_file[sizeof(redir.out_file) - 1] = '\0';
        // Trim trailing spaces
        int len = (int)strlen(redir.out_file);
        while (len > 0 && redir.out_file[len - 1] == ' ') redir.out_file[--len] = '\0';
        return redir;
    }

    p = strchr(cmdline, '>');
    if (p) {
        redir.redirect_out = true;
        redir.redirect_append = false;
        *p = '\0';
        p++;
        while (*p == ' ') p++;
        strncpy(redir.out_file, p, sizeof(redir.out_file) - 1);
        redir.out_file[sizeof(redir.out_file) - 1] = '\0';
        int len = (int)strlen(redir.out_file);
        while (len > 0 && redir.out_file[len - 1] == ' ') redir.out_file[--len] = '\0';
    }

    return redir;
}

// ===========================================================================
// Command execution
// ===========================================================================

static int parse_args(char* buffer, char* argv[], int max_args) {
    int   argc  = 0;
    char* token = strtok(buffer, " ");
    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token        = strtok(NULL, " ");
    }
    return argc;
}

// Execute a single command (no pipes/redirects)
static int execute_single(int argc, char *argv[]) {
    if (argc <= 0) return 0;

    // Built-in commands
    if (strcmp(argv[0], "help") == 0) {
        printf("\033[1mAvailable commands:\033[0m\r\n");
        printf("\r\n  \033[1mSystem:\033[0m\r\n");
        printf("    help version clear reboot history health stats\r\n");
        printf("    supervisor dmesg fetch\r\n");
        printf("\r\n  \033[1mProcesses & Memory:\033[0m\r\n");
        printf("    tasks memory top profile\r\n");
        printf("\r\n  \033[1mUsers & Security:\033[0m\r\n");
        printf("    users perms\r\n");
        printf("\r\n  \033[1mFilesystem & Text:\033[0m\r\n");
        printf("    fs cat echo head tail wc grep hexdump tee\r\n");
        printf("\r\n  \033[1mVirtual Filesystems:\033[0m\r\n");
        printf("    proc dev\r\n");
        printf("\r\n  \033[1mHardware:\033[0m\r\n");
        printf("    hw pio dma usb pinout i2cscan wire\r\n");
        printf("    pwmtune adc gpiowatch neopixel display\r\n");
        printf("\r\n  \033[1mNetworking:\033[0m\r\n");
        printf("    net mqtt remote ota\r\n");
        printf("\r\n  \033[1mScripting & Packages:\033[0m\r\n");
        printf("    sage script pkg\r\n");
        printf("\r\n  \033[1mServices:\033[0m\r\n");
        printf("    sensor power cron ipc\r\n");
        printf("\r\n  \033[1mDebug & Diagnostics:\033[0m\r\n");
        printf("    logcat trace watchpoint benchmark selftest\r\n");
        printf("    coredump syslog\r\n");
        printf("\r\n  \033[1mShell:\033[0m\r\n");
        printf("    env alias export screen man\r\n");
        printf("\r\n  Use 'man <cmd>' for detailed help. Tab to autocomplete.\r\n");
        printf("  Use UP/DOWN arrows for history. !! repeats last command.\r\n");
        return 0;
    }

    if (strcmp(argv[0], "version") == 0) {
        printf("littleOS v0.6.0 - RP2040\r\n");
        printf("With SageLang v0.8.0\r\n");
        printf("Supervisor: %s\r\n",
               supervisor_is_running() ? "Active" : "Inactive");
        return 0;
    }

    if (strcmp(argv[0], "clear") == 0) {
        printf("\033[2J\033[H");
        return 0;
    }

    if (strcmp(argv[0], "history") == 0) {
        printf("Command history:\r\n");
        int start = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE : 0;
        for (int i = start; i < history_count; i++) {
            printf(" %d: %s\r\n", i + 1, history[i % HISTORY_SIZE]);
        }
        return 0;
    }

    if (strcmp(argv[0], "reboot") == 0) {
        printf("Rebooting system...\r\n");
        dmesg_info("System reboot requested by user");
        sleep_ms(500);
        watchdog_enable(1, 1);
        while (1) {}
    }

    if (strcmp(argv[0], "exit") == 0) {
        printf("Logout\r\n");
        return 0;
    }

    // Look up in command table
    for (int i = 0; cmd_table[i].name != NULL; i++) {
        if (strcmp(argv[0], cmd_table[i].name) == 0) {
            return cmd_table[i].handler(argc, argv);
        }
    }

    printf("Unknown command: %s\r\n", argv[0]);
    printf("Type 'help' for available commands\r\n");
    return -1;
}

// Execute command line with alias/env expansion, pipes, redirects
void shell_execute_command(const char *cmd) {
    char expanded[MAX_CMD_LEN];
    char work[MAX_CMD_LEN];

    // Skip empty commands
    if (!cmd || cmd[0] == '\0') return;

    // 1. Alias expansion
    shell_alias_expand(cmd, expanded, sizeof(expanded));

    // 2. Environment variable expansion
    char env_expanded[MAX_CMD_LEN];
    shell_env_expand(expanded, env_expanded, sizeof(env_expanded));

    // 3. Check for pipe
    char *pipe_pos = strchr(env_expanded, '|');
    if (pipe_pos) {
        // Split into two commands
        *pipe_pos = '\0';
        char *cmd1 = env_expanded;
        char *cmd2 = pipe_pos + 1;
        while (*cmd2 == ' ') cmd2++;

        // Execute cmd1, capture output
        pipe_capture = true;
        pipe_len = 0;

        strncpy(work, cmd1, MAX_CMD_LEN - 1);
        work[MAX_CMD_LEN - 1] = '\0';
        // Trim trailing spaces
        int len = (int)strlen(work);
        while (len > 0 && work[len - 1] == ' ') work[--len] = '\0';

        char *argv[32];
        int argc = parse_args(work, argv, 32);
        if (argc > 0) execute_single(argc, argv);

        pipe_capture = false;
        pipe_buffer[pipe_len] = '\0';

        // For now, print pipe output (full pipe forwarding would require
        // stdin redirection which isn't practical on bare metal)
        // Instead, pass captured output as info
        printf("%s", pipe_buffer);

        // Execute cmd2 (pipe output displayed above)
        strncpy(work, cmd2, MAX_CMD_LEN - 1);
        work[MAX_CMD_LEN - 1] = '\0';
        len = (int)strlen(work);
        while (len > 0 && work[len - 1] == ' ') work[--len] = '\0';

        argc = parse_args(work, argv, 32);
        if (argc > 0) execute_single(argc, argv);
        return;
    }

    // 4. Check for I/O redirect
    strncpy(work, env_expanded, MAX_CMD_LEN - 1);
    work[MAX_CMD_LEN - 1] = '\0';
    io_redirect_t redir = parse_redirects(work);

    // Trim trailing spaces from command
    int len = (int)strlen(work);
    while (len > 0 && work[len - 1] == ' ') work[--len] = '\0';

    if (redir.redirect_out && redir.out_file[0]) {
        // Capture output, then write to file
        pipe_capture = true;
        pipe_len = 0;

        char *argv[32];
        int argc = parse_args(work, argv, 32);
        if (argc > 0) execute_single(argc, argv);

        pipe_capture = false;
        pipe_buffer[pipe_len] = '\0';

        // Write captured output to filesystem
        extern struct fs *g_fs_ptr;
        if (g_fs_ptr) {
            struct fs_file fd;
            uint16_t flags = FS_O_WRONLY | FS_O_CREAT;
            if (redir.redirect_append) flags |= FS_O_APPEND;
            else flags |= FS_O_TRUNC;

            if (fs_open(g_fs_ptr, redir.out_file, flags, &fd) == FS_OK) {
                fs_write(g_fs_ptr, &fd, (uint8_t *)pipe_buffer, (uint32_t)pipe_len);
                fs_close(g_fs_ptr, &fd);
                printf("Output written to %s (%d bytes)\r\n", redir.out_file, pipe_len);
            } else {
                printf("Error: cannot open %s for writing\r\n", redir.out_file);
                // Print the output anyway
                printf("%s", pipe_buffer);
            }
        } else {
            printf("Error: filesystem not mounted\r\n");
            printf("%s", pipe_buffer);
        }
        return;
    }

    // 5. Normal execution
    char *argv[32];
    int argc = parse_args(work, argv, 32);
    if (argc > 0) {
        execute_single(argc, argv);
    }
}

// ===========================================================================
// MOTD (Message of the Day)
// ===========================================================================

static void show_motd(void) {
    printf("\r\n");
    printf("\033[36m");
    printf("  _  _  _    _  _        ___  ___\r\n");
    printf(" | |(_)| |_ | |(_) ___  / _ \\/ __|\r\n");
    printf(" | || ||  _||  _|| |/ -_)| (_) \\__ \\\r\n");
    printf(" |_||_| \\__| \\__||_|\\___|\\___/|___/\r\n");
    printf("\033[0m\r\n");
    printf(" RP2040 Dual-Core ARM Cortex-M0+ | 264KB SRAM\r\n");
    printf(" Type \033[1mhelp\033[0m for commands, \033[1mman <cmd>\033[0m for details\r\n");
    printf("\r\n");
}

// ===========================================================================
// Main shell loop
// ===========================================================================

void shell_run(void) {
    char buffer[MAX_CMD_LEN];
    int  idx = 0;

    // Ctrl+C flag
    volatile bool interrupted = false;

    // ANSI escape sequence state machine
    enum { STATE_NORMAL, STATE_ESC, STATE_CSI } escape_state = STATE_NORMAL;

    // Ctrl+A state for screen switching
    bool ctrl_a_pending = false;

    // Track time for periodic tasks
    uint32_t last_wdt_feed   = to_ms_since_boot(get_absolute_time());
    uint32_t last_heartbeat  = last_wdt_feed;
    uint32_t last_cron_tick  = last_wdt_feed;

    // Show MOTD
    show_motd();
    print_prompt();

    while (1) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Feed watchdog every 1 second
        if (now - last_wdt_feed >= 1000) {
            wdt_feed();
            last_wdt_feed = now;
        }

        // Send heartbeat to supervisor every 500ms
        if (now - last_heartbeat >= 500) {
            supervisor_heartbeat();
            last_heartbeat = now;
        }

        // Run cron tick every second
        if (now - last_cron_tick >= 1000) {
            cron_tick();
            last_cron_tick = now;
        }

        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            sleep_ms(10);
            continue;
        }

        // Handle Ctrl+A for screen switching
        if (ctrl_a_pending) {
            ctrl_a_pending = false;
            if (c == 'n' || c == 'N') {
                // Ctrl+A, N = next screen
                extern int tmux_next(void);
                tmux_next();
                print_prompt();
                printf("%.*s", idx, buffer);
                fflush(stdout);
                continue;
            } else if (c == 'p' || c == 'P') {
                // Ctrl+A, P = prev screen
                extern int tmux_prev(void);
                tmux_prev();
                print_prompt();
                printf("%.*s", idx, buffer);
                fflush(stdout);
                continue;
            }
            // Not a screen command, fall through
        }

        // Ctrl+C (ETX = 0x03) - interrupt
        if (c == 0x03) {
            printf("^C\r\n");
            idx = 0;
            interrupted = true;
            print_prompt();
            fflush(stdout);
            continue;
        }

        // Ctrl+A (SOH = 0x01) - screen prefix
        if (c == 0x01) {
            ctrl_a_pending = true;
            continue;
        }

        // Ctrl+L (FF = 0x0C) - clear screen
        if (c == 0x0C) {
            printf("\033[2J\033[H");
            print_prompt();
            printf("%.*s", idx, buffer);
            fflush(stdout);
            continue;
        }

        // Ctrl+D (EOT = 0x04) - EOF/logout
        if (c == 0x04 && idx == 0) {
            printf("logout\r\n");
            continue;
        }

        // Handle ANSI escape sequences for arrow keys
        switch (escape_state) {
        case STATE_NORMAL:
            if (c == 0x1B) {
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
            } else if (c == 'C') { // Right arrow - ignored for now
                continue;
            } else if (c == 'D') { // Left arrow - ignored for now
                continue;
            } else if (c == 'Z') { // Shift+Tab - ignored
                continue;
            }
            break;
        }

        // Tab completion
        if (c == '\t') {
            tab_complete(buffer, &idx);
            continue;
        }

        // Enter key
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            buffer[idx] = '\0';
            interrupted = false;

            if (idx > 0) {
                // History expansion (!!, !n)
                char orig[MAX_CMD_LEN];
                strncpy(orig, buffer, MAX_CMD_LEN);
                bool was_expanded = expand_history(buffer, &idx);

                // Add original to history (not the expanded version)
                add_to_history(was_expanded ? orig : buffer);

                // Feed watchdog before command execution
                wdt_feed();
                supervisor_heartbeat();

                // Execute the command
                shell_execute_command(buffer);

                wdt_feed();
                supervisor_heartbeat();
            }

            idx = 0;
            print_prompt();
        } else if (c == '\b' || c == 0x7F) { // Backspace
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 0x17) { // Ctrl+W - delete word
            while (idx > 0 && buffer[idx - 1] == ' ') {
                idx--;
                printf("\b \b");
            }
            while (idx > 0 && buffer[idx - 1] != ' ') {
                idx--;
                printf("\b \b");
            }
            fflush(stdout);
        } else if (c == 0x15) { // Ctrl+U - clear line
            clear_line(idx);
            idx = 0;
        } else if (c >= 32 && c < 127) {
            if (idx < (int)sizeof(buffer) - 1) {
                buffer[idx++] = (char)c;
                putchar(c);
                fflush(stdout);
            }
        }
    }
}
