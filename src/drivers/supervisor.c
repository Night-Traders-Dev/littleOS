#include "supervisor.h"
#include "watchdog.h"
#include "dmesg.h"
#include "multicore.h"

#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/adc.h"
#include "hardware/structs/sio.h"
#endif

static volatile system_metrics_t metrics;
static volatile bool supervisor_running = false;
static volatile bool alerts_enabled = true;
static volatile bool single_core_mode = false;

static uint32_t last_heap_used = 0;
static uint32_t memory_stable_count = 0;

// Spinlock for safe cross-core metrics access
#ifdef PICO_BUILD
static spin_lock_t *metrics_lock = NULL;
#endif

#define TEMP_SENSOR_ADC_CHANNEL 4
// FIXED: Proper parentheses for operator precedence
#define TEMP_CONVERSION_FACTOR (3.3f / (1 << 12))
#define TEMP_SENSOR_VOLTAGE_AT_27C 0.706f
#define TEMP_SENSOR_SLOPE -0.001721f

// FIXED: Define heap size as constant
#define HEAP_SIZE_BYTES 65536

static void supervisor_copy_metrics(system_metrics_t *out) {
    if (!out) {
        return;
    }

#ifdef PICO_BUILD
    if (metrics_lock) {
        uint32_t save = spin_lock_blocking(metrics_lock);
        memcpy(out, (const void *)&metrics, sizeof(*out));
        spin_unlock(metrics_lock, save);
        return;
    }
#endif
    memcpy(out, (const void *)&metrics, sizeof(*out));
}

/**
 * @brief Read RP2040 die temperature
 */
static float read_temperature(void) {
#ifdef PICO_BUILD
    adc_select_input(TEMP_SENSOR_ADC_CHANNEL);
    uint16_t adc_value = adc_read();

    float voltage = adc_value * TEMP_CONVERSION_FACTOR;
    float temp = 27.0f - (voltage - TEMP_SENSOR_VOLTAGE_AT_27C) / TEMP_SENSOR_SLOPE;

    return temp;
#else
    return 25.0f;
#endif
}

/**
 * @brief Check system health and update flags
 */
static void check_system_health(void) {
    system_metrics_t snapshot;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t flags = 0;
    system_health_t health = HEALTH_OK;
    bool core0_responsive = true;
    uint32_t corrected_heartbeat = 0;
    float temp_celsius;
    float temp_peak;

    supervisor_copy_metrics(&snapshot);

    uint32_t time_since_feed = now - snapshot.last_feed_time_ms;

    /* Guard against wraparound or uninitialized timestamps: if the
     * elapsed time looks impossibly large (> 10 minutes), treat it
     * as a stale value and skip the warning. */
    if (time_since_feed > 600000) {
        time_since_feed = 0;
    }

    if (time_since_feed > (SUPERVISOR_WATCHDOG_TIMEOUT_MS / 2)) {
        flags |= HEALTH_FLAG_WATCHDOG;
        health = HEALTH_WARNING;
        if (alerts_enabled) {
            printf("[SUPERVISOR] WARNING: Watchdog not fed for %u ms\r\n",
                   (unsigned)time_since_feed);
        }
    }

    corrected_heartbeat = snapshot.core0_last_heartbeat;
    uint32_t time_since_heartbeat = now - snapshot.core0_last_heartbeat;

    /* Detect uint32_t wraparound: if elapsed time seems impossibly large
     * (> 10 minutes), it's likely a timer wrap, not an actual hang */
    if (time_since_heartbeat > 600000) {
        corrected_heartbeat = now;
        time_since_heartbeat = 0;
    }

    if (time_since_heartbeat > 5000) {
        flags |= HEALTH_FLAG_CORE0_HUNG;
        health = HEALTH_CRITICAL;
        core0_responsive = false;
        if (alerts_enabled) {
            printf("[SUPERVISOR] CRITICAL: Core 0 not responding! (last heartbeat %u ms ago)\r\n",
                   (unsigned)time_since_heartbeat);
        }
    }

    if (snapshot.memory_usage_percent > SUPERVISOR_MEMORY_WARN_PERCENT) {
        flags |= HEALTH_FLAG_MEMORY_HIGH;
        if (health < HEALTH_WARNING) health = HEALTH_WARNING;
        if (alerts_enabled && (snapshot.warning_count % 100 == 0)) {
            printf("[SUPERVISOR] WARNING: Memory usage high: %.1f%%\r\n",
                snapshot.memory_usage_percent);
        }
    }

    if (snapshot.heap_used_bytes > last_heap_used) {
        memory_stable_count = 0;
    } else if (snapshot.heap_used_bytes == last_heap_used) {
        memory_stable_count++;
    } else {
        memory_stable_count = 0;
    }

    if (memory_stable_count == 0 && snapshot.heap_used_bytes > (last_heap_used + 1024)) {
        if (snapshot.heap_used_bytes > 50000) {
            flags |= HEALTH_FLAG_MEMORY_LEAK;
            if (health < HEALTH_WARNING) health = HEALTH_WARNING;
        }
    }

    last_heap_used = snapshot.heap_used_bytes;

    temp_celsius = read_temperature();
    temp_peak = snapshot.temp_peak_celsius;
    if (temp_celsius > temp_peak) {
        temp_peak = temp_celsius;
    }

    if (temp_celsius > SUPERVISOR_TEMP_CRITICAL_C) {
        flags |= HEALTH_FLAG_TEMP_CRITICAL;
        health = HEALTH_EMERGENCY;
        if (alerts_enabled) {
            printf("[SUPERVISOR] EMERGENCY: Temperature critical! %.1f°C\r\n",
                temp_celsius);
        }
    } else if (temp_celsius > SUPERVISOR_TEMP_WARN_C) {
        flags |= HEALTH_FLAG_TEMP_HIGH;
        if (health < HEALTH_WARNING) health = HEALTH_WARNING;
        if (alerts_enabled && (snapshot.warning_count % 100 == 0)) {
            printf("[SUPERVISOR] WARNING: Temperature high: %.1f°C\r\n",
                temp_celsius);
        }
    }

    // Update health flags atomically with spinlock
#ifdef PICO_BUILD
    if (metrics_lock) {
        uint32_t save = spin_lock_blocking(metrics_lock);
        metrics.core0_last_heartbeat = corrected_heartbeat;
        metrics.core0_responsive = core0_responsive;
        metrics.temp_celsius = temp_celsius;
        metrics.temp_peak_celsius = temp_peak;
        metrics.health_flags = flags;
        metrics.health_status = health;
        if (health >= HEALTH_WARNING) metrics.warning_count++;
        if (health >= HEALTH_CRITICAL) metrics.critical_count++;
        spin_unlock(metrics_lock, save);
    } else
#endif
    {
        metrics.core0_last_heartbeat = corrected_heartbeat;
        metrics.core0_responsive = core0_responsive;
        metrics.temp_celsius = temp_celsius;
        metrics.temp_peak_celsius = temp_peak;
        metrics.health_flags = flags;
        metrics.health_status = health;
        if (health >= HEALTH_WARNING) metrics.warning_count++;
        if (health >= HEALTH_CRITICAL) metrics.critical_count++;
    }
}

/**
 * @brief Main supervisor loop (runs on Core 1)
 */
static void supervisor_loop(void) {
#ifdef PICO_BUILD
    printf("[Core 1 Supervisor] Starting...\r\n");

    adc_init();
    adc_set_temp_sensor_enabled(true);

    supervisor_running = true;

    /* Metrics are already initialized by supervisor_init() on Core 0.
     * Do NOT memset here — that would race with Core 0 heartbeats and
     * zero out last_feed_time_ms, causing a bogus watchdog warning. */
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t last_check_time = now;

    printf("[Core 1 Supervisor] Monitoring system health...\r\n");

    while (supervisor_running) {
        now = to_ms_since_boot(get_absolute_time());
#ifdef PICO_BUILD
        if (metrics_lock) {
            uint32_t save = spin_lock_blocking(metrics_lock);
            metrics.uptime_ms = now;
            spin_unlock(metrics_lock, save);
        } else
#endif
        {
            metrics.uptime_ms = now;
        }

        if (now - last_check_time >= SUPERVISOR_CHECK_INTERVAL_MS) {
            check_system_health();
            last_check_time = now;
        }

        // NOTE: Do NOT feed the hardware watchdog here. Only Core 0
        // should feed it so that a Core 0 hang actually triggers a
        // hardware reset. Core 1 monitors the heartbeat instead.
        sleep_ms(10);
    }

    printf("[Core 1 Supervisor] Stopped\r\n");
#endif
}

/**
 * @brief Single-core fallback: run health checks cooperatively from Core 0.
 *
 * Called from supervisor_heartbeat() when multicore launch failed.
 */
static void supervisor_poll_health(void) {
#ifdef PICO_BUILD
    static uint32_t last_poll_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!metrics_lock) return;

    /* Update uptime */
    uint32_t save = spin_lock_blocking(metrics_lock);
    metrics.uptime_ms = now;
    spin_unlock(metrics_lock, save);

    /* Run health check at the normal supervisor interval */
    if (now - last_poll_ms >= SUPERVISOR_CHECK_INTERVAL_MS) {
        check_system_health();
        last_poll_ms = now;
    }

    // watchdog fed by supervisor_heartbeat() caller — not here
#endif
}

void supervisor_init(void) {
#ifdef PICO_BUILD
    if (supervisor_running) {
        printf("Supervisor already running\r\n");
        return;
    }

    if (multicore_is_running()) {
        printf("Cannot start Supervisor: Core 1 is currently running a script.\r\n");
        return;
    }

    // Claim a hardware spinlock for cross-core metrics access
    if (!metrics_lock) {
        int lock_num = spin_lock_claim_unused(true);
        metrics_lock = spin_lock_init(lock_num);
    }

    // Initialize metrics for single-core fallback path
    uint32_t now = to_ms_since_boot(get_absolute_time());
    memset((void *)&metrics, 0, sizeof(metrics));
    metrics.core0_responsive = true;
    metrics.health_status = HEALTH_OK;
    metrics.uptime_ms = now;
    metrics.core0_last_heartbeat = now;
    metrics.last_feed_time_ms = now;

    // Probe Core 1 FIFO: drain any stale data, then check if Core 1
    // can accept FIFO writes.  On emulators that only run a single core
    // (e.g. Bramble in default mode) the FIFO drain after reset never
    // sees the expected response, so multicore_launch_core1 would hang.
    // We detect this by checking if the FIFO is ready after reset.
    bool core1_available = true;

    // Reset Core 1 — on real hardware this puts it in the boot ROM
    // waiting-for-launch state and pushes a 0 onto our FIFO.
    multicore_reset_core1();
    sleep_ms(10);

    // After reset, the boot ROM on Core 1 pushes 0 onto Core 0's FIFO.
    // If we can read it, Core 1 is alive and ready for the launch seq.
    if (!(sio_hw->fifo_st & SIO_FIFO_ST_VLD_BITS)) {
        core1_available = false;
    } else {
        (void)sio_hw->fifo_rd;  // drain the sentinel
    }

    if (core1_available) {
        multicore_launch_core1(supervisor_loop);
        sleep_ms(100);
    }

    if (supervisor_running) {
        single_core_mode = false;
        printf("Supervisor: Launched on Core 1\r\n");
    } else {
        // Core 1 failed to start — fall back to cooperative polling
        single_core_mode = true;
        supervisor_running = true;

        // Enable ADC for temperature reading on Core 0
        adc_init();
        adc_set_temp_sensor_enabled(true);

        printf("Supervisor: Single-core mode (cooperative polling)\r\n");
    }
#endif
}

void supervisor_stop(void) {
#ifdef PICO_BUILD
    if (!supervisor_running) {
        return;
    }

    supervisor_running = false;

    if (!single_core_mode) {
        sleep_ms(200);
        multicore_reset_core1();
    }

    printf("Supervisor: Stopped\r\n");
#endif
}

bool supervisor_is_running(void) {
    return supervisor_running;
}

bool supervisor_pause_for_flash(void) {
#ifdef PICO_BUILD
    if (!supervisor_running) {
        return false;
    }

    supervisor_running = false;

    if (!single_core_mode) {
        sleep_ms(200);
        multicore_reset_core1();
    }

    return true;
#else
    return false;
#endif
}

void supervisor_resume_after_flash(bool paused) {
#ifdef PICO_BUILD
    if (paused) {
        supervisor_init();
    }
#else
    (void)paused;
#endif
}

bool supervisor_get_metrics(system_metrics_t* out_metrics) {
    if (!out_metrics) {
        return false;
    }
    supervisor_copy_metrics(out_metrics);
    return true;
}

system_health_t supervisor_get_health(void) {
    system_metrics_t snapshot;
    supervisor_copy_metrics(&snapshot);
    return snapshot.health_status;
}

void supervisor_heartbeat(void) {
#ifdef PICO_BUILD
    static uint32_t last_hw_feed_ms = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Always update the heartbeat/feed timestamps so the supervisor
    // knows Core 0 is alive. This is cheap (one spinlock + two stores).
    if (metrics_lock) {
        uint32_t save = spin_lock_blocking(metrics_lock);
        metrics.core0_last_heartbeat = now;
        metrics.last_feed_time_ms = now;
        spin_unlock(metrics_lock, save);
    } else {
        metrics.core0_last_heartbeat = now;
        metrics.last_feed_time_ms = now;
    }

    // Feed the hardware watchdog at a lower rate (every 2s is plenty
    // for the 8s timeout).
    if (now - last_hw_feed_ms >= 2000) {
        last_hw_feed_ms = now;
        if (metrics_lock) {
            uint32_t save = spin_lock_blocking(metrics_lock);
            metrics.watchdog_feeds++;
            spin_unlock(metrics_lock, save);
        } else {
            metrics.watchdog_feeds++;
        }
        wdt_feed();
    }

    // In single-core mode, also run the health checks cooperatively
    if (single_core_mode) {
        supervisor_poll_health();
    }
#endif
}

void supervisor_report_memory(int allocated) {
#ifdef PICO_BUILD
    if (metrics_lock) {
        uint32_t save = spin_lock_blocking(metrics_lock);
        if (allocated > 0) {
            metrics.heap_used_bytes += (uint32_t)allocated;
            metrics.heap_allocations++;
            if (metrics.heap_used_bytes > metrics.heap_peak_bytes) {
                metrics.heap_peak_bytes = metrics.heap_used_bytes;
            }
        } else if (allocated < 0) {
            uint32_t freed = (uint32_t)(-allocated);
            if (metrics.heap_used_bytes >= freed) {
                metrics.heap_used_bytes -= freed;
            } else {
                metrics.heap_used_bytes = 0;
            }
            metrics.heap_frees++;
        }

        if (metrics.heap_used_bytes <= HEAP_SIZE_BYTES) {
            metrics.heap_free_bytes = HEAP_SIZE_BYTES - metrics.heap_used_bytes;
        } else {
            metrics.heap_free_bytes = 0;
        }
        metrics.memory_usage_percent = (float)(metrics.heap_used_bytes * 100) / HEAP_SIZE_BYTES;
        spin_unlock(metrics_lock, save);
        return;
    }
#endif

    if (allocated > 0) {
        metrics.heap_used_bytes += (uint32_t)allocated;
        metrics.heap_allocations++;
        if (metrics.heap_used_bytes > metrics.heap_peak_bytes) {
            metrics.heap_peak_bytes = metrics.heap_used_bytes;
        }
    } else if (allocated < 0) {
        uint32_t freed = (uint32_t)(-allocated);
        // Guard against unsigned underflow
        if (metrics.heap_used_bytes >= freed) {
            metrics.heap_used_bytes -= freed;
        } else {
            metrics.heap_used_bytes = 0;
        }
        metrics.heap_frees++;
    }

    // Safe calculation of free bytes
    if (metrics.heap_used_bytes <= HEAP_SIZE_BYTES) {
        metrics.heap_free_bytes = HEAP_SIZE_BYTES - metrics.heap_used_bytes;
    } else {
        metrics.heap_free_bytes = 0;
    }
    metrics.memory_usage_percent = (float)(metrics.heap_used_bytes * 100) / HEAP_SIZE_BYTES;
}

const char* supervisor_health_string(system_health_t status) {
    switch (status) {
        case HEALTH_OK:       return "OK";
        case HEALTH_WARNING:  return "WARNING";
        case HEALTH_CRITICAL: return "CRITICAL";
        case HEALTH_EMERGENCY: return "EMERGENCY";
        default:              return "UNKNOWN";
    }
}

void supervisor_set_alerts(bool enable) {
    alerts_enabled = enable;
    printf("Supervisor alerts: %s\r\n", enable ? "ENABLED" : "DISABLED");
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
        " Used: %lu bytes (%.1f%%)\r\n"
        " Peak: %lu bytes\r\n"
        " Allocs: %lu / Frees: %lu\r\n"
        "\r\n"
        "Temperature:\r\n"
        " Current: %.1f°C\r\n"
        " Peak: %.1f°C\r\n"
        "\r\n"
        "Watchdog:\r\n"
        " Feeds: %lu\r\n"
        " Last feed: %lu ms ago\r\n"
        "\r\n"
        "Core 0:\r\n"
        " Responsive: %s\r\n"
        " Last heartbeat: %lu ms ago\r\n"
        "\r\n"
        "Events:\r\n"
        " Warnings: %lu\r\n"
        " Critical: %lu\r\n"
        " Recoveries: %lu\r\n",
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
