// src/sys/logcat.c
// Structured logging system (logcat) for littleOS

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pico/stdlib.h"
#include "logcat.h"

/* ============================================================================
 * Static Ring Buffer
 * ========================================================================== */

static log_entry_t ring_buf[LOGCAT_MAX_ENTRIES];
static int ring_head  = 0;   /* Next write position */
static int ring_count = 0;   /* Number of entries in buffer */
static log_level_t global_min_level = LOG_INFO;

/* ============================================================================
 * Internal Helpers
 * ========================================================================== */

static uint32_t logcat_timestamp(void)
{
#ifdef PICO_BUILD
    return to_ms_since_boot(get_absolute_time());
#else
    return 0;
#endif
}

/* ============================================================================
 * Public API
 * ========================================================================== */

void logcat_init(void)
{
    memset(ring_buf, 0, sizeof(ring_buf));
    ring_head  = 0;
    ring_count = 0;
    global_min_level = LOG_INFO;
}

void logcat_log(log_level_t level, const char *tag, const char *fmt, ...)
{
    if (level < global_min_level)
        return;

    log_entry_t *entry = &ring_buf[ring_head];

    entry->timestamp_ms = logcat_timestamp();
    entry->level = level;

    if (tag) {
        strncpy(entry->tag, tag, LOGCAT_MAX_TAG_LEN - 1);
        entry->tag[LOGCAT_MAX_TAG_LEN - 1] = '\0';
    } else {
        entry->tag[0] = '\0';
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->msg, LOGCAT_MAX_MSG_LEN, fmt, args);
    va_end(args);

    ring_head = (ring_head + 1) % LOGCAT_MAX_ENTRIES;
    if (ring_count < LOGCAT_MAX_ENTRIES)
        ring_count++;
}

int logcat_dump(char *buf, size_t buflen, log_level_t min_level, const char *tag_filter)
{
    if (!buf || buflen == 0)
        return 0;

    buf[0] = '\0';
    size_t offset = 0;
    int matched = 0;

    /* Calculate the index of the oldest entry */
    int start;
    if (ring_count < LOGCAT_MAX_ENTRIES)
        start = 0;
    else
        start = ring_head;  /* Oldest entry when buffer is full */

    for (int i = 0; i < ring_count; i++) {
        int idx = (start + i) % LOGCAT_MAX_ENTRIES;
        log_entry_t *e = &ring_buf[idx];

        /* Filter by level */
        if (e->level < min_level)
            continue;

        /* Filter by tag substring */
        if (tag_filter && tag_filter[0] != '\0') {
            if (strstr(e->tag, tag_filter) == NULL)
                continue;
        }

        int written = snprintf(buf + offset, buflen - offset,
                               "[%lu] %s/%s: %s\r\n",
                               (unsigned long)e->timestamp_ms,
                               logcat_level_str(e->level),
                               e->tag,
                               e->msg);

        if (written < 0 || (size_t)written >= buflen - offset)
            break;

        offset += (size_t)written;
        matched++;
    }

    return matched;
}

void logcat_clear(void)
{
    memset(ring_buf, 0, sizeof(ring_buf));
    ring_head  = 0;
    ring_count = 0;
}

int logcat_get_count(void)
{
    return ring_count;
}

int logcat_set_level(log_level_t level)
{
    if (level < LOG_VERBOSE || level > LOG_FATAL)
        return -1;
    global_min_level = level;
    return 0;
}

log_level_t logcat_get_level(void)
{
    return global_min_level;
}

const char *logcat_level_str(log_level_t level)
{
    switch (level) {
        case LOG_VERBOSE: return "V";
        case LOG_DEBUG:   return "D";
        case LOG_INFO:    return "I";
        case LOG_WARN:    return "W";
        case LOG_ERROR:   return "E";
        case LOG_FATAL:   return "F";
        default:          return "?";
    }
}

log_level_t logcat_parse_level(const char *str)
{
    if (!str)
        return LOG_INFO;

    if (strcmp(str, "V") == 0 || strcmp(str, "verbose") == 0 || strcmp(str, "VERBOSE") == 0)
        return LOG_VERBOSE;
    if (strcmp(str, "D") == 0 || strcmp(str, "debug") == 0 || strcmp(str, "DEBUG") == 0)
        return LOG_DEBUG;
    if (strcmp(str, "I") == 0 || strcmp(str, "info") == 0 || strcmp(str, "INFO") == 0)
        return LOG_INFO;
    if (strcmp(str, "W") == 0 || strcmp(str, "warn") == 0 || strcmp(str, "WARN") == 0)
        return LOG_WARN;
    if (strcmp(str, "E") == 0 || strcmp(str, "error") == 0 || strcmp(str, "ERROR") == 0)
        return LOG_ERROR;
    if (strcmp(str, "F") == 0 || strcmp(str, "fatal") == 0 || strcmp(str, "FATAL") == 0)
        return LOG_FATAL;

    return LOG_INFO;  /* Default fallback */
}
