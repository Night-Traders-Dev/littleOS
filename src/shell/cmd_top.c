// src/shell/cmd_top.c - Live system monitor (htop-style) for littleOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "scheduler.h"
#include "profiler.h"
#include "memory_segmented.h"
#include "watchdog.h"
#include "supervisor.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/* ============================================================================
 * ANSI escape sequences
 * ========================================================================== */

#define ANSI_CLEAR      "\033[2J\033[H"
#define ANSI_BOLD       "\033[1m"
#define ANSI_RESET      "\033[0m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_RED        "\033[31m"
#define ANSI_CYAN       "\033[36m"
#define ANSI_WHITE      "\033[37m"
#define ANSI_BOLD_GREEN "\033[1;32m"
#define ANSI_BOLD_RED   "\033[1;31m"
#define ANSI_REVERSE    "\033[7m"

/* ============================================================================
 * Helpers
 * ========================================================================== */

static const char *state_short(task_state_t s) {
    switch (s) {
    case TASK_STATE_IDLE:       return "IDL";
    case TASK_STATE_READY:      return "RDY";
    case TASK_STATE_RUNNING:    return "RUN";
    case TASK_STATE_BLOCKED:    return "BLK";
    case TASK_STATE_SUSPENDED:  return "SUS";
    case TASK_STATE_TERMINATED: return "END";
    default:                    return "???";
    }
}

static const char *state_color(task_state_t s) {
    switch (s) {
    case TASK_STATE_RUNNING:   return ANSI_BOLD_GREEN;
    case TASK_STATE_READY:     return ANSI_YELLOW;
    case TASK_STATE_BLOCKED:   return ANSI_RED;
    case TASK_STATE_SUSPENDED: return ANSI_RED;
    default:                   return ANSI_WHITE;
    }
}

static const char *priority_short(task_priority_t p) {
    switch (p) {
    case TASK_PRIORITY_LOW:      return "LOW";
    case TASK_PRIORITY_NORMAL:   return "NRM";
    case TASK_PRIORITY_HIGH:     return " HI";
    case TASK_PRIORITY_CRITICAL: return "CRT";
    default:                     return "???";
    }
}

/* Format bytes as human-readable: "1234" or "1.2K" */
static void fmt_mem(char *buf, size_t bufsz, uint32_t bytes) {
    if (bytes < 1024) {
        snprintf(buf, bufsz, "%luB", (unsigned long)bytes);
    } else {
        snprintf(buf, bufsz, "%lu.%luK",
                 (unsigned long)(bytes / 1024),
                 (unsigned long)((bytes % 1024) * 10 / 1024));
    }
}

/* Format milliseconds as MM:SS.ms */
static void fmt_time(char *buf, size_t bufsz, uint32_t ms) {
    uint32_t secs = ms / 1000;
    uint32_t frac = (ms % 1000) / 10;
    uint32_t mins = secs / 60;
    secs %= 60;
    snprintf(buf, bufsz, "%02lu:%02lu.%02lu",
             (unsigned long)mins, (unsigned long)secs, (unsigned long)frac);
}

/* Non-blocking character read; returns -1 if nothing available */
static int top_getchar(void) {
#ifdef PICO_BUILD
    return getchar_timeout_us(0);
#else
    return PICO_ERROR_TIMEOUT;
#endif
}

/* Platform millisecond timestamp */
static uint32_t top_now_ms(void) {
#ifdef PICO_BUILD
    return to_ms_since_boot(get_absolute_time());
#else
    static uint32_t fake = 0;
    return fake += 1000;
#endif
}

/* ============================================================================
 * Display rendering
 * ========================================================================== */

static void top_render(uint32_t uptime_ms) {
    /* ---------- system stats ---------- */
    profiler_system_stats_t sys;
    memset(&sys, 0, sizeof(sys));
    profiler_get_system_stats(&sys);

    MemoryStats mem = memory_get_stats();

    /* ---------- clear screen ---------- */
    printf(ANSI_CLEAR);

    /* ---------- line 1: uptime ---------- */
    uint32_t up_s  = uptime_ms / 1000;
    uint32_t up_d  = up_s / 86400;  up_s %= 86400;
    uint32_t up_h  = up_s / 3600;   up_s %= 3600;
    uint32_t up_m  = up_s / 60;     up_s %= 60;

    /* Approximate load from CPU usage */
    float load = sys.cpu_usage_percent / 100.0f;

    printf(ANSI_BOLD "littleOS top" ANSI_RESET " - %02lu:%02lu:%02lu up %lu day%s, %02lu:%02lu, "
           "load average: %.2f\r\n",
           (unsigned long)up_h, (unsigned long)up_m, (unsigned long)up_s,
           (unsigned long)up_d, up_d == 1 ? "" : "s",
           (unsigned long)up_h, (unsigned long)up_m,
           (double)load);

    /* ---------- line 2: task counts ---------- */
    uint16_t total = task_get_count();
    uint16_t n_run = 0, n_rdy = 0, n_blk = 0, n_idle = 0;
    task_descriptor_t desc;

    for (uint16_t id = 0; id < LITTLEOS_MAX_TASKS; id++) {
        if (!task_get_descriptor(id, &desc)) continue;
        if (desc.state == TASK_STATE_TERMINATED) continue;
        switch (desc.state) {
        case TASK_STATE_RUNNING:   n_run++;  break;
        case TASK_STATE_READY:     n_rdy++;  break;
        case TASK_STATE_BLOCKED:
        case TASK_STATE_SUSPENDED: n_blk++;  break;
        default:                   n_idle++; break;
        }
    }

    printf(ANSI_BOLD "Tasks:" ANSI_RESET " %u total, "
           ANSI_GREEN "%u running" ANSI_RESET ", "
           ANSI_YELLOW "%u ready" ANSI_RESET ", "
           "%u sleeping, "
           ANSI_RED "%u blocked" ANSI_RESET "\r\n",
           total, n_run, n_rdy, n_idle, n_blk);

    /* ---------- line 3: CPU ---------- */
    float cpu_us = sys.cpu_usage_percent;
    float cpu_id = 100.0f - cpu_us;
    if (cpu_id < 0.0f) cpu_id = 0.0f;

    printf(ANSI_BOLD "%%Cpu(s):" ANSI_RESET " %5.1f us, %5.1f sy, %5.1f id\r\n",
           (double)(cpu_us * 0.7f),
           (double)(cpu_us * 0.3f),
           (double)cpu_id);

    /* ---------- line 4: memory ---------- */
    uint32_t mem_total = (uint32_t)(mem.kernel_used + mem.kernel_free);
    uint32_t mem_used  = (uint32_t)mem.kernel_used;
    uint32_t mem_free  = (uint32_t)mem.kernel_free;

    printf(ANSI_BOLD "KiB Mem:" ANSI_RESET " %7.1f total, %7.1f free, %7.1f used, %7.1f interp\r\n",
           (double)mem_total / 1024.0,
           (double)mem_free / 1024.0,
           (double)mem_used / 1024.0,
           (double)mem.interpreter_used / 1024.0);

    /* ---------- blank separator ---------- */
    printf("\r\n");

    /* ---------- column header ---------- */
    printf(ANSI_REVERSE ANSI_BOLD
           "  PID  STATE  PRI  CORE  MEM      PEAK     CPU%%   TIME+      NAME"
           ANSI_RESET "\r\n");

    /* ---------- gather per-task CPU stats ---------- */
    profiler_task_stats_t tstats[LITTLEOS_MAX_TASKS];
    int n_tstats = profiler_get_task_stats(tstats, LITTLEOS_MAX_TASKS);

    /* ---------- task rows ---------- */
    for (uint16_t id = 0; id < LITTLEOS_MAX_TASKS; id++) {
        if (!task_get_descriptor(id, &desc)) continue;
        if (desc.state == TASK_STATE_TERMINATED) continue;

        /* Look up CPU% from profiler */
        float cpu_pct = 0.0f;
        for (int j = 0; j < n_tstats; j++) {
            if (tstats[j].task_id == desc.task_id) {
                cpu_pct = tstats[j].cpu_percent;
                break;
            }
        }

        char mem_buf[16], peak_buf[16], time_buf[16];
        fmt_mem(mem_buf, sizeof(mem_buf), desc.memory_allocated);
        fmt_mem(peak_buf, sizeof(peak_buf), desc.memory_peak);
        fmt_time(time_buf, sizeof(time_buf), desc.total_runtime_ms);

        const char *sc = state_color(desc.state);
        const char *ss = state_short(desc.state);
        const char *core_str = (desc.core_affinity == 0) ? "  0" :
                               (desc.core_affinity == 1) ? "  1" : "any";

        printf(" %4u  %s%-3s" ANSI_RESET "   %s  %s  %-7s  %-7s  %5.1f  %s  %s\r\n",
               desc.task_id,
               sc, ss,
               priority_short(desc.priority),
               core_str,
               mem_buf,
               peak_buf,
               (double)cpu_pct,
               time_buf,
               desc.name);
    }

    /* ---------- bottom bar ---------- */
    printf("\r\n" ANSI_REVERSE
           " Press 'q' quit | 'k' kill task | 's' sort | 'r' reverse "
           ANSI_RESET "\r\n");
}

/* ============================================================================
 * Kill-task prompt (inline, non-blocking-ish)
 * ========================================================================== */

static void top_kill_prompt(void) {
    printf("\r\n" ANSI_BOLD "Kill PID: " ANSI_RESET);

    /* Read up to 5 digits with simple blocking (short timeout) */
    char buf[8];
    int pos = 0;
    while (pos < 5) {
#ifdef PICO_BUILD
        int c = getchar_timeout_us(3000000); /* 3 s timeout */
#else
        int c = -1;
#endif
        if (c < 0) break;                    /* timeout */
        if (c == '\r' || c == '\n') break;   /* enter   */
        if (c == 27) { pos = 0; break; }     /* escape  */
        if (c >= '0' && c <= '9') {
            buf[pos++] = (char)c;
            printf("%c", c);
        }
    }
    buf[pos] = '\0';

    if (pos == 0) {
        printf("(cancelled)\r\n");
        return;
    }

    uint16_t pid = (uint16_t)atoi(buf);
    if (task_terminate(pid)) {
        printf("\r\nKilled task %u\r\n", pid);
    } else {
        printf("\r\nFailed to kill task %u\r\n", pid);
    }

    /* Brief pause so the user can see the message */
#ifdef PICO_BUILD
    sleep_ms(800);
#endif
}

/* ============================================================================
 * Main command entry point
 * ========================================================================== */

int cmd_top(int argc, char *argv[]) {
    /* Parse arguments */
    uint32_t refresh_ms = 1000;  /* default 1 second */
    int max_iterations  = 0;     /* 0 = infinite      */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            refresh_ms = (uint32_t)(atoi(argv[++i]) * 1000);
            if (refresh_ms < 200) refresh_ms = 200;   /* floor */
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: top [-d SEC] [-n NUM]\r\n");
            printf("  -d SEC   Refresh interval in seconds (default 1)\r\n");
            printf("  -n NUM   Exit after NUM iterations\r\n");
            return 0;
        }
    }

    int iteration = 0;

    while (1) {
        uint32_t start = top_now_ms();

        /* Feed watchdog & heartbeat so we don't trigger reset */
        wdt_feed();
        supervisor_heartbeat();

        top_render(start);
        iteration++;

        if (max_iterations > 0 && iteration >= max_iterations) {
            break;
        }

        /* Wait for refresh interval, polling for input */
        while (1) {
            uint32_t elapsed = top_now_ms() - start;
            if (elapsed >= refresh_ms) break;

            int ch = top_getchar();
            if (ch == 'q' || ch == 'Q') {
                /* Restore terminal */
                printf(ANSI_CLEAR);
                return 0;
            } else if (ch == 'k' || ch == 'K') {
                top_kill_prompt();
                break;  /* re-render immediately */
            }
            /* 's' and 'r' are placeholders for future sort toggling */

            /* Feed watchdog during wait */
            wdt_feed();

#ifdef PICO_BUILD
            sleep_ms(50);  /* ~20 Hz poll rate */
#else
            break;  /* non-pico: just exit the wait loop */
#endif
        }
    }

    printf(ANSI_CLEAR);
    return 0;
}
