// src/sys/watchpoint.c - Memory Watchpoint System for littleOS (RP2040)

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "watchpoint.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/* ============================================================================
 * Internal state
 * ========================================================================== */

static watchpoint_t watchpoints[WATCHPOINT_MAX];

/* ============================================================================
 * Helpers
 * ========================================================================== */

static const char *wp_type_str(watchpoint_type_t type)
{
    switch (type) {
        case WP_TYPE_WRITE: return "WRITE";
        case WP_TYPE_READ:  return "READ ";
        case WP_TYPE_VALUE: return "VALUE";
        default:            return "?????";
    }
}

static uint32_t wp_read_value(uint32_t addr, uint32_t size)
{
#ifdef PICO_BUILD
    switch (size) {
        case 1:  return (uint32_t)(*(volatile uint8_t *)addr);
        case 2:  return (uint32_t)(*(volatile uint16_t *)addr);
        case 4:  return *(volatile uint32_t *)addr;
        default: return 0;
    }
#else
    /* Off-target: return 0 (no real memory to read) */
    (void)addr;
    (void)size;
    return 0;
#endif
}

/* ============================================================================
 * Public API
 * ========================================================================== */

void watchpoint_init(void)
{
    memset(watchpoints, 0, sizeof(watchpoints));
}

int watchpoint_add(uint32_t addr, uint32_t size, watchpoint_type_t type,
                   const char *label)
{
    /* Validate size */
    if (size != 1 && size != 2 && size != 4)
        return -1;

    /* Find a free slot */
    int slot = -1;
    for (int i = 0; i < WATCHPOINT_MAX; i++) {
        if (!watchpoints[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;

    watchpoint_t *wp = &watchpoints[slot];
    wp->address       = addr;
    wp->size          = size;
    wp->type          = type;
    wp->last_value    = wp_read_value(addr, size);
    wp->trigger_count = 0;
    wp->active        = true;

    if (label) {
        strncpy(wp->label, label, sizeof(wp->label) - 1);
        wp->label[sizeof(wp->label) - 1] = '\0';
    } else {
        snprintf(wp->label, sizeof(wp->label), "wp%d", slot);
    }

    return slot;
}

int watchpoint_remove(int index)
{
    if (index < 0 || index >= WATCHPOINT_MAX)
        return -1;
    if (!watchpoints[index].active)
        return -1;

    memset(&watchpoints[index], 0, sizeof(watchpoint_t));
    return 0;
}

void watchpoint_check(void)
{
    for (int i = 0; i < WATCHPOINT_MAX; i++) {
        watchpoint_t *wp = &watchpoints[i];
        if (!wp->active)
            continue;

        uint32_t current = wp_read_value(wp->address, wp->size);

        if (current != wp->last_value) {
            wp->trigger_count++;
            printf("[WATCHPOINT] %s @ 0x%08X: 0x%08X -> 0x%08X (triggers=%u)\r\n",
                   wp->label,
                   (unsigned)wp->address,
                   (unsigned)wp->last_value,
                   (unsigned)current,
                   (unsigned)wp->trigger_count);
            wp->last_value = current;
        }
    }
}

int watchpoint_list(char *buf, size_t buflen)
{
    if (!buf || buflen == 0)
        return -1;

    int written = 0;
    int active_count = 0;

    for (int i = 0; i < WATCHPOINT_MAX; i++) {
        watchpoint_t *wp = &watchpoints[i];
        if (!wp->active)
            continue;

        active_count++;
        int n = snprintf(buf + written, buflen - (size_t)written,
                         "[%d] %-12s %s  addr=0x%08X  size=%u  val=0x%08X  triggers=%u\r\n",
                         i,
                         wp->label,
                         wp_type_str(wp->type),
                         (unsigned)wp->address,
                         (unsigned)wp->size,
                         (unsigned)wp->last_value,
                         (unsigned)wp->trigger_count);

        if (n < 0 || (size_t)(written + n) >= buflen)
            break;
        written += n;
    }

    if (active_count == 0) {
        int n = snprintf(buf + written, buflen - (size_t)written,
                         "No active watchpoints.\r\n");
        if (n > 0 && (size_t)(written + n) < buflen)
            written += n;
    }

    return written;
}

int watchpoint_get_count(void)
{
    int count = 0;
    for (int i = 0; i < WATCHPOINT_MAX; i++) {
        if (watchpoints[i].active)
            count++;
    }
    return count;
}
