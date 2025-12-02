# Kernel Message Buffer (dmesg)

## Overview

littleOS includes a kernel message buffer system (`dmesg`) that captures system events, errors, and diagnostic messages throughout the boot process and runtime operation. Similar to Linux's `dmesg`, this circular buffer allows you to review system activity even after the event has occurred.

## Features

- **Circular Ring Buffer**: Stores last 128 messages with timestamps
- **8 Log Levels**: From emergency to debug (Linux kernel compatible)
- **Boot Sequence Tracking**: Captures all initialization milestones
- **Runtime Logging**: Continuous system event monitoring
- **Filtering**: View messages by severity level
- **Persistent Until Reboot**: Messages survive system recovery events

## Log Levels

Messages are categorized by severity (most to least critical):

| Level | Name | Description | Use Case |
|-------|------|-------------|----------|
| 0 | EMERG | System is unusable | Complete system failure |
| 1 | ALERT | Action required immediately | Hardware malfunction |
| 2 | CRIT | Critical conditions | Watchdog recovery, core hang |
| 3 | ERR | Error conditions | Failed operations, invalid state |
| 4 | WARN | Warning conditions | High temperature, memory usage |
| 5 | NOTICE | Normal but significant | Configuration changes |
| 6 | INFO | Informational | Boot milestones, subsystem init |
| 7 | DEBUG | Debug-level messages | Detailed troubleshooting |

## Shell Command Usage

### View All Messages

```bash
> dmesg
```

Displays complete message buffer with timestamps:

```
========== littleOS Kernel Message Buffer ==========
Total messages: 15 | Uptime: 45320ms
=====================================================
[    0ms] <INFO> littleOS dmesg initialized
[    1ms] <INFO> Boot sequence started
[   12ms] <INFO> RP2040 littleOS kernel starting
[   24ms] <INFO> Watchdog timer initialized (8s timeout)
[   38ms] <INFO> Configuration storage initialized
[   52ms] <INFO> SageLang interpreter initialized
[   68ms] <INFO> Script storage system initialized
[ 3142ms] <INFO> Watchdog enabled - monitoring for system hangs
[ 3256ms] <INFO> Supervisor launched on Core 1
[ 3268ms] <INFO> Boot sequence complete - entering shell
=====================================================
```

### Filter by Log Level

**Show only errors and above:**
```bash
> dmesg --level err
> dmesg -l err
```

**Show warnings and above:**
```bash
> dmesg -l warn
```

**Available levels**: `emerg`, `alert`, `crit`, `err`, `warn`, `notice`, `info`, `debug`

### Clear Buffer

```bash
> dmesg --clear
> dmesg -c
```

Clears all messages from buffer (new message logged confirming clear).

### Help

```bash
> dmesg --help
> dmesg -h
```

## Programming API

### Initialization

```c
#include "dmesg.h"

// Call early in boot sequence
void kernel_main(void) {
    dmesg_init();
    // ... rest of kernel init
}
```

### Logging Messages

**Direct API:**
```c
dmesg_log(DMESG_LEVEL_INFO, "Initializing subsystem X");
dmesg_log(DMESG_LEVEL_ERR, "Failed to open file: %s", filename);
```

**Convenience Macros** (recommended):
```c
dmesg_emerg("System unusable: %s", reason);
dmesg_alert("Immediate action required!");
dmesg_crit("Critical failure in %s", module);
dmesg_err("Error: %s failed with code %d", func, code);
dmesg_warn("Warning: temperature %.1f°C", temp);
dmesg_notice("Configuration changed: %s", param);
dmesg_info("Service started: %s", service);
dmesg_debug("Debug: var=%d, ptr=%p", var, ptr);
```

### Reading Buffer

```c
// Get message count
uint32_t count = dmesg_get_count();

// Get system uptime
uint32_t uptime_ms = dmesg_get_uptime();

// Print all messages
dmesg_print_all();

// Print filtered messages (errors and above)
dmesg_print_level(DMESG_LEVEL_ERR);

// Clear buffer
dmesg_clear();
```

## Integration Points

littleOS automatically logs these events:

### Boot Sequence
- dmesg subsystem initialization
- Watchdog timer setup
- Crash recovery detection
- Configuration storage init
- SageLang interpreter loading
- Script storage initialization
- Autoboot script execution
- Supervisor launch

### Runtime Events
- System reboots (user-initiated)
- Watchdog recovery events
- Critical errors from subsystems
- Configuration changes
- Supervisor health alerts

## Typical Boot Message Sequence

```
[    0ms] <INFO> littleOS dmesg initialized
[    1ms] <INFO> Boot sequence started
[   12ms] <INFO> RP2040 littleOS kernel starting
[   24ms] <INFO> Watchdog timer initialized (8s timeout)
[   38ms] <INFO> Configuration storage initialized
[   52ms] <INFO> SageLang interpreter initialized
[   68ms] <INFO> Script storage system initialized
[ 3142ms] <INFO> Watchdog enabled - monitoring for system hangs
[ 3256ms] <INFO> Supervisor launched on Core 1
[ 3268ms] <INFO> Boot sequence complete - entering shell
```

## After Crash Recovery

```
[    0ms] <INFO> littleOS dmesg initialized
[    1ms] <INFO> Boot sequence started
[   12ms] <INFO> RP2040 littleOS kernel starting
[   24ms] <INFO> Watchdog timer initialized (8s timeout)
[   26ms] <CRIT> System recovered from watchdog reset
[   38ms] <INFO> Configuration storage initialized
...
```

## Troubleshooting Use Cases

### Check Boot Problems

If system appears to hang during boot, after recovery:
```bash
> dmesg
```
Look for last logged message before hang.

### Investigate Crashes

After watchdog recovery:
```bash
> dmesg -l crit
```
Shows critical events including recovery reason.

### Monitor Warnings

Check for temperature or memory warnings:
```bash
> dmesg -l warn
```

### Debug Autoboot Issues

If autoboot script fails:
```bash
> dmesg
```
Look for messages around autoboot execution.

## Technical Details

### Buffer Architecture
- **Type**: Circular ring buffer
- **Size**: 128 entries
- **Entry Size**: ~124 bytes (120 char message + metadata)
- **Total Memory**: ~16 KB
- **Behavior**: Oldest messages overwritten when full

### Timestamp Resolution
- **Unit**: Milliseconds since boot
- **Source**: Pico SDK `time_us_32()` converted to ms
- **Accuracy**: ±1ms
- **Range**: 0 to ~49.7 days (uint32_t overflow)

### Thread Safety
- **Core 0 Only**: dmesg functions called from main kernel/shell
- **Core 1**: Supervisor does NOT log to dmesg (avoid contention)
- **No Locking**: Single-core access model

## Best Practices

1. **Log Important Events**: Initialization, errors, state changes
2. **Use Appropriate Levels**: Don't mark everything as EMERG
3. **Keep Messages Concise**: 120 character limit
4. **Include Context**: Module name, error codes, relevant values
5. **Check After Crashes**: First troubleshooting step

## Examples

### Subsystem Initialization
```c
void my_subsystem_init(void) {
    dmesg_info("My subsystem initializing");
    
    if (!init_hardware()) {
        dmesg_err("My subsystem: hardware init failed");
        return;
    }
    
    dmesg_info("My subsystem ready");
}
```

### Error Handling
```c
int load_config(const char* path) {
    if (!path) {
        dmesg_err("load_config: NULL path");
        return -1;
    }
    
    FILE* f = fopen(path, "r");
    if (!f) {
        dmesg_warn("load_config: file not found: %s", path);
        return -2;
    }
    
    dmesg_info("Config loaded: %s", path);
    return 0;
}
```

### Critical Events
```c
void handle_critical_error(const char* reason) {
    dmesg_crit("CRITICAL: %s", reason);
    // Initiate safe shutdown or recovery
}
```

## See Also

- [SUPERVISOR.md](SUPERVISOR.md) - System health monitoring
- [WATCHDOG.md](WATCHDOG.md) - Automatic crash recovery
- [BOOT_SEQUENCE.md](BOOT_SEQUENCE.md) - Boot process documentation
