#include "supervisor.h"
#include "watchdog.h"
#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/adc.h"
#endif

// Global metrics (shared between cores via hardware)
static volatile system_metrics_t metrics;
static volatile bool supervisor_running = false;
static volatile bool alerts_enabled = true;

// Memory tracking
static uint32_t last_heap_used = 0;
static uint32_t memory_stable_count = 0;

// Temperature sensor constants
#define TEMP_SENSOR_ADC_CHANNEL 4
#define TEMP_CONVERSION_FACTOR 3.3f / (1 << 12)
#define TEMP_SENSOR_VOLTAGE_AT_27C 0.706f
#define TEMP_SENSOR_SLOPE -0.001721f

/**
 * @brief Read RP2040 die temperature
 * @return Temperature in Celsius
 */
static float read_temperature(void) {
#ifdef PICO_BUILD
    // Select ADC channel 4 (temperature sensor)
    adc_select_input(TEMP_SENSOR_ADC_CHANNEL);
    
    // Read ADC (12-bit)
    uint16_t adc_value = adc_read();
    
    // Convert to voltage
    float voltage = adc_value * TEMP_CONVERSION_FACTOR;
    
    // Convert voltage to temperature
    // T = 27 - (V - 0.706) / 0.001721
    float temp = 27.0f - (voltage - TEMP_SENSOR_VOLTAGE_AT_27C) / TEMP_SENSOR_SLOPE;
    
    return temp;
#else
    return 25.0f; // Simulated temp for non-Pico builds
#endif
}

/**
 * @brief Check system health and update flags
 */
static void check_system_health(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t flags = 0;
    system_health_t health = HEALTH_OK;
    
    // Check watchdog feeding
    uint32_t time_since_feed = now - metrics.last_feed_time_ms;
    if (time_since_feed > (SUPERVISOR_WATCHDOG_TIMEOUT_MS / 2)) {
        flags |= HEALTH_FLAG_WATCHDOG;
        health = HEALTH_WARNING;
        if (alerts_enabled) {
            printf("[SUPERVISOR] WARNING: Watchdog not fed for %lu ms\r\n", time_since_feed);
        }
    }
    
    // Check Core 0 heartbeat with overflow protection
    uint32_t time_since_heartbeat = now - metrics.core0_last_heartbeat;
    
    // Detect overflow/underflow - if value is unreasonably large, resync
    if (time_since_heartbeat > 1000000000) {  // > ~11 days is clearly overflow
        printf("[SUPERVISOR] Heartbeat timing overflow detected, resynchronizing\r\n");
        metrics.core0_last_heartbeat = now;
        time_since_heartbeat = 0;
    }
    
    if (time_since_heartbeat > 5000) {  // 5 second timeout
        flags |= HEALTH_FLAG_CORE0_HUNG;
        health = HEALTH_CRITICAL;
        metrics.core0_responsive = false;
        if (alerts_enabled) {
            printf("[SUPERVISOR] CRITICAL: Core 0 not responding! (last heartbeat %lu ms ago)\r\n", 
                   time_since_heartbeat);
        }
    } else {
        metrics.core0_responsive = true;
    }
    
    // Check memory usage
    if (metrics.memory_usage_percent > SUPERVISOR_MEMORY_WARN_PERCENT) {
        flags |= HEALTH_FLAG_MEMORY_HIGH;
        if (health < HEALTH_WARNING) health = HEALTH_WARNING;
        if (alerts_enabled && (metrics.warning_count % 100 == 0)) {  // Don't spam
            printf("[SUPERVISOR] WARNING: Memory usage high: %.1f%%\r\n", 
                   metrics.memory_usage_percent);
        }
    }
    
    // Check for memory leaks (heap growing without freeing)
    if (metrics.heap_used_bytes > last_heap_used) {
        memory_stable_count = 0;
    } else if (metrics.heap_used_bytes == last_heap_used) {
        memory_stable_count++;
    } else {
        memory_stable_count = 0;
    }
    
    // If heap keeps growing for 50 consecutive checks, possible leak
    if (memory_stable_count == 0 && metrics.heap_used_bytes > (last_heap_used + 1024)) {
        if (metrics.heap_used_bytes > 50000) {  // Only flag if significant usage
            flags |= HEALTH_FLAG_MEMORY_LEAK;
            if (health < HEALTH_WARNING) health = HEALTH_WARNING;
        }
    }
    last_heap_used = metrics.heap_used_bytes;
    
    // Check temperature
    metrics.temp_celsius = read_temperature();
    if (metrics.temp_celsius > metrics.temp_peak_celsius) {
        metrics.temp_peak_celsius = metrics.temp_celsius;
    }
    
    if (metrics.temp_celsius > SUPERVISOR_TEMP_CRITICAL_C) {
        flags |= HEALTH_FLAG_TEMP_CRITICAL;
        health = HEALTH_EMERGENCY;
        if (alerts_enabled) {
            printf("[SUPERVISOR] EMERGENCY: Temperature critical! %.1f°C\r\n", 
                   metrics.temp_celsius);
        }
    } else if (metrics.temp_celsius > SUPERVISOR_TEMP_WARN_C) {
        flags |= HEALTH_FLAG_TEMP_HIGH;
        if (health < HEALTH_WARNING) health = HEALTH_WARNING;
        if (alerts_enabled && (metrics.warning_count % 100 == 0)) {
            printf("[SUPERVISOR] WARNING: Temperature high: %.1f°C\r\n", 
                   metrics.temp_celsius);
        }
    }
    
    // Update metrics
    metrics.health_flags = flags;
    metrics.health_status = health;
    
    if (health >= HEALTH_WARNING) {
        metrics.warning_count++;
    }
    if (health >= HEALTH_CRITICAL) {
        metrics.critical_count++;
    }
}

/**
 * @brief Main supervisor loop (runs on Core 1)
 */
static void supervisor_loop(void) {
#ifdef PICO_BUILD
    printf("[Core 1 Supervisor] Starting...\r\n");
    
    // Initialize ADC for temperature sensing
    adc_init();
    adc_set_temp_sensor_enabled(true);
    
    supervisor_running = true;
    
    // Initialize metrics with current time to avoid false alarms
    uint32_t now = to_ms_since_boot(get_absolute_time());
    memset((void*)&metrics, 0, sizeof(metrics));
    metrics.core0_responsive = true;
    metrics.health_status = HEALTH_OK;
    metrics.uptime_ms = now;
    metrics.core0_last_heartbeat = now;  // Initialize to now
    metrics.last_feed_time_ms = now;     // Initialize to now
    
    uint32_t last_check_time = now;
    
    printf("[Core 1 Supervisor] Monitoring system health...\r\n");
    
    while (supervisor_running) {
        now = to_ms_since_boot(get_absolute_time());
        metrics.uptime_ms = now;
        
        // Perform health checks at regular intervals
        if (now - last_check_time >= SUPERVISOR_CHECK_INTERVAL_MS) {
            check_system_health();
            last_check_time = now;
        }
        
        // Feed the watchdog from supervisor side
        // This ensures watchdog is fed even if Core 0 hangs
        wdt_feed();
        
        // Small delay to prevent busy-waiting
        sleep_ms(10);
    }
    
    printf("[Core 1 Supervisor] Stopped\r\n");
#endif
}

void supervisor_init(void) {
#ifdef PICO_BUILD
    if (supervisor_running) {
        printf("Supervisor already running\r\n");
        return;
    }
    
    // Initialize heartbeat timestamp BEFORE launching Core 1
    uint32_t now = to_ms_since_boot(get_absolute_time());
    metrics.core0_last_heartbeat = now;
    metrics.last_feed_time_ms = now;
    
    // Reset Core 1 if it was previously running
    multicore_reset_core1();
    
    // Launch supervisor on Core 1
    multicore_launch_core1(supervisor_loop);
    
    // Wait a moment for Core 1 to start
    sleep_ms(100);
    
    // Send immediate heartbeat to establish baseline
    supervisor_heartbeat();
    
    printf("Supervisor: Launched on Core 1\r\n");
#endif
}

void supervisor_stop(void) {
#ifdef PICO_BUILD
    if (!supervisor_running) {
        return;
    }
    
    supervisor_running = false;
    sleep_ms(200);  // Give supervisor time to exit cleanly
    multicore_reset_core1();
    
    printf("Supervisor: Stopped\r\n");
#endif
}

bool supervisor_is_running(void) {
    return supervisor_running;
}

bool supervisor_get_metrics(system_metrics_t* out_metrics) {
    if (!out_metrics) {
        return false;
    }
    
    // Copy metrics atomically (since it's volatile)
    memcpy(out_metrics, (void*)&metrics, sizeof(system_metrics_t));
    return true;
}

system_health_t supervisor_get_health(void) {
    return metrics.health_status;
}

void supervisor_heartbeat(void) {
#ifdef PICO_BUILD
    uint32_t now = to_ms_since_boot(get_absolute_time());
    metrics.core0_last_heartbeat = now;
    metrics.last_feed_time_ms = now;  // Also update watchdog feed time
    metrics.watchdog_feeds++;
#endif
}

void supervisor_report_memory(int allocated) {
    if (allocated > 0) {
        metrics.heap_used_bytes += allocated;
        metrics.heap_allocations++;
        if (metrics.heap_used_bytes > metrics.heap_peak_bytes) {
            metrics.heap_peak_bytes = metrics.heap_used_bytes;
        }
    } else {
        metrics.heap_used_bytes -= (-allocated);
        metrics.heap_frees++;
    }
    
    // Calculate usage percentage (assuming 64KB total heap for RP2040)
    metrics.heap_free_bytes = 65536 - metrics.heap_used_bytes;
    metrics.memory_usage_percent = (float)metrics.heap_used_bytes / 655.36f;
}

const char* supervisor_health_string(system_health_t status) {
    switch (status) {
        case HEALTH_OK:        return "OK";
        case HEALTH_WARNING:   return "WARNING";
        case HEALTH_CRITICAL:  return "CRITICAL";
        case HEALTH_EMERGENCY: return "EMERGENCY";
        default:               return "UNKNOWN";
    }
}

void supervisor_set_alerts(bool enable) {
    alerts_enabled = enable;
}

int supervisor_get_stats_string(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }
    
    system_metrics_t m;
    supervisor_get_metrics(&m);
    
    return snprintf(buffer, size,
        "=== System Health Report ===\r\n"
        "Status: %s\r\n"
        "Uptime: %lu ms\r\n"
        "\r\n"
        "Memory:\r\n"
        "  Used: %lu bytes (%.1f%%)\r\n"
        "  Peak: %lu bytes\r\n"
        "  Allocs: %lu / Frees: %lu\r\n"
        "\r\n"
        "Temperature:\r\n"
        "  Current: %.1f°C\r\n"
        "  Peak: %.1f°C\r\n"
        "\r\n"
        "Watchdog:\r\n"
        "  Feeds: %lu\r\n"
        "  Last feed: %lu ms ago\r\n"
        "\r\n"
        "Core 0:\r\n"
        "  Responsive: %s\r\n"
        "  Last heartbeat: %lu ms ago\r\n"
        "\r\n"
        "Events:\r\n"
        "  Warnings: %lu\r\n"
        "  Critical: %lu\r\n"
        "  Recoveries: %lu\r\n",
        supervisor_health_string(m.health_status),
        m.uptime_ms,
        m.heap_used_bytes, m.memory_usage_percent,
        m.heap_peak_bytes,
        m.heap_allocations, m.heap_frees,
        m.temp_celsius,
        m.temp_peak_celsius,
        m.watchdog_feeds,
        m.uptime_ms - m.last_feed_time_ms,
        m.core0_responsive ? "Yes" : "No",
        m.uptime_ms - m.core0_last_heartbeat,
        m.warning_count,
        m.critical_count,
        m.recovery_count
    );
}
