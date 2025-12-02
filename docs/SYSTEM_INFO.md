# System Information Commands

## Overview

The System Information module provides access to hardware details, runtime statistics, and system metrics for the RP2040 microcontroller running littleOS.

## Architecture

```
┌────────────────────────────────────────┐
│      SageLang Scripts                  │
│  (sys_* functions)                     │
└──────────────┬─────────────────────────┘
               │
┌──────────────▼─────────────────────────┐
│   src/sage_system.c                    │
│   (SageLang Native Bindings)           │
└──────────────┬─────────────────────────┘
               │
┌──────────────▼─────────────────────────┐
│   src/system_info.c                    │
│   (System Information Implementation)  │
└──────────────┬─────────────────────────┘
               │
┌──────────────▼─────────────────────────┐
│   Pico SDK (hardware APIs)             │
│   (clocks, ADC, unique_id, etc.)       │
└────────────────────────────────────────┘
```

---

## Available Functions

### SageLang System Functions

All functions are available in SageLang scripts when littleOS is running.

#### `sys_version()`
Get littleOS version string.

**Returns:** `string` - Version number (e.g., "0.2.0")

**Example:**
```sagelang
let version = sys_version();
print("Running littleOS " + version);
```

---

#### `sys_uptime()`
Get system uptime in seconds since boot.

**Returns:** `number` - Uptime in seconds

**Example:**
```sagelang
let uptime = sys_uptime();
let minutes = uptime / 60;
print("System uptime: " + str(minutes) + " minutes");
```

---

#### `sys_temp()`
Get RP2040 internal temperature sensor reading.

**Returns:** `number` - Temperature in degrees Celsius

**Example:**
```sagelang
let temp = sys_temp();
if (temp > 50) {
    print("WARNING: High temperature!");
}
print("CPU Temperature: " + str(temp) + "°C");
```

---

#### `sys_clock()`
Get current CPU clock speed.

**Returns:** `number` - Clock speed in MHz

**Example:**
```sagelang
let mhz = sys_clock();
print("CPU running at " + str(mhz) + " MHz");
```

---

#### `sys_free_ram()`
Get available free RAM.

**Returns:** `number` - Free RAM in kilobytes

**Example:**
```sagelang
let free = sys_free_ram();
if (free < 10) {
    print("Low memory warning!");
}
print("Free RAM: " + str(free) + " KB");
```

---

#### `sys_total_ram()`
Get total RAM available on the system.

**Returns:** `number` - Total RAM in kilobytes

**Example:**
```sagelang
let total = sys_total_ram();
let free = sys_free_ram();
let used = total - free;
let percent = (used / total) * 100;
print("Memory usage: " + str(percent) + "%");
```

---

#### `sys_board_id()`
Get unique board identifier (hardware serial number).

**Returns:** `string` - 16-character hexadecimal ID

**Example:**
```sagelang
let id = sys_board_id();
print("Board ID: " + id);
```

---

#### `sys_info()`
Get comprehensive system information as a dictionary.

**Returns:** `dict` - Dictionary with system information

**Dictionary Keys:**
- `version` (string) - OS version
- `build_date` (string) - Build date and time
- `cpu_model` (string) - CPU model name
- `cpu_mhz` (number) - CPU clock speed in MHz
- `cpu_cores` (number) - Number of CPU cores
- `cpu_revision` (number) - Chip revision number
- `total_ram_kb` (number) - Total RAM in KB
- `free_ram_kb` (number) - Free RAM in KB
- `used_ram_kb` (number) - Used RAM in KB
- `flash_mb` (number) - Flash size in MB
- `uptime_seconds` (number) - Uptime in seconds
- `uptime_minutes` (number) - Uptime in minutes
- `uptime_hours` (number) - Uptime in hours
- `uptime_days` (number) - Uptime in days
- `temperature` (number) - Temperature in °C
- `board_id` (string) - Unique board ID

**Example:**
```sagelang
let info = sys_info();
print("OS: " + info["version"]);
print("CPU: " + info["cpu_model"] + " @ " + str(info["cpu_mhz"]) + " MHz");
print("RAM: " + str(info["used_ram_kb"]) + " / " + str(info["total_ram_kb"]) + " KB");
print("Temp: " + str(info["temperature"]) + "°C");
print("Uptime: " + str(info["uptime_hours"]) + " hours");
```

---

#### `sys_print()`
Print formatted system information report to console.

**Returns:** `nil`

**Example:**
```sagelang
sys_print();
```

**Output:**
```
=================================
    littleOS System Information
=================================

OS Version:    0.2.0
Build Date:    Dec  2 2025 12:45:30

--- CPU Information ---
Model:         RP2040
Revision:      1
Cores:         2
Clock Speed:   125 MHz

--- Memory Information ---
Total RAM:     264 KB
Used RAM:      128 KB
Free RAM:      136 KB
Flash Size:    2 MB

--- System Uptime ---
Uptime:        2 hours, 34 min, 12 sec

--- Sensors ---
Temperature:   28.5°C

--- Hardware ---
Board ID:      E6614103E7452534

=================================
```

---

## Complete Examples

### Example 1: System Monitor

```sagelang
// Simple system monitor
while (true) {
    let temp = sys_temp();
    let free = sys_free_ram();
    let uptime = sys_uptime();
    
    print("\r\n=== System Status ===");
    print("Uptime: " + str(uptime) + "s");
    print("Temperature: " + str(temp) + "°C");
    print("Free RAM: " + str(free) + " KB");
    
    // Check for warnings
    if (temp > 50) {
        print("⚠️  HIGH TEMPERATURE WARNING!");
    }
    
    if (free < 20) {
        print("⚠️  LOW MEMORY WARNING!");
    }
    
    // Wait 5 seconds
    sleep(5000);
}
```

### Example 2: Memory Usage Report

```sagelang
let total = sys_total_ram();
let free = sys_free_ram();
let used = total - free;
let percent = (used / total) * 100;

print("Memory Usage Report:");
print("-------------------");
print("Total:  " + str(total) + " KB");
print("Used:   " + str(used) + " KB");
print("Free:   " + str(free) + " KB");
print("Usage:  " + str(percent) + "%");

// Visual bar
let bars = percent / 5;  // 20 bars = 100%
let bar_str = "[";
let i = 0;
while (i < 20) {
    if (i < bars) {
        bar_str = bar_str + "█";
    } else {
        bar_str = bar_str + "░";
    }
    i = i + 1;
}
bar_str = bar_str + "]";
print(bar_str);
```

### Example 3: Temperature Logger

```sagelang
// Log temperature every minute
let log = [];
let max_samples = 60;  // Keep last hour

while (true) {
    let temp = sys_temp();
    let uptime = sys_uptime();
    
    // Create log entry
    let entry = {};
    entry["time"] = uptime;
    entry["temp"] = temp;
    
    // Add to log
    push(log, entry);
    
    // Keep only recent samples
    if (len(log) > max_samples) {
        // Remove oldest entry
        log = slice(log, 1, len(log));
    }
    
    // Calculate statistics
    let sum = 0;
    let min_temp = 999;
    let max_temp = -999;
    
    let i = 0;
    while (i < len(log)) {
        let t = log[i]["temp"];
        sum = sum + t;
        if (t < min_temp) min_temp = t;
        if (t > max_temp) max_temp = t;
        i = i + 1;
    }
    
    let avg = sum / len(log);
    
    print("\r\nTemperature Stats:");
    print("Current: " + str(temp) + "°C");
    print("Average: " + str(avg) + "°C");
    print("Min:     " + str(min_temp) + "°C");
    print("Max:     " + str(max_temp) + "°C");
    print("Samples: " + str(len(log)));
    
    // Wait 60 seconds
    sleep(60000);
}
```

### Example 4: System Info Dashboard

```sagelang
// Display complete system info
let info = sys_info();

print("\r\n╔════════════════════════════════════╗");
print("║   littleOS System Dashboard        ║");
print("╠════════════════════════════════════╣");
print("║ Version:   " + info["version"] + "                       ║");
print("║ Board ID:  " + info["board_id"] + "     ║");
print("╠════════════════════════════════════╣");

let uptime_hrs = info["uptime_hours"];
let uptime_min = info["uptime_minutes"] % 60;
print("║ Uptime:    " + str(uptime_hrs) + "h " + str(uptime_min) + "m" + "               ║");

let cpu_str = info["cpu_model"] + " @ " + str(info["cpu_mhz"]) + "MHz";
print("║ CPU:       " + cpu_str + "          ║");

let temp_str = str(info["temperature"]) + "°C";
print("║ Temp:      " + temp_str + "                       ║");

let ram_used = info["used_ram_kb"];
let ram_total = info["total_ram_kb"];
print("║ RAM:       " + str(ram_used) + "/" + str(ram_total) + " KB" + "          ║");

print("╚════════════════════════════════════╝\r\n");
```

---

## C API Reference

For C code integration:

### Header
```c
#include "system_info.h"
```

### Functions

```c
bool system_get_cpu_info(cpu_info_t* info);
bool system_get_memory_info(memory_info_t* info);
bool system_get_uptime(uptime_info_t* info);
float system_get_temperature(void);
bool system_get_board_id(char* buffer, size_t buffer_size);
const char* system_get_version(void);
const char* system_get_build_date(void);
void system_print_info(void);
```

### Example C Code

```c
#include "system_info.h"
#include <stdio.h>

void check_system_health(void) {
    float temp = system_get_temperature();
    
    if (temp > 50.0f) {
        printf("WARNING: High temperature: %.1f°C\r\n", temp);
    }
    
    memory_info_t mem;
    if (system_get_memory_info(&mem)) {
        if (mem.free_ram < 20480) {  // Less than 20KB
            printf("WARNING: Low memory: %u KB free\r\n", 
                   mem.free_ram / 1024);
        }
    }
}
```

---

## Technical Details

### Temperature Sensor

- **Sensor:** RP2040 internal temperature diode on ADC channel 4
- **Formula:** `T = 27 - (ADC_voltage - 0.706) / 0.001721`
- **Accuracy:** ±5°C typical
- **Range:** -40°C to +85°C
- **Use case:** Thermal monitoring, not precision measurements

### Memory Estimation

The `sys_free_ram()` function provides an **estimate** of free RAM:
- Uses stack pointer position vs heap end
- Does not account for fragmentation
- Suitable for monitoring trends, not exact measurements

### Board ID

- **Source:** RP2040 unique 64-bit chip ID
- **Format:** 16-character hexadecimal string
- **Uniqueness:** Guaranteed unique per chip
- **Use cases:** Device identification, licensing, fleet management

### Performance

- **sys_version()**: O(1), ~1μs
- **sys_uptime()**: O(1), ~2μs
- **sys_temp()**: ~100μs (ADC conversion)
- **sys_clock()**: O(1), ~3μs
- **sys_free_ram()**: O(1), ~5μs
- **sys_board_id()**: ~50μs
- **sys_info()**: ~200μs (combines all calls)
- **sys_print()**: ~1ms (formatted output)

---

## Testing

### Quick Test in SageLang REPL

```bash
sage> sys_print();
# Should display full system information

sage> let temp = sys_temp();
sage> print(temp);
# Should show current temperature

sage> let info = sys_info();
sage> print(info["version"]);
# Should show "0.2.0"
```

---

## Troubleshooting

### Temperature reads 0 or invalid
- ADC may not be initialized
- Try calling `sys_temp()` a second time
- Check if another process is using ADC

### Free RAM seems incorrect
- This is an estimate based on stack position
- Actual free RAM may vary due to fragmentation
- Use for trends, not absolute values

### Board ID all zeros
- Rare hardware issue
- Try power cycle
- Flash may be corrupted

---

## Future Enhancements

- **Voltage monitoring** - Read VSYS and VBUS voltages
- **Flash filesystem stats** - Used/free flash space
- **Process information** - Task list and CPU usage
- **Network stats** - If WiFi/Ethernet added
- **Historical tracking** - Store metrics over time

---

## References

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) - Section 4.9.5 (Temperature Sensor)
- [Pico SDK Documentation](https://www.raspberrypi.com/documentation/pico-sdk/)
- littleOS version: 0.2.0
