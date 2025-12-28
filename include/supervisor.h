#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @file supervisor.h
 * @brief Core 1 Supervisor - OS Health Monitoring & Security
 *
 * The supervisor runs on Core 1 and monitors:
 * - Watchdog feeding (ensures Core 0 doesn't hang)
 * - Memory usage and leaks
 * - System temperature
 * - Stack overflow detection
 * - Crash recovery
 * - Performance metrics
 */

// Supervisor configuration
#define SUPERVISOR_CHECK_INTERVAL_MS    100   // How often to check system health
#define SUPERVISOR_WATCHDOG_TIMEOUT_MS  8000  // Watchdog timeout (8 seconds)
#define SUPERVISOR_MEMORY_WARN_PERCENT  80    // Warn when memory usage exceeds this
#define SUPERVISOR_TEMP_WARN_C          70    // Warn when temp exceeds this (Celsius)
#define SUPERVISOR_TEMP_CRITICAL_C      80    // Critical temp threshold

// System health status
typedef enum {
    HEALTH_OK        = 0,  // All systems nominal
    HEALTH_WARNING   = 1,  // Warning conditions detected
    HEALTH_CRITICAL  = 2,  // Critical conditions detected
    HEALTH_EMERGENCY = 3   // Emergency - system may crash
} system_health_t;

// Health check flags (bitfield)
typedef enum {
    HEALTH_FLAG_NONE           = 0,
    HEALTH_FLAG_WATCHDOG       = (1 << 0),  // Watchdog not being fed
    HEALTH_FLAG_MEMORY_HIGH    = (1 << 1),  // Memory usage high
    HEALTH_FLAG_MEMORY_LEAK    = (1 << 2),  // Possible memory leak detected
    HEALTH_FLAG_TEMP_HIGH      = (1 << 3),  // Temperature high
    HEALTH_FLAG_TEMP_CRITICAL  = (1 << 4),  // Temperature critical
    HEALTH_FLAG_STACK_OVERFLOW = (1 << 5),  // Stack overflow detected
    HEALTH_FLAG_CORE0_HUNG     = (1 << 6),  // Core 0 appears hung
    HEALTH_FLAG_FIFO_OVERFLOW  = (1 << 7),  // FIFO communication overflow
} health_flag_t;

// System metrics
typedef struct {
    // Watchdog
    uint32_t watchdog_feeds;        // Total watchdog feeds
    uint32_t last_feed_time_ms;     // Last time watchdog was fed
    bool     watchdog_healthy;      // Is watchdog being fed regularly?

    // Memory
    uint32_t heap_used_bytes;       // Current heap usage
    uint32_t heap_free_bytes;       // Free heap
    uint32_t heap_peak_bytes;       // Peak heap usage
    uint32_t heap_allocations;      // Total allocations
    uint32_t heap_frees;            // Total frees
    float    memory_usage_percent;  // Memory usage percentage

    // Temperature
    float    temp_celsius;          // Current die temperature
    float    temp_peak_celsius;     // Peak temperature

    // Performance
    uint32_t core0_cycles;          // Core 0 CPU cycles
    uint32_t core1_cycles;          // Core 1 CPU cycles
    uint32_t uptime_ms;             // System uptime

    // Health
    system_health_t health_status;  // Overall health status
    uint32_t health_flags;          // Active health flags
    uint32_t warning_count;         // Total warnings issued
    uint32_t critical_count;        // Total critical events
    uint32_t recovery_count;        // Times system recovered from hang

    // Core 0 monitoring
    uint32_t core0_last_heartbeat;  // Last heartbeat from Core 0
    bool     core0_responsive;      // Is Core 0 responding?
} system_metrics_t;

/**
 * @brief Initialize supervisor system on Core 1
 *
 * This launches Core 1 with the supervisor loop.
 * The supervisor will continuously monitor system health.
 */
void supervisor_init(void);

/**
 * @brief Stop supervisor (for shutdown)
 */
void supervisor_stop(void);

/**
 * @brief Check if supervisor is running
 * @return true if supervisor is active
 */
bool supervisor_is_running(void);

/**
 * @brief Get current system metrics
 * @param metrics Pointer to metrics structure to fill
 * @return true if metrics retrieved successfully
 */
bool supervisor_get_metrics(system_metrics_t* metrics);

/**
 * @brief Get current system health status
 * @return Health status code
 */
system_health_t supervisor_get_health(void);

/**
 * @brief Send heartbeat from Core 0 (call periodically from main loop)
 *
 * Core 0 should call this regularly to indicate it's still responsive.
 * If heartbeats stop, supervisor may trigger recovery actions.
 */
void supervisor_heartbeat(void);

/**
 * @brief Report memory allocation/free events to supervisor
 * @param allocated Bytes allocated (positive) or freed (negative)
 */
void supervisor_report_memory(int allocated);

/**
 * @brief Get human-readable health status string
 * @param status Health status code
 * @return String description
 */
const char* supervisor_health_string(system_health_t status);

/**
 * @brief Enable/disable supervisor console alerts
 * @param enable true to enable alerts, false to disable
 */
void supervisor_set_alerts(bool enable);

/**
 * @brief Get supervisor statistics as formatted string
 * @param buffer Buffer to write to
 * @param size   Buffer size
 * @return Number of bytes written
 */
int supervisor_get_stats_string(char* buffer, size_t size);

#endif // SUPERVISOR_H
