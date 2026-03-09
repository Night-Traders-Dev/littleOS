/* profiler.h - Runtime Profiling for littleOS */
#ifndef LITTLEOS_PROFILER_H
#define LITTLEOS_PROFILER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROFILER_MAX_SECTIONS   16
#define PROFILER_NAME_LEN       24
#define PROFILER_HISTORY_LEN    64  /* Samples for rolling average */

/* Profiling section (named code region) */
typedef struct {
    char     name[PROFILER_NAME_LEN];
    bool     active;
    uint32_t call_count;
    uint64_t total_us;       /* Total time in microseconds */
    uint32_t min_us;         /* Minimum single call */
    uint32_t max_us;         /* Maximum single call */
    uint32_t last_us;        /* Last call duration */
    uint32_t avg_us;         /* Rolling average */
    uint32_t start_us;       /* Entry timestamp (internal) */
    /* Rolling window */
    uint32_t history[PROFILER_HISTORY_LEN];
    uint8_t  history_idx;
    uint8_t  history_count;
} profiler_section_t;

/* Task CPU usage stats */
typedef struct {
    uint16_t task_id;
    char     task_name[32];
    uint32_t cpu_us;         /* CPU time in current window */
    uint32_t total_cpu_us;   /* Total CPU time ever */
    float    cpu_percent;    /* CPU usage percentage */
    uint32_t context_switches;
    uint32_t last_scheduled_ms;
} profiler_task_stats_t;

/* System-wide profiling stats */
typedef struct {
    uint32_t uptime_ms;
    uint32_t idle_us;            /* Idle time in current window */
    float    cpu_usage_percent;  /* Overall CPU usage */
    uint32_t total_context_switches;
    uint32_t irq_count;          /* Total interrupt count */
    uint32_t irq_max_latency_us; /* Worst-case IRQ latency */
    uint32_t irq_avg_latency_us;
    uint32_t profiler_overhead_us; /* Profiling overhead estimate */
} profiler_system_stats_t;

/* Init profiler */
int profiler_init(void);

/* Start/stop profiling globally */
int profiler_start(void);
int profiler_stop(void);
bool profiler_is_running(void);

/* Section profiling - bracket code with enter/exit */
int profiler_section_register(const char *name);
void profiler_section_enter(int section_id);
void profiler_section_exit(int section_id);

/* Get section stats */
int profiler_section_stats(int section_id, profiler_section_t *stats);

/* Reset section stats */
void profiler_section_reset(int section_id);
void profiler_section_reset_all(void);

/* Task CPU tracking */
void profiler_task_switch(uint16_t old_task, uint16_t new_task);
int profiler_get_task_stats(profiler_task_stats_t *stats, int max_tasks);

/* System stats */
int profiler_get_system_stats(profiler_system_stats_t *stats);

/* IRQ tracking */
void profiler_irq_enter(void);
void profiler_irq_exit(void);

/* Update profiler window (call periodically, e.g., every 1 second) */
void profiler_update(void);

/* Print formatted profiling report */
void profiler_print_report(void);

/* Print task CPU usage */
void profiler_print_tasks(void);

/* Convenience macro for section profiling */
#define PROFILE_SECTION(name, code) do { \
    static int _prof_id = -1; \
    if (_prof_id < 0) _prof_id = profiler_section_register(name); \
    profiler_section_enter(_prof_id); \
    { code; } \
    profiler_section_exit(_prof_id); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_PROFILER_H */
