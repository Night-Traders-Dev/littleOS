#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file watchdog.h
 * @brief Watchdog timer for automatic crash recovery
 * 
 * The watchdog timer can detect system hangs and automatically reboot
 * the device. It must be "fed" (updated) periodically or it will trigger
 * a system reset.
 */

/**
 * @brief Watchdog timeout values in milliseconds
 */
#define WATCHDOG_TIMEOUT_MIN_MS     1
#define WATCHDOG_TIMEOUT_MAX_MS     8388    // ~8.3 seconds (RP2040 max)
#define WATCHDOG_TIMEOUT_DEFAULT_MS 5000    // 5 seconds

/**
 * @brief Watchdog reset reasons
 */
typedef enum {
    WATCHDOG_RESET_NONE = 0,        // No watchdog reset
    WATCHDOG_RESET_TIMEOUT = 1,     // Watchdog timeout
    WATCHDOG_RESET_FORCED = 2,      // Forced reboot via watchdog
} watchdog_reset_reason_t;

/**
 * @brief Initialize watchdog timer
 * @param timeout_ms Timeout in milliseconds (1-8388)
 * @return true if successful, false otherwise
 */
bool watchdog_init(uint32_t timeout_ms);

/**
 * @brief Feed the watchdog (reset countdown)
 * Call this periodically to prevent reset
 */
void watchdog_feed(void);

/**
 * @brief Enable watchdog timer
 * @param timeout_ms Timeout in milliseconds
 * @return true if successful
 */
bool watchdog_enable(uint32_t timeout_ms);

/**
 * @brief Disable watchdog timer
 * Note: On RP2040, watchdog cannot be fully disabled once enabled,
 * but we can set a very long timeout
 */
void watchdog_disable(void);

/**
 * @brief Check if system was reset by watchdog
 * @return Reset reason
 */
watchdog_reset_reason_t watchdog_get_reset_reason(void);

/**
 * @brief Get time until watchdog timeout
 * @return Milliseconds remaining (0 if disabled)
 */
uint32_t watchdog_get_time_remaining_ms(void);

/**
 * @brief Force immediate watchdog reboot
 * @param delay_ms Delay before reboot (0 for immediate)
 */
void watchdog_reboot(uint32_t delay_ms);

/**
 * @brief Check if watchdog is enabled
 * @return true if enabled
 */
bool watchdog_is_enabled(void);

/**
 * @brief Get watchdog statistics
 * @param total_feeds Pointer to store total feed count
 * @param last_feed_time_ms Pointer to store last feed time
 * @param timeout_ms Pointer to store current timeout
 */
void watchdog_get_stats(uint32_t* total_feeds, uint32_t* last_feed_time_ms,
                        uint32_t* timeout_ms);

/**
 * @brief Clear watchdog reset reason
 */
void watchdog_clear_reset_reason(void);

#endif // WATCHDOG_H
