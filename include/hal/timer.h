/* timer.h - General-Purpose Timer HAL for littleOS
 *
 * Wraps the RP2040/RP2350 hardware alarm system (4 hardware alarms on
 * a shared 64-bit microsecond counter) into a simple one-shot /
 * repeating timer API.
 *
 * For high-level scheduling (cron, task delays) use the kernel
 * scheduler.  This HAL is for precise, interrupt-driven timing:
 * pulse measurement, frequency counting, periodic sampling, etc.
 */
#ifndef LITTLEOS_HAL_TIMER_H
#define LITTLEOS_HAL_TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of software timers (backed by SDK alarm pool) */
#define TIMER_MAX_TIMERS    8

/* Timer modes */
typedef enum {
    TIMER_ONE_SHOT  = 0,
    TIMER_REPEATING = 1,
} timer_mode_t;

/* Timer callback: called from interrupt context.
 * Return true from a repeating timer callback to keep it running,
 * false to stop it. */
typedef bool (*timer_callback_t)(uint8_t timer_id, void *user_data);

/* Timer handle (opaque to callers) */
typedef struct {
    uint8_t          id;
    timer_mode_t     mode;
    uint32_t         interval_us;
    timer_callback_t callback;
    void            *user_data;
    bool             active;
    int64_t          _sdk_alarm_id;  /* internal: pico SDK alarm id */
} timer_handle_t;

/* ---- Lifecycle ---- */

/* Initialise the timer subsystem.  Safe to call multiple times. */
void timer_hal_init(void);

/* ---- Microsecond counter ---- */

/* Read the free-running 64-bit microsecond counter. */
uint64_t timer_hal_get_us(void);

/* Read the lower 32 bits (wraps every ~71 minutes). */
uint32_t timer_hal_get_us_32(void);

/* Busy-wait for the given number of microseconds. */
void timer_hal_delay_us(uint32_t us);

/* Busy-wait for the given number of milliseconds. */
void timer_hal_delay_ms(uint32_t ms);

/* ---- Software timers ---- */

/* Create a one-shot timer that fires after `delay_us` microseconds.
 * Returns a timer ID (0 .. TIMER_MAX_TIMERS-1), or -1 on failure. */
int timer_hal_set_oneshot(uint32_t delay_us, timer_callback_t cb, void *user_data);

/* Create a repeating timer that fires every `interval_us` microseconds.
 * Returns a timer ID, or -1 on failure. */
int timer_hal_set_repeating(uint32_t interval_us, timer_callback_t cb, void *user_data);

/* Cancel a running timer.  Safe to call on an inactive timer. */
int timer_hal_cancel(uint8_t timer_id);

/* Cancel all running timers. */
void timer_hal_cancel_all(void);

/* Check if a timer is currently active. */
bool timer_hal_is_active(uint8_t timer_id);

/* Get the number of active timers. */
uint8_t timer_hal_active_count(void);

/* ---- Measurement helpers ---- */

/* Start a timing measurement.  Returns an opaque timestamp. */
static inline uint64_t timer_hal_start_measurement(void) {
    return timer_hal_get_us();
}

/* End a measurement.  Returns elapsed microseconds since start. */
static inline uint32_t timer_hal_elapsed_us(uint64_t start) {
    return (uint32_t)(timer_hal_get_us() - start);
}

/* ---- Status ---- */

/* Print timer subsystem status to console. */
void timer_hal_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_TIMER_H */
