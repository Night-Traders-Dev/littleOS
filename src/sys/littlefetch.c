// littlefetch.c - System information display for littleOS
// Neofetch-style system info with ASCII art

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/bootrom.h"
#endif

#include "memory_segmented.h"
#include "scheduler.h"
#include "littlefetch.h"
#include "system_info.h"

// ANSI color codes (can be disabled by defining LITTLEFETCH_NO_COLOR)
#ifndef LITTLEFETCH_NO_COLOR
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#else
#define COLOR_RESET   ""
#define COLOR_BOLD    ""
#define COLOR_RED     ""
#define COLOR_GREEN   ""
#define COLOR_YELLOW  ""
#define COLOR_BLUE    ""
#define COLOR_MAGENTA ""
#define COLOR_CYAN    ""
#define COLOR_WHITE   ""
#endif



// ASCII art for littleOS (RP2040 themed)
static const char *logo[] = {
    " ___ ___            ",
    " / \\___/ \\           ",
    " | RP2040 OS |       ",
    " \\___________/       ",
    " | | | | | |         ",
    " |_|_|_|_|_|         ",
    "                     ",
    " littleOS v0.4.0       "
};

#define LOGO_LINES (sizeof(logo) / sizeof(logo[0]))

// Format uptime nicely
static void format_uptime(uint64_t ms, char *buf, size_t len) {
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours   = minutes / 60;
    uint32_t days    = hours / 24;

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
#ifdef PICO_BUILD
    return clock_get_hz(clk_sys) / 1000000;
#else
    return 0;
#endif
}

// Get memory info using new littleOS memory API
static void get_memory_info(uint32_t *total_kb, uint32_t *used_kb, uint32_t *free_kb) {
    MemoryStats stats = memory_get_stats();

    // Total = kernel + interpreter heap
    *total_kb = (stats.kernel_used + stats.kernel_free +
                 stats.interpreter_used + stats.interpreter_free) / 1024;

    // Current usage = kernel + interpreter used
    *used_kb = (stats.kernel_used + stats.interpreter_used) / 1024;

    // Free = Total - Used
    *free_kb = *total_kb - *used_kb;
}

// Print info line with color and logo
static void print_info(int line_num, const char *label, const char *value, const char *color) {
    // Left: logo
    if (line_num < (int)LOGO_LINES) {
        printf("%s%-20s%s", COLOR_CYAN, logo[line_num], COLOR_RESET);
    } else {
        printf("%-20s", "");
    }

    // Right: label/value
    if (label && value) {
        printf(" %s%s%s%s: %s%s%s\r\n",
               COLOR_BOLD, color ? color : "", label, COLOR_RESET,
               color ? color : "", value, COLOR_RESET);
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
    snprintf(buf, sizeof(buf), "littleOS %s", system_get_version(), __DATE__);
//    snprintf(buf, sizeof(buf), "littleOS v0.4.0 (%s)", __DATE__);
    print_info(line++, "Kernel", buf, COLOR_CYAN);

    // Uptime
#ifdef PICO_BUILD
    uint64_t uptime_ms = to_ms_since_boot(get_absolute_time());
#else
    uint64_t uptime_ms = 0;
#endif
    format_uptime(uptime_ms, buf, sizeof(buf));
    print_info(line++, "Uptime", buf, COLOR_GREEN);

    // Shell
    print_info(line++, "Shell", "littleOS shell", COLOR_YELLOW);

    // CPU
    uint32_t freq = get_cpu_freq_mhz();
    if (freq > 0) {
        snprintf(buf, sizeof(buf), "ARM Cortex-M0+ (Dual Core) @ %u MHz", freq);
    } else {
        snprintf(buf, sizeof(buf), "ARM Cortex-M0+ (Dual Core)");
    }
    print_info(line++, "CPU", buf, COLOR_RED);

    // Memory (using segmented heap stats)
    uint32_t total_kb = 0, used_kb = 0, free_kb = 0;
    get_memory_info(&total_kb, &used_kb, &free_kb);
    snprintf(buf, sizeof(buf), "%u KB / %u KB (%u KB free)",
             used_kb, total_kb, free_kb);
    print_info(line++, "Memory", buf, COLOR_MAGENTA);

    // Flash size
    snprintf(buf, sizeof(buf), "2 MB");
    print_info(line++, "Flash", buf, COLOR_WHITE);

    // Voltage
    snprintf(buf, sizeof(buf), "3.3V");
    print_info(line++, "Voltage", buf, COLOR_WHITE);

    // Print remaining logo lines, if any
    while (line < (int)LOGO_LINES) {
        print_info(line++, NULL, NULL, NULL);
    }

    // Color palette display (can be disabled if it upsets your host)
#ifndef LITTLEFETCH_NO_PALETTE
    printf("\r\n");
    printf("%-20s ", "");
    printf("%s███%s", COLOR_RED,     COLOR_RESET);
    printf("%s███%s", COLOR_GREEN,   COLOR_RESET);
    printf("%s███%s", COLOR_YELLOW,  COLOR_RESET);
    printf("%s███%s", COLOR_BLUE,    COLOR_RESET);
    printf("%s███%s", COLOR_MAGENTA, COLOR_RESET);
    printf("%s███%s", COLOR_CYAN,    COLOR_RESET);
    printf("%s███%s", COLOR_WHITE,   COLOR_RESET);
    printf("\r\n\r\n");
#endif
}
