/* timer.c - General-purpose timer HAL for littleOS
 *
 * Uses the Pico SDK alarm pool for software timers on top of
 * the RP2040/RP2350 hardware timer peripheral.
 */
#include "hal/timer.h"
#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#endif

/* ================================================================
 * Timer slot table
 * ================================================================ */

static timer_handle_t timers[TIMER_MAX_TIMERS];
static bool timer_initialized = false;

void timer_hal_init(void) {
    if (timer_initialized) return;
    memset(timers, 0, sizeof(timers));
    for (int i = 0; i < TIMER_MAX_TIMERS; i++) {
        timers[i].id = (uint8_t)i;
        timers[i]._sdk_alarm_id = -1;
    }
    timer_initialized = true;
}

/* ================================================================
 * Microsecond counter
 * ================================================================ */

uint64_t timer_hal_get_us(void) {
#ifdef PICO_BUILD
    return time_us_64();
#else
    return 0;
#endif
}

uint32_t timer_hal_get_us_32(void) {
#ifdef PICO_BUILD
    return time_us_32();
#else
    return 0;
#endif
}

void timer_hal_delay_us(uint32_t us) {
#ifdef PICO_BUILD
    busy_wait_us_32(us);
#else
    (void)us;
#endif
}

void timer_hal_delay_ms(uint32_t ms) {
#ifdef PICO_BUILD
    busy_wait_us_32(ms * 1000);
#else
    (void)ms;
#endif
}

/* ================================================================
 * Software timers (alarm-pool backed)
 * ================================================================ */

static int find_free_slot(void) {
    for (int i = 0; i < TIMER_MAX_TIMERS; i++) {
        if (!timers[i].active) return i;
    }
    return -1;
}

#ifdef PICO_BUILD

/* One-shot alarm callback — called from IRQ context */
static int64_t oneshot_alarm_cb(alarm_id_t id, void *user_data) {
    (void)id;
    timer_handle_t *t = (timer_handle_t *)user_data;
    if (!t || !t->active) return 0;

    if (t->callback) {
        t->callback(t->id, t->user_data);
    }

    t->active = false;
    t->_sdk_alarm_id = -1;
    return 0; /* don't reschedule */
}

/* Repeating timer callback */
static bool repeating_timer_cb(struct repeating_timer *rt) {
    timer_handle_t *t = (timer_handle_t *)rt->user_data;
    if (!t || !t->active || !t->callback) return false;

    bool keep_going = t->callback(t->id, t->user_data);
    if (!keep_going) {
        t->active = false;
        t->_sdk_alarm_id = -1;
    }
    return keep_going;
}

/* Storage for SDK repeating_timer structs (needed for lifetime) */
static struct repeating_timer sdk_repeating[TIMER_MAX_TIMERS];

#endif /* PICO_BUILD */

int timer_hal_set_oneshot(uint32_t delay_us, timer_callback_t cb, void *user_data) {
    if (!timer_initialized) timer_hal_init();
    if (!cb) return -1;

    int slot = find_free_slot();
    if (slot < 0) return -1;

    timers[slot].mode        = TIMER_ONE_SHOT;
    timers[slot].interval_us = delay_us;
    timers[slot].callback    = cb;
    timers[slot].user_data   = user_data;
    timers[slot].active      = true;

#ifdef PICO_BUILD
    alarm_id_t aid = add_alarm_in_us(delay_us, oneshot_alarm_cb,
                                     &timers[slot], true);
    if (aid < 0) {
        timers[slot].active = false;
        return -1;
    }
    timers[slot]._sdk_alarm_id = aid;
#endif

    return slot;
}

int timer_hal_set_repeating(uint32_t interval_us, timer_callback_t cb, void *user_data) {
    if (!timer_initialized) timer_hal_init();
    if (!cb || interval_us == 0) return -1;

    int slot = find_free_slot();
    if (slot < 0) return -1;

    timers[slot].mode        = TIMER_REPEATING;
    timers[slot].interval_us = interval_us;
    timers[slot].callback    = cb;
    timers[slot].user_data   = user_data;
    timers[slot].active      = true;

#ifdef PICO_BUILD
    sdk_repeating[slot].user_data = &timers[slot];
    bool ok = add_repeating_timer_us(-(int64_t)interval_us,
                                     repeating_timer_cb,
                                     &timers[slot],
                                     &sdk_repeating[slot]);
    if (!ok) {
        timers[slot].active = false;
        return -1;
    }
    timers[slot]._sdk_alarm_id = 1; /* mark as active */
#endif

    return slot;
}

int timer_hal_cancel(uint8_t timer_id) {
    if (timer_id >= TIMER_MAX_TIMERS) return -1;
    if (!timers[timer_id].active) return 0;

#ifdef PICO_BUILD
    if (timers[timer_id].mode == TIMER_REPEATING) {
        cancel_repeating_timer(&sdk_repeating[timer_id]);
    } else {
        if (timers[timer_id]._sdk_alarm_id >= 0)
            cancel_alarm((alarm_id_t)timers[timer_id]._sdk_alarm_id);
    }
#endif

    timers[timer_id].active = false;
    timers[timer_id]._sdk_alarm_id = -1;
    return 0;
}

void timer_hal_cancel_all(void) {
    for (uint8_t i = 0; i < TIMER_MAX_TIMERS; i++)
        timer_hal_cancel(i);
}

bool timer_hal_is_active(uint8_t timer_id) {
    if (timer_id >= TIMER_MAX_TIMERS) return false;
    return timers[timer_id].active;
}

uint8_t timer_hal_active_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < TIMER_MAX_TIMERS; i++) {
        if (timers[i].active) count++;
    }
    return count;
}

/* ================================================================
 * Status display
 * ================================================================ */

void timer_hal_print_status(void) {
    printf("\r\n--- Timer HAL Status ---\r\n");
    printf("Uptime: %llu us\r\n", (unsigned long long)timer_hal_get_us());
    printf("Slots:  %d / %d active\r\n", timer_hal_active_count(), TIMER_MAX_TIMERS);

    for (int i = 0; i < TIMER_MAX_TIMERS; i++) {
        if (!timers[i].active) continue;
        printf("  [%d] %s  interval=%lu us\r\n",
               i,
               timers[i].mode == TIMER_REPEATING ? "repeating" : "one-shot",
               (unsigned long)timers[i].interval_us);
    }
    printf("\r\n");
}
