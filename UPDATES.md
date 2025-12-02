# littleOS Recent Updates

## December 2, 2025 - Critical Fixes and dmesg Integration

### 1. Supervisor Heartbeat Timing Fix (CRITICAL)

**Problem Identified:**
The supervisor system was experiencing uint32_t overflow in heartbeat timing calculations, causing incorrect "last heartbeat" values (4,294,967,xxx ms ≈ 49 days) even though the system had only been running for minutes.

**Root Cause:**
Unsigned integer arithmetic underflow occurred when `metrics.core0_last_heartbeat` was larger than the current timestamp during initialization, causing wraparound to maximum uint32_t values.

**Fix Applied:**
- Added overflow detection in `check_system_health()` function
- Detects when heartbeat delta exceeds 1 billion milliseconds (~11 days)
- Automatically resynchronizes timestamps when overflow detected
- Added immediate heartbeat call after supervisor initialization

**Code Changes:**
```c
// New overflow protection in supervisor.c
uint32_t time_since_heartbeat = now - metrics.core0_last_heartbeat;

if (time_since_heartbeat > 1000000000) {  // > ~11 days is clearly overflow
    printf("[SUPERVISOR] Heartbeat timing overflow detected, resynchronizing\r\n");
    metrics.core0_last_heartbeat = now;
    time_since_heartbeat = 0;
}
```

**Impact:**
- ✅ Accurate core responsiveness detection
- ✅ Proper hang detection (5 second threshold)
- ✅ Reliable system health monitoring
- ✅ No more false overflow values

**Files Modified:**
- `src/supervisor.c` - Added overflow detection and immediate heartbeat

---

### 2. Kernel Message Buffer (dmesg) Integration

**New Feature:**
Implemented Linux-style kernel message buffer system with circular ring buffer architecture for tracking system events, boot sequence, and runtime diagnostics.

**Components Added:**

#### Core Implementation
- **`include/dmesg.h`** - Header file with API definitions and convenience macros
- **`src/dmesg.c`** - Circular buffer implementation with 128-entry capacity
- **`src/shell/cmd_dmesg.c`** - Shell command for viewing/filtering messages
- **`docs/DMESG.md`** - Complete documentation

#### Features
- **8 Log Levels**: EMERG, ALERT, CRIT, ERR, WARN, NOTICE, INFO, DEBUG
- **Circular Buffer**: 128 messages with automatic oldest-first overwrite
- **Timestamps**: Millisecond-resolution since boot
- **Filtering**: View by severity level
- **Shell Integration**: `dmesg` command with full option support

#### Shell Command Usage
```bash
# View all messages
> dmesg

# Filter by level
> dmesg --level err     # Errors and above
> dmesg -l warn         # Warnings and above

# Clear buffer
> dmesg --clear

# Help
> dmesg --help
```

#### Programming API
```c
// Initialization (called in kernel_main)
dmesg_init();

// Logging (convenience macros)
dmesg_info("System initialized");
dmesg_warn("Temperature high: %.1f°C", temp);
dmesg_err("Failed to load: %s", file);
dmesg_crit("Critical failure in %s", module);

// Direct API
dmesg_log(DMESG_LEVEL_INFO, "Message: %s", msg);

// Query
uint32_t count = dmesg_get_count();
uint32_t uptime = dmesg_get_uptime();
```

**Integration Points:**

1. **Boot Sequence** (`src/kernel.c`):
   - First subsystem initialized (before watchdog)
   - Logs all major boot milestones
   - Captures crash recovery events
   - Tracks autoboot script execution

2. **Shell** (`src/shell/shell.c`):
   - Added `dmesg` to help menu
   - Integrated command handler
   - Logs system reboots

3. **Build System** (`CMakeLists.txt`):
   - Added `src/dmesg.c` to core library
   - Added `src/shell/cmd_dmesg.c` to shell commands
   - Updated build summary

**Boot Message Example:**
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

**Files Created:**
- `include/dmesg.h` - API header (proper header, replaced duplicate C code)
- `src/shell/cmd_dmesg.c` - Shell command implementation
- `docs/DMESG.md` - Complete documentation

**Files Modified:**
- `src/kernel.c` - Initialize dmesg, add boot logging
- `src/shell/shell.c` - Integrate dmesg command
- `CMakeLists.txt` - Add dmesg to build

---

### 3. Documentation Additions

**New Documentation:**
- **`docs/DMESG.md`** - Comprehensive dmesg guide
  - Feature overview
  - Log level reference table
  - Shell command usage examples
  - Programming API documentation
  - Troubleshooting use cases
  - Best practices

**Documentation Includes:**
- Complete API reference
- Integration examples
- Buffer architecture details
- Thread safety notes
- Typical boot sequence
- Crash recovery message patterns

---

## Summary of Changes

### Files Modified
1. `src/supervisor.c` - Fixed heartbeat timing overflow
2. `src/kernel.c` - Integrated dmesg initialization
3. `src/shell/shell.c` - Added dmesg command
4. `CMakeLists.txt` - Added dmesg to build system

### Files Created
5. `include/dmesg.h` - dmesg API header
6. `src/shell/cmd_dmesg.c` - dmesg shell command
7. `docs/DMESG.md` - dmesg documentation
8. `UPDATES.md` - This file

### Commits Made
1. `0db3f40` - Fix supervisor heartbeat timing overflow issue
2. `2cfda8c` - Create proper dmesg.h header file
3. `9027fd8` - Integrate dmesg into kernel boot sequence
4. `9683532` - Add dmesg shell command
5. `068f42c` - Integrate dmesg command into shell
6. `f392f8e` - Add dmesg to build system
7. `efa2a83` - Add dmesg documentation
8. (this commit) - Document recent system improvements

---

## Testing Recommendations

### Supervisor Testing
1. Monitor `stats` command - heartbeat should show <1000ms values
2. Verify no overflow messages in console
3. Check Core 0 responsiveness remains "Yes"
4. Test across extended runtime (hours)

### dmesg Testing
1. Boot system and run `dmesg` - verify boot sequence logged
2. Test filtering: `dmesg -l err`, `dmesg -l warn`
3. Test clear: `dmesg -c`, then `dmesg` (should show clear message only)
4. Trigger watchdog recovery, check for critical message
5. Run `dmesg` after autoboot execution

### Integration Testing
1. Full boot cycle - verify no errors
2. Reboot command - check for log message
3. Crash recovery - verify both supervisor and dmesg capture event
4. Long-running session - ensure dmesg buffer wraps correctly

---

## Future Enhancements

### Potential Improvements
1. **Memory Tracking Integration**
   - Wire `supervisor_report_memory()` into allocator
   - Show actual heap usage in stats

2. **dmesg Persistence**
   - Save critical messages to flash
   - Survive across reboots
   - Implement crash log

3. **Enhanced Filtering**
   - Time range filtering
   - Keyword search
   - Module/subsystem filtering

4. **Supervisor Enhancements**
   - Bidirectional heartbeat (Core 1 responsiveness check)
   - Automatic recovery actions for CRITICAL states
   - Configurable alert thresholds

---

## Version Impact

These changes prepare the ground for **littleOS v0.3.1** or **v0.4.0** depending on release strategy:

- **v0.3.1** (Patch): If treating supervisor fix as critical bugfix
- **v0.4.0** (Minor): If emphasizing dmesg as new feature

Recommendation: **v0.4.0** - dmesg is a significant new diagnostic capability.

---

## Credits

Fixes and features developed on December 2, 2025

- Supervisor timing issue identified through system metrics analysis
- dmesg implementation inspired by Linux kernel message buffer
- Integration follows littleOS architectural patterns
