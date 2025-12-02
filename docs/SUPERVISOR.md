# Core 1 Supervisor System

**Automated System Health Monitoring & Protection**

## Overview

littleOS uses the RP2040's second core (Core 1) as a dedicated **supervisor** that continuously monitors system health, ensuring reliability and catching problems before they cause crashes.

### Why a Supervisor?

Traditional embedded systems can hang or crash silently. The supervisor prevents this by:

- **Watchdog Monitoring** - Ensures the watchdog is fed, system doesn't freeze
- **Temperature Monitoring** - Detects overheating before damage occurs
- **Memory Tracking** - Catches memory leaks and excessive usage
- **Crash Recovery** - Detects when Core 0 hangs and can trigger recovery
- **Performance Metrics** - Tracks system health over time

---

## Architecture

```
RP2040 Dual-Core System
├─ Core 0 (Main)
│  ├─ Shell / User Interface
│  ├─ SageLang Interpreter
│  ├─ Application Code
│  └─ Sends heartbeat every 500ms → Core 1
│
└─ Core 1 (Supervisor)
   ├─ Monitors Core 0 heartbeat
   ├─ Checks watchdog feeding
   ├─ Reads die temperature every 100ms
   ├─ Tracks memory usage
   └─ Triggers alerts/recovery as needed

Hardware FIFO: Core 0 ← → Core 1 (heartbeat communication)
```

---

## Quick Start

### Check System Health

```bash
> health

System Health: OK
Uptime: 45.123 seconds

Temperature: 42.3°C (peak: 45.1°C)
Memory: 12480 bytes (19.1%) - peak: 15360 bytes
Core 0: Responsive (heartbeat 127 ms ago)
```

### View Detailed Statistics

```bash
> stats

=== System Health Report ===
Status: OK
Uptime: 45123 ms

Memory:
  Used: 12480 bytes (19.1%)
  Peak: 15360 bytes
  Allocs: 453 / Frees: 412

Temperature:
  Current: 42.3°C
  Peak: 45.1°C

Watchdog:
  Feeds: 90
  Last feed: 127 ms ago

Core 0:
  Responsive: Yes
  Last heartbeat: 127 ms ago

Events:
  Warnings: 0
  Critical: 0
  Recoveries: 0
```

### Supervisor Control

```bash
# Check supervisor status
> supervisor status
Supervisor: Running
Health: OK

# Stop supervisor (not recommended during normal operation)
> supervisor stop
Supervisor stopped

# Start supervisor
> supervisor start
Supervisor started

# Control alerts
> supervisor alerts off  # Disable console alerts
> supervisor alerts on   # Enable console alerts
```

---

## Health Status Levels

### OK (Green)

All systems nominal:
- Watchdog being fed regularly
- Temperature normal (<70°C)
- Memory usage reasonable (<80%)
- Core 0 responding

### WARNING (Yellow)

Minor issues detected:
- Memory usage >80%
- Temperature >70°C
- Watchdog feed delayed (but not critical)

**Action:** Monitor the situation, investigate if persists

### CRITICAL (Orange)

Serious issues:
- Core 0 not responding (>5 seconds since heartbeat)
- Possible memory leak detected
- Temperature >80°C

**Action:** Immediate investigation required, may trigger automatic recovery

### EMERGENCY (Red)

System at risk of failure:
- Temperature critical (>85°C)
- Imminent crash detected

**Action:** Automatic protective measures (throttling, shutdown) may engage

---

## Monitored Metrics

### 1. Watchdog Feeding

**What:** Ensures the hardware watchdog is fed regularly

**Why:** If the watchdog isn't fed, system is likely hung

**Threshold:** Alert if >4 seconds between feeds (half of 8s timeout)

**Recovery:** Supervisor itself feeds watchdog, preventing false resets

### 2. Core 0 Heartbeat

**What:** Core 0 sends heartbeat every 500ms

**Why:** Detects if main core has hung (infinite loop, deadlock)

**Threshold:** Alert if >5 seconds since last heartbeat

**Recovery:** Can trigger soft reset or log crash for debugging

### 3. Temperature Monitoring

**What:** Reads RP2040 die temperature via ADC

**Why:** Overheating can cause instability or hardware damage

**Thresholds:**
- Warning: >70°C
- Critical: >80°C  
- Emergency: >85°C

**Note:** RP2040 operates up to 85°C, thermal shutdown at ~125°C

### 4. Memory Usage

**What:** Tracks heap allocations/frees

**Why:** Memory leaks cause slow degradation and eventual crash

**Thresholds:**
- Warning: >80% heap used
- Leak detection: Continuous growth without frees

**Total Heap:** ~64KB available on RP2040

### 5. Performance Counters

**What:** Uptime, operation counts, event tracking

**Why:** Helps identify patterns before failures occur

**Metrics:**
- Total uptime
- Watchdog feeds
- Allocation/free count
- Warning/critical event count

---

## Alert System

### Console Alerts

When alerts are enabled (default), supervisor prints warnings:

```
[SUPERVISOR] WARNING: Memory usage high: 85.2%
[SUPERVISOR] WARNING: Temperature high: 72.5°C
[SUPERVISOR] CRITICAL: Core 0 not responding! (last heartbeat 6237 ms ago)
```

### Alert Throttling

To prevent spam, warnings are throttled:
- Same warning only printed every 100 checks (~10 seconds)
- Critical alerts always printed immediately

### Disabling Alerts

```bash
> supervisor alerts off
```

Alerts disabled, but monitoring continues. Check status with `health` or `stats`.

---

## Integration Guide

### For Application Developers

Your code **automatically** benefits from supervisor monitoring. The shell sends heartbeats every 500ms, so as long as your code returns to the shell periodically, you're covered.

**Best Practice:** For long-running operations:

```c
#include "supervisor.h"

void long_running_task(void) {
    for (int i = 0; i < 1000000; i++) {
        // Do work...
        
        if (i % 1000 == 0) {
            supervisor_heartbeat();  // Send heartbeat every 1000 iterations
        }
    }
}
```

### Memory Tracking

If you allocate memory dynamically, report it to supervisor:

```c
#include "supervisor.h"

void* my_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        supervisor_report_memory(size);  // Report allocation
    }
    return ptr;
}

void my_free(void* ptr, size_t size) {
    free(ptr);
    supervisor_report_memory(-size);  // Report free (negative)
}
```

### Manual Health Checks

```c
#include "supervisor.h"

void check_system(void) {
    system_health_t health = supervisor_get_health();
    
    switch (health) {
        case HEALTH_OK:
            printf("System healthy\n");
            break;
        case HEALTH_WARNING:
            printf("System warning - check stats\n");
            break;
        case HEALTH_CRITICAL:
            printf("System critical!\n");
            // Maybe abort operation
            break;
        case HEALTH_EMERGENCY:
            printf("System emergency!\n");
            // Immediate protective action
            break;
    }
}
```

### Getting Metrics Programmatically

```c
#include "supervisor.h"

void display_metrics(void) {
    system_metrics_t metrics;
    
    if (supervisor_get_metrics(&metrics)) {
        printf("Uptime: %lu ms\n", metrics.uptime_ms);
        printf("Memory: %lu bytes (%.1f%%)\n", 
               metrics.heap_used_bytes, metrics.memory_usage_percent);
        printf("Temp: %.1f°C\n", metrics.temp_celsius);
        printf("Core 0: %s\n", 
               metrics.core0_responsive ? "OK" : "HUNG");
    }
}
```

---

## C API Reference

### Core Functions

#### `void supervisor_init(void)`
Start supervisor on Core 1. Called automatically during boot.

#### `void supervisor_stop(void)`
Stop supervisor. Not recommended during normal operation.

#### `bool supervisor_is_running(void)`
Check if supervisor is active.

**Returns:** `true` if running, `false` otherwise

---

### Health & Metrics

#### `system_health_t supervisor_get_health(void)`
Get current health status.

**Returns:** `HEALTH_OK`, `HEALTH_WARNING`, `HEALTH_CRITICAL`, or `HEALTH_EMERGENCY`

#### `bool supervisor_get_metrics(system_metrics_t* metrics)`
Get detailed system metrics.

**Parameters:**
- `metrics` - Pointer to structure to fill

**Returns:** `true` if successful

#### `int supervisor_get_stats_string(char* buffer, size_t size)`
Get formatted stats report.

**Returns:** Number of bytes written

---

### Heartbeat & Reporting

#### `void supervisor_heartbeat(void)`
Send heartbeat from Core 0. Called automatically by shell every 500ms.

**Usage:** Call this from long-running tasks to prevent hung detection.

#### `void supervisor_report_memory(int allocated)`
Report memory allocation/free.

**Parameters:**
- `allocated` - Bytes allocated (positive) or freed (negative)

**Example:**
```c
void* ptr = malloc(1024);
supervisor_report_memory(1024);  // Report allocation

free(ptr);
supervisor_report_memory(-1024); // Report free
```

---

### Configuration

#### `void supervisor_set_alerts(bool enable)`
Enable/disable console alerts.

**Parameters:**
- `enable` - `true` to enable, `false` to disable

#### `const char* supervisor_health_string(system_health_t status)`
Get human-readable health string.

**Returns:** "OK", "WARNING", "CRITICAL", or "EMERGENCY"

---

## Troubleshooting

### "Core 0 not responding" Alerts

**Cause:** Main core hasn't sent heartbeat in >5 seconds

**Solutions:**
1. Check if long-running operation is blocking shell
2. Add `supervisor_heartbeat()` calls to long loops
3. Investigate if system is actually hung

### High Memory Usage Warnings

**Cause:** Heap usage >80%

**Solutions:**
1. Check `stats` to see allocation/free counts
2. Look for memory leaks (allocations without matching frees)
3. Reduce memory usage or increase available heap

### Temperature Warnings

**Cause:** Die temperature >70°C

**Solutions:**
1. Improve ventilation/cooling
2. Reduce clock speed if possible
3. Check if ambient temperature is too high
4. Investigate if excessive processing is occurring

### Supervisor Won't Start

**Cause:** Core 1 initialization failed

**Solutions:**
1. Check if Core 1 is already in use
2. Verify `pico_multicore` library is linked
3. Check for SDK version compatibility

---

## Advanced Topics

### Supervisor Overhead

- **CPU Usage:** <1% (checks run every 100ms, take ~10μs)
- **Memory:** ~256 bytes for metrics structure
- **Latency:** Does not affect Core 0 timing

### Supervisor Loop Timing

```
Every 100ms:
  ├─ Check Core 0 heartbeat (5 second timeout)
  ├─ Check watchdog feed status (4 second timeout)
  ├─ Read temperature sensor (~50μs)
  ├─ Calculate memory usage
  ├─ Update health status
  └─ Feed watchdog

Every 10ms:
  └─ Quick sleep to prevent busy-waiting
```

### Customizing Thresholds

Edit `include/supervisor.h`:

```c
#define SUPERVISOR_CHECK_INTERVAL_MS    100   // Check frequency
#define SUPERVISOR_WATCHDOG_TIMEOUT_MS  8000  // Watchdog timeout
#define SUPERVISOR_MEMORY_WARN_PERCENT  80    // Memory warning
#define SUPERVISOR_TEMP_WARN_C          70    // Temp warning
#define SUPERVISOR_TEMP_CRITICAL_C      80    // Temp critical
```

Then rebuild: `make -j$(nproc)`

---

## Comparison: Supervisor vs Manual Monitoring

| Feature | Manual Monitoring | Supervisor |
|---------|------------------|-----------|
| **Watchdog** | Must call `wdt_feed()` everywhere | Automatic from Core 1 |
| **Temperature** | Manual ADC reads | Continuous monitoring |
| **Memory** | Manual tracking | Automatic leak detection |
| **Crash Detection** | No detection | Detects Core 0 hangs |
| **Overhead** | Scattered throughout code | Isolated on Core 1 |
| **Reliability** | Easy to forget | Always watching |

---

## Future Enhancements

Planned for future versions:

- [ ] **Automatic Recovery** - Soft reset when Core 0 hangs
- [ ] **Persistent Logs** - Store crash data to flash for post-mortem
- [ ] **Performance Profiling** - Track CPU usage per function
- [ ] **Network Monitoring** - Track network health (if WiFi added)
- [ ] **Configurable Actions** - User-defined responses to health events
- [ ] **Metrics Export** - Export data for external monitoring

---

## See Also

- [Watchdog Documentation](WATCHDOG.md) - Details on watchdog timer
- [Memory Management](MEMORY.md) - Heap allocation best practices
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) - Hardware specifications

---

**The supervisor is your safety net. It watches so you don't have to.** 👁️
