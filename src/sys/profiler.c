// src/sys/profiler.c - Runtime Profiling for littleOS (RP2040)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "profiler.h"
#include "dmesg.h"
#include "scheduler.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/timer.h"
#endif

/* ============================================================================
 * Platform timing abstraction
 * ========================================================================== */

#ifndef PICO_BUILD
static uint64_t fake_timer_us = 0;
#endif

static inline uint64_t profiler_time_us(void) {
#ifdef PICO_BUILD
    return time_us_64();
#else
    fake_timer_us += 1;
    return fake_timer_us;
#endif
}

/* ============================================================================
 * Internal state
 * ========================================================================== */

static profiler_section_t sections[PROFILER_MAX_SECTIONS];
static int section_count = 0;
static bool running = false;

/* Window tracking */
static uint64_t window_start_us = 0;
static uint64_t window_duration_us = 0;

/* Per-task CPU accumulators (indexed by task_id) */
#define PROFILER_MAX_TASKS LITTLEOS_MAX_TASKS
static uint32_t task_cpu_window_us[PROFILER_MAX_TASKS];
static uint32_t task_cpu_total_us[PROFILER_MAX_TASKS];
static uint32_t task_ctx_switches[PROFILER_MAX_TASKS];

/* Current task tracking */
static uint16_t current_tracked_task = 0xFFFF;
static uint64_t current_task_start_us = 0;

/* Idle tracking (task 0 is typically idle) */
static uint32_t idle_window_us = 0;

/* IRQ tracking */
static uint32_t irq_count = 0;
static uint64_t irq_enter_us = 0;
static uint32_t irq_total_us = 0;
static uint32_t irq_max_latency_us = 0;
static uint64_t irq_total_latency_us = 0;

/* Total context switches */
static uint32_t total_context_switches = 0;

/* Overhead estimation */
static uint64_t overhead_accum_us = 0;

/* ============================================================================
 * Init / Start / Stop
 * ========================================================================== */

int profiler_init(void) {
    memset(sections, 0, sizeof(sections));
    section_count = 0;
    running = false;

    memset(task_cpu_window_us, 0, sizeof(task_cpu_window_us));
    memset(task_cpu_total_us, 0, sizeof(task_cpu_total_us));
    memset(task_ctx_switches, 0, sizeof(task_ctx_switches));

    current_tracked_task = 0xFFFF;
    current_task_start_us = 0;
    idle_window_us = 0;

    irq_count = 0;
    irq_enter_us = 0;
    irq_total_us = 0;
    irq_max_latency_us = 0;
    irq_total_latency_us = 0;
    total_context_switches = 0;
    overhead_accum_us = 0;

    window_start_us = profiler_time_us();
    window_duration_us = 0;

    dmesg_info("profiler: initialized (%d max sections, %d max tasks)",
               PROFILER_MAX_SECTIONS, PROFILER_MAX_TASKS);
    return 0;
}

int profiler_start(void) {
    if (running) {
        return -1;
    }
    running = true;
    window_start_us = profiler_time_us();
    dmesg_info("profiler: started");
    return 0;
}

int profiler_stop(void) {
    if (!running) {
        return -1;
    }

    /* Flush current task accumulation */
    if (current_tracked_task != 0xFFFF) {
        uint64_t now = profiler_time_us();
        uint32_t elapsed = (uint32_t)(now - current_task_start_us);
        if (current_tracked_task < PROFILER_MAX_TASKS) {
            task_cpu_window_us[current_tracked_task] += elapsed;
            task_cpu_total_us[current_tracked_task] += elapsed;
        }
        current_task_start_us = now;
    }

    running = false;
    dmesg_info("profiler: stopped");
    return 0;
}

bool profiler_is_running(void) {
    return running;
}

/* ============================================================================
 * Section profiling
 * ========================================================================== */

int profiler_section_register(const char *name) {
    if (section_count >= PROFILER_MAX_SECTIONS) {
        dmesg_warn("profiler: max sections reached, cannot register '%s'", name);
        return -1;
    }

    /* Check for duplicate name */
    for (int i = 0; i < section_count; i++) {
        if (sections[i].active && strncmp(sections[i].name, name, PROFILER_NAME_LEN - 1) == 0) {
            return i;
        }
    }

    int id = section_count;
    section_count++;

    memset(&sections[id], 0, sizeof(profiler_section_t));
    strncpy(sections[id].name, name, PROFILER_NAME_LEN - 1);
    sections[id].name[PROFILER_NAME_LEN - 1] = '\0';
    sections[id].active = true;
    sections[id].min_us = UINT32_MAX;
    sections[id].max_us = 0;
    sections[id].history_idx = 0;
    sections[id].history_count = 0;

    dmesg_debug("profiler: registered section '%s' (id=%d)", name, id);
    return id;
}

void profiler_section_enter(int section_id) {
    if (!running) return;
    if (section_id < 0 || section_id >= section_count) return;
    if (!sections[section_id].active) return;

    uint64_t t0 = profiler_time_us();
    sections[section_id].start_us = (uint32_t)t0;

    /* Track overhead: the time spent in this function */
    uint64_t t1 = profiler_time_us();
    overhead_accum_us += (t1 - t0);
}

void profiler_section_exit(int section_id) {
    if (!running) return;
    if (section_id < 0 || section_id >= section_count) return;
    if (!sections[section_id].active) return;

    uint64_t t0 = profiler_time_us();

    uint64_t now_us = t0;
    uint32_t start = sections[section_id].start_us;
    uint32_t elapsed;

    /* Handle 32-bit wrap by using lower 32 bits of now */
    uint32_t now32 = (uint32_t)now_us;
    if (now32 >= start) {
        elapsed = now32 - start;
    } else {
        elapsed = (UINT32_MAX - start) + now32 + 1;
    }

    profiler_section_t *s = &sections[section_id];

    s->call_count++;
    s->total_us += elapsed;
    s->last_us = elapsed;

    if (elapsed < s->min_us) {
        s->min_us = elapsed;
    }
    if (elapsed > s->max_us) {
        s->max_us = elapsed;
    }

    /* Rolling history */
    s->history[s->history_idx] = elapsed;
    s->history_idx = (s->history_idx + 1) % PROFILER_HISTORY_LEN;
    if (s->history_count < PROFILER_HISTORY_LEN) {
        s->history_count++;
    }

    /* Compute rolling average from history buffer */
    uint64_t sum = 0;
    for (uint8_t i = 0; i < s->history_count; i++) {
        sum += s->history[i];
    }
    s->avg_us = (uint32_t)(sum / s->history_count);

    /* Track overhead */
    uint64_t t1 = profiler_time_us();
    overhead_accum_us += (t1 - t0);
}

int profiler_section_stats(int section_id, profiler_section_t *stats) {
    if (section_id < 0 || section_id >= section_count) return -1;
    if (!stats) return -1;

    memcpy(stats, &sections[section_id], sizeof(profiler_section_t));
    return 0;
}

void profiler_section_reset(int section_id) {
    if (section_id < 0 || section_id >= section_count) return;

    char name_bak[PROFILER_NAME_LEN];
    strncpy(name_bak, sections[section_id].name, PROFILER_NAME_LEN);

    memset(&sections[section_id], 0, sizeof(profiler_section_t));
    strncpy(sections[section_id].name, name_bak, PROFILER_NAME_LEN);
    sections[section_id].active = true;
    sections[section_id].min_us = UINT32_MAX;
}

void profiler_section_reset_all(void) {
    for (int i = 0; i < section_count; i++) {
        profiler_section_reset(i);
    }

    memset(task_cpu_window_us, 0, sizeof(task_cpu_window_us));
    memset(task_cpu_total_us, 0, sizeof(task_cpu_total_us));
    memset(task_ctx_switches, 0, sizeof(task_ctx_switches));

    idle_window_us = 0;
    irq_count = 0;
    irq_total_us = 0;
    irq_max_latency_us = 0;
    irq_total_latency_us = 0;
    total_context_switches = 0;
    overhead_accum_us = 0;

    window_start_us = profiler_time_us();
    window_duration_us = 0;

    dmesg_info("profiler: all stats reset");
}

/* ============================================================================
 * Task CPU tracking
 * ========================================================================== */

void profiler_task_switch(uint16_t old_task, uint16_t new_task) {
    if (!running) return;

    uint64_t now = profiler_time_us();

    /* Accumulate time for the outgoing task */
    if (old_task < PROFILER_MAX_TASKS && current_tracked_task == old_task) {
        uint32_t elapsed = (uint32_t)(now - current_task_start_us);
        task_cpu_window_us[old_task] += elapsed;
        task_cpu_total_us[old_task] += elapsed;
        task_ctx_switches[old_task]++;
    }

    /* Start tracking the incoming task */
    current_tracked_task = new_task;
    current_task_start_us = now;

    if (new_task < PROFILER_MAX_TASKS) {
        task_ctx_switches[new_task]++;
    }

    total_context_switches++;
}

int profiler_get_task_stats(profiler_task_stats_t *stats, int max_tasks) {
    if (!stats || max_tasks <= 0) return 0;

    /* Flush current task time if profiling is live */
    if (running && current_tracked_task < PROFILER_MAX_TASKS) {
        uint64_t now = profiler_time_us();
        uint32_t elapsed = (uint32_t)(now - current_task_start_us);
        task_cpu_window_us[current_tracked_task] += elapsed;
        task_cpu_total_us[current_tracked_task] += elapsed;
        current_task_start_us = now;
    }

    uint64_t win_us = window_duration_us;
    if (win_us == 0) {
        /* Use live window if no update has been called */
        win_us = profiler_time_us() - window_start_us;
        if (win_us == 0) win_us = 1;
    }

    int filled = 0;
    for (int i = 0; i < PROFILER_MAX_TASKS && filled < max_tasks; i++) {
        task_descriptor_t desc;
        if (!task_get_descriptor((uint16_t)i, &desc)) {
            continue;
        }
        if (desc.state == TASK_STATE_IDLE && desc.task_id == 0 && desc.name[0] == '\0') {
            continue;
        }

        profiler_task_stats_t *ts = &stats[filled];
        ts->task_id = desc.task_id;
        strncpy(ts->task_name, desc.name, 31);
        ts->task_name[31] = '\0';
        ts->cpu_us = task_cpu_window_us[i];
        ts->total_cpu_us = task_cpu_total_us[i];
        ts->context_switches = task_ctx_switches[i];
        ts->last_scheduled_ms = desc.total_runtime_ms;

        if (win_us > 0) {
            ts->cpu_percent = ((float)task_cpu_window_us[i] / (float)win_us) * 100.0f;
        } else {
            ts->cpu_percent = 0.0f;
        }

        filled++;
    }

    return filled;
}

/* ============================================================================
 * System stats
 * ========================================================================== */

int profiler_get_system_stats(profiler_system_stats_t *stats) {
    if (!stats) return -1;

    memset(stats, 0, sizeof(profiler_system_stats_t));

    uint64_t now = profiler_time_us();
    stats->uptime_ms = (uint32_t)((now - 0) / 1000);

    /* Compute idle from task 0 (idle task convention) */
    stats->idle_us = task_cpu_window_us[0];

    uint64_t win_us = window_duration_us;
    if (win_us == 0) {
        win_us = now - window_start_us;
        if (win_us == 0) win_us = 1;
    }

    float idle_pct = ((float)stats->idle_us / (float)win_us) * 100.0f;
    if (idle_pct > 100.0f) idle_pct = 100.0f;
    stats->cpu_usage_percent = 100.0f - idle_pct;
    if (stats->cpu_usage_percent < 0.0f) stats->cpu_usage_percent = 0.0f;

    stats->total_context_switches = total_context_switches;
    stats->irq_count = irq_count;
    stats->irq_max_latency_us = irq_max_latency_us;

    if (irq_count > 0) {
        stats->irq_avg_latency_us = (uint32_t)(irq_total_latency_us / irq_count);
    } else {
        stats->irq_avg_latency_us = 0;
    }

    stats->profiler_overhead_us = (uint32_t)overhead_accum_us;

    return 0;
}

/* ============================================================================
 * IRQ tracking
 * ========================================================================== */

void profiler_irq_enter(void) {
    if (!running) return;
    irq_enter_us = profiler_time_us();
    irq_count++;
}

void profiler_irq_exit(void) {
    if (!running) return;
    if (irq_enter_us == 0) return;

    uint64_t now = profiler_time_us();
    uint32_t duration = (uint32_t)(now - irq_enter_us);

    irq_total_us += duration;
    irq_total_latency_us += duration;

    if (duration > irq_max_latency_us) {
        irq_max_latency_us = duration;
    }

    irq_enter_us = 0;
}

/* ============================================================================
 * Periodic update (call every ~1 second)
 * ========================================================================== */

void profiler_update(void) {
    if (!running) return;

    uint64_t now = profiler_time_us();
    window_duration_us = now - window_start_us;

    /* Flush current task accumulation */
    if (current_tracked_task < PROFILER_MAX_TASKS) {
        uint32_t elapsed = (uint32_t)(now - current_task_start_us);
        task_cpu_window_us[current_tracked_task] += elapsed;
        task_cpu_total_us[current_tracked_task] += elapsed;
        current_task_start_us = now;
    }

    /* Snapshot idle from task 0 */
    idle_window_us = task_cpu_window_us[0];

    /* Reset window counters (keep totals) */
    window_start_us = now;
    memset(task_cpu_window_us, 0, sizeof(task_cpu_window_us));
}

/* ============================================================================
 * Printing
 * ========================================================================== */

void profiler_print_report(void) {
    printf("=== Profiler Section Report ===\n");
    printf("%-24s %8s %12s %8s %8s %8s %8s\n",
           "Section", "Calls", "Total(us)", "Min(us)", "Max(us)", "Avg(us)", "Last(us)");
    printf("------------------------------------------------------------------------\n");

    bool any = false;
    for (int i = 0; i < section_count; i++) {
        if (!sections[i].active) continue;
        any = true;

        profiler_section_t *s = &sections[i];
        uint32_t min_disp = (s->min_us == UINT32_MAX) ? 0 : s->min_us;

        printf("%-24s %8u %12llu %8u %8u %8u %8u\n",
               s->name,
               s->call_count,
               (unsigned long long)s->total_us,
               min_disp,
               s->max_us,
               s->avg_us,
               s->last_us);
    }

    if (!any) {
        printf("  (no sections registered)\n");
    }

    printf("\n");
}

void profiler_print_tasks(void) {
    profiler_task_stats_t stats[PROFILER_MAX_TASKS];
    int count = profiler_get_task_stats(stats, PROFILER_MAX_TASKS);

    printf("=== Task CPU Usage ===\n");
    printf("%-6s %-20s %8s %12s %12s %8s\n",
           "TID", "Name", "CPU %", "Window(us)", "Total(us)", "CtxSw");
    printf("--------------------------------------------------------------\n");

    if (count == 0) {
        printf("  (no tasks found)\n");
    }

    for (int i = 0; i < count; i++) {
        profiler_task_stats_t *t = &stats[i];
        printf("%-6u %-20s %7.1f%% %12u %12u %8u\n",
               t->task_id,
               t->task_name,
               (double)t->cpu_percent,
               t->cpu_us,
               t->total_cpu_us,
               t->context_switches);
    }

    printf("\n");
}
