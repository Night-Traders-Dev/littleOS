// src/sage_watchdog.c
// SageLang Native Function Bindings for Watchdog Timer
#include "sage_embed.h"
#include "watchdog.h"
#include <stdio.h>

// Include SageLang headers
#include "value.h"
#include "env.h"

/**
 * @brief SageLang native function: watchdog_enable(timeout_ms)
 * Enable watchdog with timeout in milliseconds
 * 
 * Usage in SageLang:
 *   watchdog_enable(5000);  // 5 second timeout
 */
static Value sage_watchdog_enable(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "watchdog_enable() requires 1 argument: timeout_ms\r\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_NUMBER) {
        fprintf(stderr, "watchdog_enable() argument must be a number\r\n");
        return val_bool(false);
    }
    
    uint32_t timeout_ms = (uint32_t)args[0].as.number;
    
    return val_bool(wdt_enable(timeout_ms));
}

/**
 * @brief SageLang native function: watchdog_feed()
 * Feed the watchdog to prevent reset
 * 
 * Usage in SageLang:
 *   watchdog_feed();  // Reset watchdog timer
 */
static Value sage_watchdog_feed(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    wdt_feed();
    return val_nil();
}

/**
 * @brief SageLang native function: watchdog_disable()
 * Disable the watchdog
 * 
 * Usage in SageLang:
 *   watchdog_disable();
 */
static Value sage_watchdog_disable(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    wdt_disable();
    return val_nil();
}

/**
 * @brief SageLang native function: watchdog_reboot(delay_ms)
 * Force system reboot via watchdog
 * 
 * Usage in SageLang:
 *   watchdog_reboot(100);  // Reboot in 100ms
 */
static Value sage_watchdog_reboot(int argc, Value* args) {
    uint32_t delay_ms = 0;
    
    if (argc > 0 && args[0].type == VAL_NUMBER) {
        delay_ms = (uint32_t)args[0].as.number;
    }
    
    wdt_reboot(delay_ms);
    
    // Should never reach here
    return val_nil();
}

/**
 * @brief SageLang native function: watchdog_is_enabled()
 * Check if watchdog is enabled
 * 
 * Usage in SageLang:
 *   if (watchdog_is_enabled()) {
 *       print("Watchdog active");
 *   }
 */
static Value sage_watchdog_is_enabled(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    return val_bool(wdt_is_enabled());
}

/**
 * @brief SageLang native function: watchdog_time_remaining()
 * Get time remaining until watchdog timeout
 * 
 * Usage in SageLang:
 *   let remaining = watchdog_time_remaining();
 *   print("Time remaining: " + remaining + "ms");
 */
static Value sage_watchdog_time_remaining(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    uint32_t remaining = wdt_get_time_remaining_ms();
    return val_number(remaining);
}

/**
 * @brief SageLang native function: watchdog_was_reset()
 * Check if system was reset by watchdog
 * 
 * Usage in SageLang:
 *   if (watchdog_was_reset()) {
 *       print("System recovered from crash!");
 *   }
 */
static Value sage_watchdog_was_reset(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    watchdog_reset_reason_t reason = wdt_get_reset_reason();
    return val_bool(reason == WATCHDOG_RESET_TIMEOUT);
}

/**
 * @brief SageLang native function: watchdog_get_feeds()
 * Get total number of watchdog feeds
 * 
 * Usage in SageLang:
 *   let feeds = watchdog_get_feeds();
 */
static Value sage_watchdog_get_feeds(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    uint32_t feeds = 0;
    wdt_get_stats(&feeds, NULL, NULL);
    
    return val_number(feeds);
}

/**
 * @brief Register all watchdog native functions with SageLang environment
 */
void sage_register_watchdog_functions(Env* env) {
    if (!env) return;
    
    // Register watchdog functions
    env_define(env, "watchdog_enable", 15, val_native(sage_watchdog_enable));
    env_define(env, "watchdog_feed", 13, val_native(sage_watchdog_feed));
    env_define(env, "watchdog_disable", 16, val_native(sage_watchdog_disable));
    env_define(env, "watchdog_reboot", 15, val_native(sage_watchdog_reboot));
    env_define(env, "watchdog_is_enabled", 19, val_native(sage_watchdog_is_enabled));
    env_define(env, "watchdog_time_remaining", 23, val_native(sage_watchdog_time_remaining));
    env_define(env, "watchdog_was_reset", 18, val_native(sage_watchdog_was_reset));
    env_define(env, "watchdog_get_feeds", 18, val_native(sage_watchdog_get_feeds));
    
    printf("Watchdog: Registered 8 native functions\r\n");
}
