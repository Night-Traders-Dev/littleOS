// src/sys/trace.c - Execution Trace Buffer for littleOS (RP2040)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "trace.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/timer.h"
#endif

/* ============================================================================
 * Platform timing abstraction
 * ========================================================================== */

#ifndef PICO_BUILD
static uint32_t fake_timer_us = 0;
#endif

static inline uint32_t trace_time_us(void) {
#ifdef PICO_BUILD
    return time_us_32();
#else
    fake_timer_us += 100;
    return fake_timer_us;
#endif
}

/* ============================================================================
 * Internal state — static ring buffer
 * ========================================================================== */

static trace_entry_t trace_buf[TRACE_MAX_ENTRIES];
static int trace_head  = 0;   /* next write position */
static int trace_count = 0;   /* number of valid entries */
static bool trace_enabled = false;

/* ============================================================================
 * Type name lookup
 * ========================================================================== */

static const char *trace_type_str(trace_type_t type) {
    switch (type) {
        case TRACE_ENTER: return "ENTER";
        case TRACE_EXIT:  return "EXIT ";
        case TRACE_EVENT: return "EVENT";
        case TRACE_ISR:   return "ISR  ";
        case TRACE_SCHED: return "SCHED";
        default:          return "?????";
    }
}

/* ============================================================================
 * Public API
 * ========================================================================== */

void trace_init(void)
{
    memset(trace_buf, 0, sizeof(trace_buf));
    trace_head  = 0;
    trace_count = 0;
    trace_enabled = false;
}

void trace_record(trace_type_t type, const char *name, uint32_t arg)
{
    if (!trace_enabled)
        return;

    trace_entry_t *entry = &trace_buf[trace_head];
    entry->timestamp_us = trace_time_us();
    entry->type = type;
    entry->arg  = arg;

    if (name) {
        strncpy(entry->name, name, TRACE_MAX_NAME_LEN - 1);
        entry->name[TRACE_MAX_NAME_LEN - 1] = '\0';
    } else {
        entry->name[0] = '\0';
    }

    trace_head = (trace_head + 1) % TRACE_MAX_ENTRIES;
    if (trace_count < TRACE_MAX_ENTRIES)
        trace_count++;
}

void trace_clear(void)
{
    memset(trace_buf, 0, sizeof(trace_buf));
    trace_head  = 0;
    trace_count = 0;
}

int trace_dump(char *buf, size_t buflen)
{
    if (!buf || buflen == 0)
        return -1;

    int written = 0;
    int start;

    if (trace_count < TRACE_MAX_ENTRIES) {
        start = 0;
    } else {
        start = trace_head;  /* oldest entry */
    }

    for (int i = 0; i < trace_count; i++) {
        int idx = (start + i) % TRACE_MAX_ENTRIES;
        trace_entry_t *e = &trace_buf[idx];

        int n = snprintf(buf + written, buflen - (size_t)written,
                         "[%10u] %s %s (arg=0x%04X)\r\n",
                         (unsigned)e->timestamp_us,
                         trace_type_str(e->type),
                         e->name,
                         (unsigned)e->arg);

        if (n < 0 || (size_t)(written + n) >= buflen)
            break;
        written += n;
    }

    return written;
}

int trace_get_count(void)
{
    return trace_count;
}

bool trace_is_enabled(void)
{
    return trace_enabled;
}

void trace_enable(bool enable)
{
    trace_enabled = enable;
}
