// include/cron.h
// Cron/Scheduled Tasks System for littleOS

#ifndef LITTLEOS_CRON_H
#define LITTLEOS_CRON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * littleOS Cron System
 *
 * Provides periodic command execution through the shell.
 * Two scheduling modes:
 *   - Interval-based: repeat every N milliseconds
 *   - One-shot (simple schedule): run once after N milliseconds, then disable
 */

/* ============================================================================
 * Cron Definitions
 * ========================================================================== */

#define CRON_MAX_JOBS        4
#define CRON_MAX_NAME        24
#define CRON_MAX_COMMAND     128

typedef enum {
    CRON_MODE_INTERVAL,     /* Repeating every interval_ms             */
    CRON_MODE_ONESHOT,      /* Run once after interval_ms, then disable */
} cron_mode_t;

typedef struct {
    char        name[CRON_MAX_NAME];        /* Job name                 */
    char        command[CRON_MAX_COMMAND];   /* Shell command to execute */
    uint32_t    interval_ms;                /* Interval / delay in ms   */
    uint32_t    last_run_ms;                /* Timestamp of last run    */
    uint32_t    next_run_ms;                /* Timestamp of next run    */
    bool        enabled;                    /* Whether job is active    */
    uint32_t    run_count;                  /* Total execution count    */
    cron_mode_t mode;                       /* Scheduling mode          */
    bool        in_use;                     /* Slot occupied            */
} cron_job_t;

/* ============================================================================
 * Cron API
 * ========================================================================== */

/**
 * Initialize the cron system
 * Clears all job slots.
 */
void cron_init(void);

/**
 * Add a periodic (interval-based) cron job
 *
 * @param name        Unique job name (max CRON_MAX_NAME-1 chars)
 * @param command     Shell command to execute each interval
 * @param interval_ms Repeat interval in milliseconds
 * @return 0 on success, -1 on error (full, duplicate name, bad params)
 */
int cron_add(const char *name, const char *command, uint32_t interval_ms);

/**
 * Add a one-shot scheduled job (runs once then disables itself)
 *
 * @param name        Unique job name
 * @param command     Shell command to execute
 * @param delay_ms    Delay before execution in milliseconds
 * @return 0 on success, -1 on error
 */
int cron_add_oneshot(const char *name, const char *command, uint32_t delay_ms);

/**
 * Remove a cron job by name
 *
 * @param name Job name
 * @return 0 on success, -1 if not found
 */
int cron_remove(const char *name);

/**
 * Enable a cron job by name
 *
 * @param name Job name
 * @return 0 on success, -1 if not found
 */
int cron_enable(const char *name);

/**
 * Disable a cron job by name
 *
 * @param name Job name
 * @return 0 on success, -1 if not found
 */
int cron_disable(const char *name);

/**
 * Tick function — call from main loop to check and execute due jobs.
 * Compares current time against next_run_ms for each enabled job.
 */
void cron_tick(void);

/**
 * List all cron jobs into a buffer
 *
 * @param buf    Output buffer
 * @param buflen Buffer size
 * @return Number of bytes written
 */
int cron_list(char *buf, size_t buflen);

/**
 * Get the number of active (in-use) cron jobs
 *
 * @return Job count
 */
int cron_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_CRON_H */
