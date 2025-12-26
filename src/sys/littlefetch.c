// littlefetch.c - System information display for littleOS
// Neofetch-style system info with ASCII art

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/bootrom.h"
#include "users_config.h"
#include "scheduler.h"
#include "memory.h"
#include "dmesg.h"

// ANSI color codes
#define COLOR_RESET     "\033[0m"
#define COLOR_BOLD      "\033[1m"
#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_BLUE      "\033[34m"
#define COLOR_MAGENTA   "\033[35m"
#define COLOR_CYAN      "\033[36m"
#define COLOR_WHITE     "\033[37m"

// ASCII art for littleOS (RP2040 themed)
static const char *logo[] = {
    "    ___       ___    ",
    "   /   \\___/   \\   ",
    "  |  RP2040 OS  |  ",
    "   \\___________/   ",
    "    | | | | | |    ",
    "    |_|_|_|_|_|    ",
    "                   ",
    " littleOS v0.1    "
};

#define LOGO_LINES (sizeof(logo) / sizeof(logo[0]))

// Format uptime nicely
static void format_uptime(uint64_t ms, char *buf, size_t len) {
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;

    if (days > 0) {
        snprintf(buf, len, "%ud %uh %um", days, hours % 24, minutes % 60);
    } else if (hours > 0) {
        snprintf(buf, len, "%uh %um %us", hours, minutes % 60, seconds % 60);
    } else if (minutes > 0) {
        snprintf(buf, len, "%um %us", minutes, seconds % 60);
    } else {
        snprintf(buf, len, "%us", seconds);
    }
}

// Get CPU frequency in MHz
static uint32_t get_cpu_freq_mhz(void) {
    return clock_get_hz(clk_sys) / 1000000;
}

// Get memory info using littleOS memory API
static void get_memory_info(uint32_t *total_kb, uint32_t *used_kb, uint32_t *free_kb) {
    memory_stats_t stats;

    if (memory_get_stats(&stats)) {
        // Total heap size from configuration
        *total_kb = LITTLEOS_HEAP_SIZE / 1024;

        // Current usage from stats
        *used_kb = stats.current_usage / 1024;

        // Free = Total - Used
        *free_kb = *total_kb - *used_kb;
    } else {
        // Fallback values
        *total_kb = LITTLEOS_HEAP_SIZE / 1024;
        *used_kb = 0;
        *free_kb = *total_kb;
    }
}

// Get current task/user context
static task_sec_ctx_t* get_current_context(void) {
    uint16_t current_task_id = task_get_current();

    if (current_task_id != 0xFFFF) {
        task_descriptor_t desc;
        if (task_get_descriptor(current_task_id, &desc)) {
            // Return pointer to security context in descriptor
            static task_sec_ctx_t ctx;
            ctx = desc.sec_ctx;
            return &ctx;
        }
    }

    // Fallback: return root context
    static task_sec_ctx_t root_ctx;
    root_ctx = users_root_context();
    return &root_ctx;
}

// Print info line with color
static void print_info(int line_num, const char *label, const char *value, const char *color) {
    // Print logo on left
    if (line_num < LOGO_LINES) {
        printf("%s%-20s%s", COLOR_CYAN, logo[line_num], COLOR_RESET);
    } else {
        printf("%-20s", "");
    }

    // Print info on right
    if (label && value) {
        printf("  %s%s%s%s: %s%s%s\r\n", 
               COLOR_BOLD, color, label, COLOR_RESET,
               color, value, COLOR_RESET);
    } else {
        printf("\r\n");
    }
}

// Main fetch function
void littlefetch(void) {
    char buf[128];
    int line = 0;

    // Header (blank line with logo)
    print_info(line++, NULL, NULL, NULL);

    // OS
    print_info(line++, "OS", "littleOS RP2040", COLOR_CYAN);

    // Host (chip info)
    snprintf(buf, sizeof(buf), "Raspberry Pi RP2040");
    print_info(line++, "Host", buf, COLOR_CYAN);

    // Kernel version
    snprintf(buf, sizeof(buf), "littleOS v0.1 (%s)", __DATE__);
    print_info(line++, "Kernel", buf, COLOR_CYAN);

    // Uptime
    uint64_t uptime_ms = to_ms_since_boot(get_absolute_time());
    format_uptime(uptime_ms, buf, sizeof(buf));
    print_info(line++, "Uptime", buf, COLOR_GREEN);

    // Current user
    task_sec_ctx_t *ctx = get_current_context();
    if (ctx) {
        const user_account_t *user = users_get_by_uid(ctx->uid);
        if (user) {
            snprintf(buf, sizeof(buf), "%s (UID=%d, GID=%d)", 
                     user->username, ctx->uid, ctx->gid);
        } else {
            snprintf(buf, sizeof(buf), "UID=%d, GID=%d", ctx->uid, ctx->gid);
        }
    } else {
        snprintf(buf, sizeof(buf), "Unknown");
    }
    print_info(line++, "User", buf, COLOR_YELLOW);

    // Shell
    print_info(line++, "Shell", "littleOS shell", COLOR_YELLOW);

    // CPU
    uint32_t freq = get_cpu_freq_mhz();
    snprintf(buf, sizeof(buf), "ARM Cortex-M0+ (Dual Core) @ %u MHz", freq);
    print_info(line++, "CPU", buf, COLOR_RED);

    // Memory (using littleOS heap stats)
    uint32_t total_kb, used_kb, free_kb;
    get_memory_info(&total_kb, &used_kb, &free_kb);
    snprintf(buf, sizeof(buf), "%u KB / %u KB (%u KB free)", 
             used_kb, total_kb, free_kb);
    print_info(line++, "Memory", buf, COLOR_MAGENTA);

    // Tasks (using littleOS scheduler API)
    uint16_t task_count = task_get_count();
    snprintf(buf, sizeof(buf), "%u running", task_count);
    print_info(line++, "Tasks", buf, COLOR_BLUE);

    // Flash size
    snprintf(buf, sizeof(buf), "2 MB");
    print_info(line++, "Flash", buf, COLOR_WHITE);

    // Voltage
    snprintf(buf, sizeof(buf), "3.3V");
    print_info(line++, "Voltage", buf, COLOR_WHITE);

    // Print remaining logo lines
    while (line < LOGO_LINES) {
        print_info(line++, NULL, NULL, NULL);
    }

    // Color palette display
    printf("\r\n");
    printf("%-20s  ", "");
    printf("%s███%s", COLOR_RED, COLOR_RESET);
    printf("%s███%s", COLOR_GREEN, COLOR_RESET);
    printf("%s███%s", COLOR_YELLOW, COLOR_RESET);
    printf("%s███%s", COLOR_BLUE, COLOR_RESET);
    printf("%s███%s", COLOR_MAGENTA, COLOR_RESET);
    printf("%s███%s", COLOR_CYAN, COLOR_RESET);
    printf("%s███%s", COLOR_WHITE, COLOR_RESET);
    printf("\r\n\r\n");
}