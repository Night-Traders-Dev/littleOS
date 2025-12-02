// src/watchdog.c
// Watchdog Timer Implementation
#include "watchdog.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"

// Watchdog state
static struct {
    bool enabled;
    uint32_t timeout_ms;
    uint32_t feed_count;
    uint32_t last_feed_time_ms;
    watchdog_reset_reason_t last_reset_reason;
} wdt_state = {
    .enabled = false,
    .timeout_ms = 0,
    .feed_count = 0,
    .last_feed_time_ms = 0,
    .last_reset_reason = WATCHDOG_RESET_NONE
};

/**
 * @brief Initialize watchdog timer
 */
bool watchdog_init(uint32_t timeout_ms) {
    if (timeout_ms < WATCHDOG_TIMEOUT_MIN_MS || 
        timeout_ms > WATCHDOG_TIMEOUT_MAX_MS) {
        printf("Watchdog: Invalid timeout %u ms (range: %u-%u)\r\n",
               timeout_ms, WATCHDOG_TIMEOUT_MIN_MS, WATCHDOG_TIMEOUT_MAX_MS);
        return false;
    }
    
    // Check if we were reset by watchdog
    if (watchdog_caused_reboot()) {
        wdt_state.last_reset_reason = WATCHDOG_RESET_TIMEOUT;
        printf("Watchdog: System recovered from watchdog reset!\r\n");
    } else {
        wdt_state.last_reset_reason = WATCHDOG_RESET_NONE;
    }
    
    wdt_state.timeout_ms = timeout_ms;
    wdt_state.enabled = false;
    wdt_state.feed_count = 0;
    wdt_state.last_feed_time_ms = to_ms_since_boot(get_absolute_time());
    
    printf("Watchdog: Initialized (timeout: %u ms)\r\n", timeout_ms);
    
    return true;
}

/**
 * @brief Enable watchdog timer
 */
bool watchdog_enable(uint32_t timeout_ms) {
    if (timeout_ms < WATCHDOG_TIMEOUT_MIN_MS || 
        timeout_ms > WATCHDOG_TIMEOUT_MAX_MS) {
        return false;
    }
    
    wdt_state.timeout_ms = timeout_ms;
    
    // Enable watchdog with specified timeout using Pico SDK function
    // Note: Pico SDK watchdog_enable takes (delay_ms, pause_on_debug)
    watchdog_enable(timeout_ms, 1);  // Pause on debug
    
    wdt_state.enabled = true;
    wdt_state.feed_count = 0;
    wdt_state.last_feed_time_ms = to_ms_since_boot(get_absolute_time());
    
    printf("Watchdog: Enabled (timeout: %u ms)\r\n", timeout_ms);
    
    return true;
}

/**
 * @brief Feed the watchdog
 */
void watchdog_feed(void) {
    if (!wdt_state.enabled) {
        return;
    }
    
    // Update the watchdog timer using Pico SDK function
    watchdog_update();
    
    wdt_state.feed_count++;
    wdt_state.last_feed_time_ms = to_ms_since_boot(get_absolute_time());
}

/**
 * @brief Disable watchdog timer
 * Note: RP2040 watchdog cannot be truly disabled once enabled,
 * but we can set a very long timeout
 */
void watchdog_disable(void) {
    if (!wdt_state.enabled) {
        return;
    }
    
    // Set maximum timeout (essentially disabled)
    // Note: SDK's watchdog_enable takes (delay_ms, pause_on_debug)
    watchdog_enable(WATCHDOG_TIMEOUT_MAX_MS, 1);
    
    wdt_state.enabled = false;
    
    printf("Watchdog: Disabled (set to max timeout)\r\n");
}

/**
 * @brief Get reset reason
 */
watchdog_reset_reason_t watchdog_get_reset_reason(void) {
    return wdt_state.last_reset_reason;
}

/**
 * @brief Get time remaining until timeout
 */
uint32_t watchdog_get_time_remaining_ms(void) {
    if (!wdt_state.enabled) {
        return 0;
    }
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed = current_time - wdt_state.last_feed_time_ms;
    
    if (elapsed >= wdt_state.timeout_ms) {
        return 0;
    }
    
    return wdt_state.timeout_ms - elapsed;
}

/**
 * @brief Force immediate reboot via watchdog
 */
void watchdog_reboot(uint32_t delay_ms) {
    printf("Watchdog: Forcing reboot in %u ms...\r\n", delay_ms);
    
    if (delay_ms == 0) {
        delay_ms = 1;  // Minimum 1ms
    }
    
    wdt_state.last_reset_reason = WATCHDOG_RESET_FORCED;
    
    // Enable watchdog with short timeout to force reboot
    // SDK's watchdog_enable takes (delay_ms, pause_on_debug)
    watchdog_enable(delay_ms, 0);  // Don't pause on debug for reboot
    
    // Don't feed it - let it timeout and reboot
    while (1) {
        tight_loop_contents();
    }
}

/**
 * @brief Check if watchdog is enabled
 */
bool watchdog_is_enabled(void) {
    return wdt_state.enabled;
}

/**
 * @brief Get watchdog statistics
 */
void watchdog_get_stats(uint32_t* total_feeds, uint32_t* last_feed_time_ms,
                        uint32_t* timeout_ms) {
    if (total_feeds) {
        *total_feeds = wdt_state.feed_count;
    }
    
    if (last_feed_time_ms) {
        *last_feed_time_ms = wdt_state.last_feed_time_ms;
    }
    
    if (timeout_ms) {
        *timeout_ms = wdt_state.timeout_ms;
    }
}

/**
 * @brief Clear reset reason
 */
void watchdog_clear_reset_reason(void) {
    wdt_state.last_reset_reason = WATCHDOG_RESET_NONE;
}
