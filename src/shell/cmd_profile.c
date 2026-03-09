// src/shell/cmd_profile.c - Shell commands for runtime profiling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "profiler.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/timer.h"
#endif

/* ============================================================================
 * Platform timing for benchmark
 * ========================================================================== */

static inline uint64_t bench_time_us(void) {
#ifdef PICO_BUILD
    return time_us_64();
#else
    static uint64_t counter = 0;
    counter += 1;
    return counter;
#endif
}

/* ============================================================================
 * Usage
 * ========================================================================== */

static void profile_usage(void) {
    printf("Usage: profile <command> [args]\n\n");
    printf("Commands:\n");
    printf("  status           Show profiler status\n");
    printf("  start            Enable profiling\n");
    printf("  stop             Disable profiling\n");
    printf("  report           Show section profiling report\n");
    printf("  tasks            Show per-task CPU usage\n");
    printf("  system           Show system-wide stats\n");
    printf("  reset            Reset all profiling stats\n");
    printf("  section <name>   Register a new profiling section\n");
    printf("  benchmark        Run timing benchmark\n");
}

/* ============================================================================
 * Subcommands
 * ========================================================================== */

static void cmd_profile_status(void) {
    profiler_system_stats_t sys;
    profiler_get_system_stats(&sys);

    printf("=== Profiler Status ===\n");
    printf("  State:              %s\n", profiler_is_running() ? "RUNNING" : "STOPPED");
    printf("  Uptime:             %u ms\n", sys.uptime_ms);
    printf("  CPU Usage:          %.1f%%\n", (double)sys.cpu_usage_percent);
    printf("  Context Switches:   %u\n", sys.total_context_switches);
    printf("  IRQ Count:          %u\n", sys.irq_count);
    printf("  Profiler Overhead:  %u us\n", sys.profiler_overhead_us);
    printf("\n");
}

static void cmd_profile_start(void) {
    int ret = profiler_start();
    if (ret == 0) {
        printf("Profiling started.\n");
    } else {
        printf("Profiling is already running.\n");
    }
}

static void cmd_profile_stop(void) {
    int ret = profiler_stop();
    if (ret == 0) {
        printf("Profiling stopped.\n");
    } else {
        printf("Profiling is not running.\n");
    }
}

static void cmd_profile_report(void) {
    profiler_print_report();
}

static void cmd_profile_tasks(void) {
    profiler_print_tasks();
}

static void cmd_profile_system(void) {
    profiler_system_stats_t sys;
    int ret = profiler_get_system_stats(&sys);
    if (ret != 0) {
        printf("ERROR: Failed to get system stats\n");
        return;
    }

    printf("=== System Profiling Stats ===\n");
    printf("  Uptime:                %u ms\n", sys.uptime_ms);
    printf("  CPU Usage:             %.1f%%\n", (double)sys.cpu_usage_percent);
    printf("  Idle Time (window):    %u us\n", sys.idle_us);
    printf("  Context Switches:      %u\n", sys.total_context_switches);
    printf("\n");
    printf("  IRQ Count:             %u\n", sys.irq_count);
    printf("  IRQ Max Latency:       %u us\n", sys.irq_max_latency_us);
    printf("  IRQ Avg Latency:       %u us\n", sys.irq_avg_latency_us);
    printf("\n");
    printf("  Profiler Overhead:     %u us\n", sys.profiler_overhead_us);
    printf("\n");
}

static void cmd_profile_reset(void) {
    profiler_section_reset_all();
    printf("All profiling stats reset.\n");
}

static void cmd_profile_section(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: profile section <name>\n");
        return;
    }

    const char *name = argv[2];
    int id = profiler_section_register(name);
    if (id < 0) {
        printf("ERROR: Failed to register section '%s' (max sections reached)\n", name);
    } else {
        printf("Registered section '%s' (id=%d)\n", name, id);
    }
}

static void cmd_profile_benchmark(void) {
    printf("=== Profiler Benchmark ===\n");
    printf("Running timing precision test...\n\n");

    /* Test 1: Measure timer resolution */
    uint64_t t_prev = bench_time_us();
    uint64_t t_next;
    uint32_t resolution_ns = 0;
    int resolution_samples = 0;
    uint64_t resolution_sum = 0;

    for (int i = 0; i < 1000; i++) {
        t_next = bench_time_us();
        if (t_next != t_prev) {
            uint64_t diff = t_next - t_prev;
            resolution_sum += diff;
            resolution_samples++;
            t_prev = t_next;
        }
    }

    if (resolution_samples > 0) {
        resolution_ns = (uint32_t)((resolution_sum * 1000) / resolution_samples);
    }

    printf("  Timer resolution:     ~%u ns (%u samples)\n",
           resolution_ns, resolution_samples);

    /* Test 2: Busy loop benchmark */
    #define BENCH_ITERATIONS 100000

    volatile uint32_t sink = 0;
    uint64_t start = bench_time_us();

    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++) {
        sink += i;
    }

    uint64_t end = bench_time_us();
    uint64_t elapsed_us = end - start;

    (void)sink;

    if (elapsed_us == 0) elapsed_us = 1;

    uint32_t iters_per_sec;
    if (elapsed_us > 0) {
        iters_per_sec = (uint32_t)((uint64_t)BENCH_ITERATIONS * 1000000ULL / elapsed_us);
    } else {
        iters_per_sec = 0;
    }

    printf("  Busy loop:            %u iterations in %llu us\n",
           BENCH_ITERATIONS, (unsigned long long)elapsed_us);
    printf("  Throughput:           %u iterations/sec\n", iters_per_sec);

    /* Test 3: Profiler section overhead */
    int bench_sec = profiler_section_register("_benchmark");
    if (bench_sec >= 0) {
        bool was_running = profiler_is_running();
        if (!was_running) {
            profiler_start();
        }

        uint64_t oh_start = bench_time_us();
        for (int i = 0; i < 10000; i++) {
            profiler_section_enter(bench_sec);
            profiler_section_exit(bench_sec);
        }
        uint64_t oh_end = bench_time_us();
        uint64_t oh_elapsed = oh_end - oh_start;

        uint32_t per_call_ns = 0;
        if (oh_elapsed > 0) {
            per_call_ns = (uint32_t)((oh_elapsed * 1000) / 10000);
        }

        printf("  Section enter/exit:   10000 pairs in %llu us\n",
               (unsigned long long)oh_elapsed);
        printf("  Per-call overhead:    ~%u ns\n", per_call_ns);

        /* Get the section stats */
        profiler_section_t sec_stats;
        if (profiler_section_stats(bench_sec, &sec_stats) == 0) {
            uint32_t min_disp = (sec_stats.min_us == UINT32_MAX) ? 0 : sec_stats.min_us;
            printf("  Section stats:        calls=%u min=%u us max=%u us avg=%u us\n",
                   sec_stats.call_count, min_disp, sec_stats.max_us, sec_stats.avg_us);
        }

        /* Clean up: reset the benchmark section */
        profiler_section_reset(bench_sec);

        if (!was_running) {
            profiler_stop();
        }
    }

    printf("\nBenchmark complete.\n");
}

/* ============================================================================
 * Main command entry point
 * ========================================================================== */

int cmd_profile(int argc, char *argv[]) {
    if (argc < 2) {
        profile_usage();
        return 0;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "status") == 0) {
        cmd_profile_status();
    } else if (strcmp(sub, "start") == 0) {
        cmd_profile_start();
    } else if (strcmp(sub, "stop") == 0) {
        cmd_profile_stop();
    } else if (strcmp(sub, "report") == 0) {
        cmd_profile_report();
    } else if (strcmp(sub, "tasks") == 0) {
        cmd_profile_tasks();
    } else if (strcmp(sub, "system") == 0) {
        cmd_profile_system();
    } else if (strcmp(sub, "reset") == 0) {
        cmd_profile_reset();
    } else if (strcmp(sub, "section") == 0) {
        cmd_profile_section(argc, argv);
    } else if (strcmp(sub, "benchmark") == 0) {
        cmd_profile_benchmark();
    } else {
        printf("Unknown profile command: '%s'\n\n", sub);
        profile_usage();
        return 1;
    }

    return 0;
}
