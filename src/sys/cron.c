// src/sys/cron.c
// Cron/Scheduled Tasks implementation for littleOS

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "cron.h"

/* Command dispatch — implemented in shell.c */
extern void shell_execute_command(const char *cmd);

/* ============================================================================
 * Static Job Table
 * ========================================================================== */

static cron_job_t cron_jobs[CRON_MAX_JOBS];

/* ============================================================================
 * Internal Helpers
 * ========================================================================== */

static cron_job_t *cron_find(const char *name)
{
    for (int i = 0; i < CRON_MAX_JOBS; i++) {
        if (cron_jobs[i].in_use &&
            strncmp(cron_jobs[i].name, name, CRON_MAX_NAME) == 0) {
            return &cron_jobs[i];
        }
    }
    return NULL;
}

static cron_job_t *cron_find_free_slot(void)
{
    for (int i = 0; i < CRON_MAX_JOBS; i++) {
        if (!cron_jobs[i].in_use)
            return &cron_jobs[i];
    }
    return NULL;
}

/* ============================================================================
 * Public API
 * ========================================================================== */

void cron_init(void)
{
    memset(cron_jobs, 0, sizeof(cron_jobs));
}

int cron_add(const char *name, const char *command, uint32_t interval_ms)
{
    if (!name || !command || interval_ms == 0)
        return -1;

    if (cron_find(name))
        return -1;  /* duplicate name */

    cron_job_t *job = cron_find_free_slot();
    if (!job)
        return -1;  /* table full */

    memset(job, 0, sizeof(*job));
    strncpy(job->name, name, CRON_MAX_NAME - 1);
    job->name[CRON_MAX_NAME - 1] = '\0';
    strncpy(job->command, command, CRON_MAX_COMMAND - 1);
    job->command[CRON_MAX_COMMAND - 1] = '\0';
    job->interval_ms = interval_ms;
    job->mode        = CRON_MODE_INTERVAL;
    job->enabled     = true;
    job->in_use      = true;
    job->run_count   = 0;
    job->last_run_ms = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    job->next_run_ms = now + interval_ms;

    return 0;
}

int cron_add_oneshot(const char *name, const char *command, uint32_t delay_ms)
{
    if (!name || !command || delay_ms == 0)
        return -1;

    if (cron_find(name))
        return -1;

    cron_job_t *job = cron_find_free_slot();
    if (!job)
        return -1;

    memset(job, 0, sizeof(*job));
    strncpy(job->name, name, CRON_MAX_NAME - 1);
    job->name[CRON_MAX_NAME - 1] = '\0';
    strncpy(job->command, command, CRON_MAX_COMMAND - 1);
    job->command[CRON_MAX_COMMAND - 1] = '\0';
    job->interval_ms = delay_ms;
    job->mode        = CRON_MODE_ONESHOT;
    job->enabled     = true;
    job->in_use      = true;
    job->run_count   = 0;
    job->last_run_ms = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    job->next_run_ms = now + delay_ms;

    return 0;
}

int cron_remove(const char *name)
{
    cron_job_t *job = cron_find(name);
    if (!job)
        return -1;

    memset(job, 0, sizeof(*job));
    return 0;
}

int cron_enable(const char *name)
{
    cron_job_t *job = cron_find(name);
    if (!job)
        return -1;

    if (!job->enabled) {
        job->enabled = true;
        /* Reset next_run relative to now */
        uint32_t now = to_ms_since_boot(get_absolute_time());
        job->next_run_ms = now + job->interval_ms;
    }
    return 0;
}

int cron_disable(const char *name)
{
    cron_job_t *job = cron_find(name);
    if (!job)
        return -1;

    job->enabled = false;
    return 0;
}

void cron_tick(void)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());

    for (int i = 0; i < CRON_MAX_JOBS; i++) {
        cron_job_t *job = &cron_jobs[i];

        if (!job->in_use || !job->enabled)
            continue;

        if (now >= job->next_run_ms) {
            /* Execute the command through the shell */
            shell_execute_command(job->command);

            job->last_run_ms = now;
            job->run_count++;

            if (job->mode == CRON_MODE_ONESHOT) {
                job->enabled = false;
            } else {
                /* Schedule next run */
                job->next_run_ms = now + job->interval_ms;
            }
        }
    }
}

int cron_list(char *buf, size_t buflen)
{
    int written = 0;
    int n;

    n = snprintf(buf + written, buflen - written,
                 "%-16s %-8s %-10s %-12s %-12s %-5s\r\n",
                 "NAME", "MODE", "INTERVAL", "LAST RUN", "NEXT RUN", "RUNS");
    if (n > 0) written += n;

    n = snprintf(buf + written, buflen - written,
                 "--------------------------------------------------------------\r\n");
    if (n > 0) written += n;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    for (int i = 0; i < CRON_MAX_JOBS; i++) {
        cron_job_t *job = &cron_jobs[i];
        if (!job->in_use)
            continue;

        const char *mode_str = (job->mode == CRON_MODE_ONESHOT) ? "oneshot" : "interval";
        const char *state    = job->enabled ? "ON" : "OFF";

        /* Show interval in human-readable form */
        char interval_str[16];
        if (job->interval_ms >= 60000)
            snprintf(interval_str, sizeof(interval_str), "%lum%lus",
                     (unsigned long)(job->interval_ms / 60000),
                     (unsigned long)((job->interval_ms % 60000) / 1000));
        else if (job->interval_ms >= 1000)
            snprintf(interval_str, sizeof(interval_str), "%lus",
                     (unsigned long)(job->interval_ms / 1000));
        else
            snprintf(interval_str, sizeof(interval_str), "%lums",
                     (unsigned long)job->interval_ms);

        /* Time since last run */
        char last_str[16];
        if (job->last_run_ms == 0)
            snprintf(last_str, sizeof(last_str), "never");
        else
            snprintf(last_str, sizeof(last_str), "%lus ago",
                     (unsigned long)((now - job->last_run_ms) / 1000));

        /* Time until next run */
        char next_str[16];
        if (!job->enabled)
            snprintf(next_str, sizeof(next_str), "disabled");
        else if (now >= job->next_run_ms)
            snprintf(next_str, sizeof(next_str), "due now");
        else
            snprintf(next_str, sizeof(next_str), "in %lus",
                     (unsigned long)((job->next_run_ms - now) / 1000));

        n = snprintf(buf + written, buflen - written,
                     "%-16s %-8s %-10s %-12s %-12s %lu [%s]\r\n",
                     job->name, mode_str, interval_str, last_str, next_str,
                     (unsigned long)job->run_count, state);
        if (n > 0) written += n;

        /* Also show the command */
        n = snprintf(buf + written, buflen - written,
                     "  cmd: %s\r\n", job->command);
        if (n > 0) written += n;
    }

    if (cron_get_count() == 0) {
        n = snprintf(buf + written, buflen - written, "(no cron jobs)\r\n");
        if (n > 0) written += n;
    }

    return written;
}

int cron_get_count(void)
{
    int count = 0;
    for (int i = 0; i < CRON_MAX_JOBS; i++) {
        if (cron_jobs[i].in_use)
            count++;
    }
    return count;
}
