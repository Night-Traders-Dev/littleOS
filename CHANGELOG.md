# Changelog

All notable changes to littleOS. Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.8.1] - 2026-05-08

### Fixed
- Fixed buffer over-read in `grep` command (`cmd_text.c`) to prevent memory leaks or faults.
- Resolved race conditions in the Scheduler and IPC subystems using hardware spinlocks (`spin_lock_blocking`).
- Fixed unsafe `task_terminate` mechanism to avoid memcpy shifts that caused crashes.
- Fixed segmented memory manager (bump allocator replaced with first-fit block allocator in `.bss` instead of linker symbol overlaps). Added `kernel_free` and `interpreter_free`.
- Prevented Core 1 conflicts between the Multicore driver script runner and the system Supervisor (`supervisor_init` vs `multicore_launch_script`).
- Fixed bare-metal RISC-V build failures by stubbing out desktop-only `sagelang` modules (AOT, JIT, SGPU) and providing `dirent.h` fallbacks in `sage_compat.h`.

## [0.8.0] - 2026-03-20

### Added - F2FS Inline Data and Append

- **Inline data** for small files (<= 384 bytes): data stored directly in the inode body, zero block allocation overhead. Transparent to the user — files auto-promote to block-based when they grow past 384 bytes.
- **`fs append <path> <text>`** command for appending to existing files
- **`fs stat`** now shows inode version, flags (inline/extents/compressed/dedup), and storage mode
- New inode v2 fields: `inode_flags`, `inline_data[384]`, `extent_count`, `content_hash`, `comp_size`
- Backward-compatible: v1 inodes (inode_flags=0) continue to work with block-based I/O

### Added - FAT12/FAT16 Filesystem (Flash-Backed)

- **`fat` shell command** - Full FAT12 and FAT16 filesystem alongside the existing F2FS-style FS
- **Subcommands**: `fat init <12|16> [sectors]`, `fat mount`, `fat unmount`, `fat erase`, `fat info`, `fat ls`, `fat cat`, `fat write`, `fat mkdir`, `fat rm`, `fat touch`
- **Flash-backed** storage — persists across power cycles and reboots, zero RAM overhead (only the in-memory FAT cluster table lives in RAM)
- Split flash partition layout: F2FS gets 512KB, FAT gets 448KB (RP2040) or 2.5MB (RP2350 4MB+ flash)
- Standard BPB, 8.3 filenames, dual FAT copies, subdirectories with `.` and `..`
- FAT12 for small volumes (< 4085 clusters), FAT16 for larger volumes
- In-memory FAT table for O(1) cluster lookup
- Both F2FS and FAT can be mounted simultaneously on separate flash partitions
- Read-only subcommands (`ls`, `cat`, `info`) require `PERM_READ`; write operations require `PERM_WRITE`

### Added - TAP Network Bridge Support

- **`net tap <ip> <gw> [mask]`** command - Bring up TAP interface with static IP (default /24 netmask)
- **`net tap dhcp`** command - Bring up TAP interface with DHCP
- **`net status` works on TAP** - Detects when CYW43 link reports DOWN but lwIP netif has an IP (TAP bridge mode)
- Shows `Mode: TAP bridge` vs `Mode: WiFi` in status output
- RSSI display hidden on TAP (no radio hardware)
- CYW43 lazy-init on `net status` (was only on `net connect`/`net scan`)
- **Security**: `net tap` requires `CAP_NET_ADMIN` (same as `net connect`); remote shell logs non-local connections with `dmesg_warn` security alert
- TAP status shows `Security: TAP bridge (host-side firewall applies)` reminder
- API: `net_tap_up(ip, gw, netmask)` and `net_tap_dhcp()` in `net.h`

### Added - Command Timeout System

- **`timeout` shell command** - View or set command execution timeout (`timeout <ms>`, 0 to disable)
- **Hardware timer alarm** fires after 30 seconds (default) and sets `shell_cmd_abort` flag
- All long-running commands (`top`, `gpiowatch`, `adc stream`, `neopixel animate`, `pwmtune sweep`, `pinout watch`) check the flag and exit gracefully
- SageLang eval loop checks `shell_cmd_abort` after each statement, returns `SAGE_ERROR_TIMEOUT`
- Timeout is cooperative (flag-based); the hardware watchdog (8s) remains the last resort for truly hung commands
- `shell_cmd_abort` exported in `shell.h` for custom commands to check

### Added - Multi-Policy Scheduler

- **3 scheduling policies** switchable at runtime via `tasks policy <name>`:
  - `priority` - Fixed-priority: highest-priority task always runs (default)
  - `round-robin` - Equal 20ms time slices, rotates through all ready tasks
  - `cfs` - Completely Fair Scheduler: weighted virtual runtime ensures fairness while respecting priority
- **`tasks policy`** shell command to view/switch scheduler policy
- **CFS virtual runtime** (`vruntime` field) tracks weighted execution time per task
- Policy changes take effect immediately and adjust all task time slices

### Added - SageLang Bytecode VM and Tooling

- **Bytecode VM** - Stack-based virtual machine for fast execution of simple statements
  - `bytecode.c` - AST-to-bytecode compiler for loops, assignments, arithmetic, function calls
  - `runtime.c` - Dual execution engine: bytecode VM with automatic AST interpreter fallback
  - `vm.c` already present; now wired into the eval pipeline via `sage_execute_stmt()`
  - Mode: `SAGE_RUNTIME_AUTO` (bytecode when possible, AST for classes/generators/exceptions)
- **Constant folding** (`constfold.c`) - Compile-time evaluation of constant expressions
  - Number arithmetic: `2 + 3` -> `5`
  - String concatenation: `"a" + "b"` -> `"ab"`
  - Boolean logic: `true and false` -> `false`
- **Linter** (`linter.c`) - Static analysis via `sage --lint "code"`
  - 13 rules: unused variables, naming conventions, style, complexity, error patterns
  - Severity levels: error, warning, style
- **`sage -m`** now works as a single argument (previously required `sage -m dummy`)
- **`sage --lint CODE`** - New command to lint SageLang code from the shell

### Changed - Performance Improvements

- **Command hash table** - Shell command lookup is now O(1) average via djb2 hash (was O(n) linear scan of 64 entries)
- **Scheduler tick optimization** - Cached `running_task_ptr` eliminates O(n) `find_task()` call in 1ms SysTick handler
- **GPIO debug disabled** - `GPIO_DEBUG` set to 0 in production; eliminates UART printf on every GPIO operation
- **SageLang heartbeat reduction** - Removed per-statement `sage_force_heartbeat()`; now uses time-based 250ms interval only, reducing overhead during script execution
- **CYW43 deferred init** - WiFi hardware initialization deferred until first `net connect`, `net scan`, or `net status`. Boot no longer loads 230KB CYW43 firmware or starts PIO/DMA polling until WiFi is actually needed.

### Changed - Flash Partition Layout

- F2FS partition reduced from 960KB to 512KB (`0x100000-0x17FFFF`)
- New FAT partition: `0x180000-0x1EFFFF` (448KB on RP2040, 2.5MB on RP2350 4MB+ flash)
- Config storage unchanged at `0x1F0000-0x1FFFFF`

### Changed - SageLang Submodule

- Updated to commit `ad82849` (SageLang v0.13.0)
- Build now includes 16 source files (was 12): added `bytecode.c`, `runtime.c`, `linter.c`, `constfold.c`
- Execution pipeline: lex -> parse -> constant fold -> bytecode/AST execute

### Fixed - Watchdog Integration

- **`wdt_init()` never called at boot** - Boot reason (`watchdog_caused_reboot()`) was never checked; system never knew if it recovered from a watchdog reset. Now `wdt_init(8000)` is called before `wdt_enable()` in kernel init.
- **Core 1 supervisor defeated the watchdog** - Supervisor loop called `wdt_feed()` every 10ms unconditionally, meaning the hardware watchdog would never fire even if Core 0 was completely dead. Removed `wdt_feed()` from Core 1; only Core 0 feeds via `supervisor_heartbeat()` now.
- **Shell bypassed supervisor feed tracking** - Shell loop called `wdt_feed()` independently of `supervisor_heartbeat()`, creating two uncoordinated feed paths. Removed the direct `wdt_feed()` call; single feed path: shell -> `supervisor_heartbeat()` -> `wdt_feed()` (every 2s).

### Fixed - Scheduler Bugs

- **Priority scheduler didn't round-robin among equal-priority tasks** - `select_priority()` always picked the first highest-priority match; now uses two-pass: find max priority, then rotate among tasks at that level
- **CFS new-task starvation** - New tasks started with `vruntime = 0`, starving all existing tasks until the new task caught up. New tasks under CFS now start at `min(vruntime)` of all ready tasks.
- **CFS vruntime not reset on policy switch** - Switching to CFS mid-run left stale vruntimes; `scheduler_set_policy(CFS)` now resets all vruntimes to 0

### Fixed - Shell Security Audit

- **Authorization default-deny** - `shell_authorize_command()` now returns `false` for unrecognized commands instead of `true` (was a security bypass for any new command not in the auth table)
- **`fat` command missing from authorization** - Would hit default-deny. Added with read/write subcommand split matching `fs` (read-only: `ls`, `cat`, `info`; write: all others)
- **`timeout` command missing from authorization** - Added to unconditionally-allowed built-in list (alongside `help`, `version`, `clear`, etc.)
- **Buffer overflow in `cmd_script.c`** - `strcpy()` replaced with bounded `memcpy()` in script save code path; loop now breaks when buffer space is exhausted
- **Tab completion overflow** - Added `MAX_CMD_LEN` bounds checks before `memcpy()` in single-match and partial-completion paths

---

## [0.7.0] - 2026-03-13

### Added - RP2350 Multi-Board Support

- **8 board targets** via `LITTLEOS_BOARD` CMake variable:
  - `pico` (RP2040, ARM Cortex-M0+)
  - `pico_w` (RP2040 + WiFi)
  - `pico2` (RP2350, ARM Cortex-M33)
  - `pico2_riscv` (RP2350, RISC-V Hazard3)
  - `pico2_w` (RP2350 + WiFi, ARM)
  - `pico2_w_riscv` (RP2350 + WiFi, RISC-V)
  - `adafruit_feather_rp2350` (ARM Cortex-M33)
  - `adafruit_feather_rp2350_riscv` (RISC-V Hazard3)
- **Board config header** (`include/board/board_config.h`) - auto-detects chip, board name, features via SDK macros
- **Board-specific pinout headers** (`include/board/pinout_pico.h`, `pinout_feather_rp2350.h`)
- **Batch build flags** in `build.sh`: `--rp2040-all`, `--rp2350-all`, `--all`

### Added - HSTX DVI Video Output (RP2350)

- **DVI driver** (`src/hal/hstx_dvi.c`) - TMDS encoding via HSTX hardware peripheral
- **Resolutions**: 640x480@60Hz, 320x240@60Hz (pixel-doubled)
- **Pixel formats**: RGB332 (8-bit), RGB565 (16-bit)
- **DMA-driven scanout** via `DREQ_HSTX` on GPIO 12-19
- **`dvi` shell command** - init, start, stop, test pattern, fill, status

### Changed - HAL Platform Abstraction

- GPIO pin limit uses `NUM_BANK0_GPIOS` from SDK `platform_defs.h` instead of hardcoded 29
- DMA channel count uses `NUM_DMA_CHANNELS` (12 for RP2040, 16 for RP2350)
- PIO block count uses `NUM_PIOS` (2 for RP2040, 3 for RP2350)
- PIO state machines per block uses `NUM_PIO_STATE_MACHINES`
- DMA DREQ enum replaced with SDK's `DREQ_*` defines
- Removed hardcoded `-mcpu=cortex-m0plus -mthumb` flags (SDK handles per-platform)

### Changed - System Info

- `system_info.c` uses `board_config.h` values instead of hardcoded RP2040 constants
- `littlefetch.c` displays correct chip model, core type, and RAM size per board
- RISC-V Hazard3 core detection via `PICO_RISCV` macro
- Flash size from `PICO_FLASH_SIZE_BYTES` (2MB Pico, 8MB Feather)

### Changed - Build System

- `build.sh` expanded with interactive board selection menu (8 options)
- CMake board selection replaces `LITTLEOS_PICO_W` option for WiFi boards
- RP2350 targets automatically enable HSTX sources and `LITTLEOS_HAS_HSTX=1`

### Fixed

- `procfs.c` compilation on RP2350 - wrapped `GPIO_FUNC_XIP` with `#if !PICO_RP2350`
- Added `GPIO_FUNC_PIO2` and `GPIO_FUNC_HSTX` for RP2350 GPIO function enumeration
- `cmd_pinout.c` board name display uses `BOARD_NAME` from `board_config.h`

### Technical Details

**Supported chips:**

| Chip | SRAM | Cores | DMA | PIO | GPIO | HSTX |
|------|------|-------|-----|-----|------|------|
| RP2040 | 264 KB | 2x Cortex-M0+ | 12 ch | 2 blocks | 30 pins | No |
| RP2350 | 520 KB | 2x Cortex-M33 or Hazard3 | 16 ch | 3 blocks | 48 pins | Yes |

---

## [0.6.0] - 2025-03-13

### Added - Debug & Diagnostics Suite

- **`logcat`** - Structured logging with tag/level filters
- **`trace`** - Execution trace buffer with function-level timing
- **`watchpoint`** - Memory watchpoints (break on read/write to address ranges)
- **`benchmark`** - Performance benchmarks (cpu, mem, gpio, fs)
- **`selftest`** - Hardware self-test suite (RAM, flash, GPIO, ADC, timers)
- **`coredump`** - Crash dump viewer (survives soft reboot via `.uninitialized_data` section)
- **`syslog`** - Persistent system log (survives soft reboot)

### Added - Hardware Tools

- **`i2cscan`** - I2C bus scanner (detect devices on I2C0/I2C1)
- **`wire`** - Interactive I2C/SPI REPL for raw bus communication
- **`pwmtune`** - PWM frequency/duty cycle tuner with live adjustment
- **`adc`** - ADC read/stream/stats (continuous sampling, min/max/avg)
- **`gpiowatch`** - GPIO state monitor (watch pins for changes)
- **`neopixel`** - WS2812 NeoPixel LED control (set colors, patterns, animations)
- **`display`** - SSD1306 OLED display driver (text, pixels, shapes)

### Added - Networking (Pico W)

- **WiFi support** via `pico_cyw43_arch_lwip_threadsafe_background`
- **`net`** command - WiFi scan/connect/status, TCP/UDP sockets, ping, HTTP GET, DNS
- **`mqtt`** - MQTT IoT client (connect, publish, subscribe)
- **`remote`** - Remote shell over TCP
- **`ota`** - Over-the-air firmware update framework
- **lwIP configuration** (`include/lwipopts.h`) - conservative memory settings for RP2040
- Pico W build: `cmake -DLITTLEOS_PICO_W=ON ..`

### Added - Shell Enhancements

- **`env`** - Environment variables (`$HOME`, `$PATH`, etc.)
- **`alias`** - Command aliases (`alias ll="ls -la"`)
- **`export`** - Set/modify environment variables
- **`screen`** - Terminal multiplexer (split panes)
- **`man`** - Built-in manual pages for all commands
- **`top`** - Live system monitor (htop-style, auto-refreshing)
- **Pipes** - `cmd1 | cmd2` pipe output between commands
- **Output redirect** - `echo hello > file`
- **`!!`** - Repeat last command
- **Tab completion** for command names

### Added - Text Processing

- **`cat`** - Display file contents
- **`echo`** - Print text with redirect support
- **`head`** / **`tail`** - Show first/last lines
- **`wc`** - Word/line/byte count
- **`grep`** - Pattern search in files
- **`hexdump`** - Hex dump of file contents
- **`tee`** - Duplicate output to file

### Added - Virtual Filesystems

- **`proc`** - Process filesystem (`/proc/cpuinfo`, `/proc/meminfo`, etc.)
- **`dev`** - Device filesystem (`/dev/gpio`, `/dev/uart`, etc.)

### Added - System Services

- **`cron`** - Scheduled task execution (add, list, remove jobs)
- **`pkg`** - Package manager framework
- **`sensor`** - Sensor framework with validation and logging
- **`power`** - Power management and sleep modes
- **`ipc`** - Inter-process communication channels
- **`profile`** - Runtime profiling

### Added - System Info

- **`fetch`** - System info display (neofetch-style)
- **`pinout`** - Visual GPIO pin diagram

### Changed - SageLang Submodule

- Updated to commit `24dd9ba` with complete `PICO_BUILD` guards
- Source layout changed: C sources now in `src/c/`, self-hosting Sage in `src/sage/`
- Added `sage_thread.c` to build (provides thread stubs for embedded)
- Uses `SAGE_PLATFORM_PICO` and `SAGE_HAS_THREADS` macros

### Changed - RAM Optimization

Reduced BSS from 167KB to 105KB (62KB freed):

- `DMESG_BUFFER_SIZE` 128 -> 64, `DMESG_MSG_MAX` 120 -> 96
- `LOGCAT_MAX_ENTRIES` 128 -> 64, `LOGCAT_MAX_MSG_LEN` 80 -> 64
- `TRACE_MAX_ENTRIES` 256 -> 128, `TRACE_MAX_NAME_LEN` 24 -> 16
- `HISTORY_SIZE` 20 -> 10, `MAX_CMD_LEN` 512 -> 256
- `IPC_MAX_MSG_SIZE` 128 -> 64, `IPC_MAX_CHANNELS` 8 -> 4
- `CONFIG_MAX_VALUE_LEN` 256 -> 128, `CONFIG_MAX_ENTRIES` 32 -> 16
- `SHELL_ENV_MAX_VARS` 32 -> 16, `SHELL_ALIAS_MAX` 16 -> 8
- `CRON_MAX_JOBS` 8 -> 4
- FS backend blocks 128 -> 16

### Fixed

- `.noinit` section overlap with `.bss` - changed to `.uninitialized_data` (Pico SDK native)
- `cmd_neopixel.c` `uint` type error - changed to `unsigned int`
- `cmd_remote.c` missing `pico/stdlib.h` for `absolute_time_t`
- lwIP `TCP_SND_QUEUELEN` sanity check failure

### Technical Details

**Binary sizes:**

- Standard Pico: text=361KB, BSS=105KB, ~151KB free RAM
- Pico W (WiFi): text=679KB, BSS=135KB, ~121KB free RAM

---

## [0.5.0] - 2025-02-15

### Added - System Infrastructure

- **Task scheduler** - Cooperative multitasking with priority support
- **IPC subsystem** - Message-passing channels between tasks
- **OTA framework** - Over-the-air update infrastructure
- **DMA engine** - DMA transfer management
- **Power management** - Sleep modes and power state control
- **Sensor framework** - Unified sensor registration, polling, and validation
- **Runtime profiler** - Function-level timing and call counting
- **Memory accounting** - Per-task memory tracking

### Added - Hardware Drivers

- **PIO** - Programmable I/O state machine control
- **USB** - USB device mode (CDC, HID, MSC)
- **Hardware peripherals** (`hw` command) - I2C, SPI, PWM, ADC access

### Changed

- Kernel boot sequence expanded with IPC, OTA, DMA, power, sensor, profiler init
- Shell environment system (env vars, aliases, prompt customization)
- Security context integrated into boot flow

---

## [0.4.0] - 2025-12-29

### Added - Production Filesystem (F2FS-inspired)

**Complete modular filesystem with crash recovery, directories, and full shell integration**

#### Core Implementation (`src/drivers/fs/`)

- **`fs.h`** - Complete public API + on-disk structures
  - Superblock with CRC32 protection
  - Dual checkpoints (CP0/CP1) for crash recovery
  - NAT (Node Address Table) - log-structured inodes
  - SIT (Segment Information Table) - wear leveling
  - 512B blocks, 4KB segments, 256 inodes max
- **`fs_core.c`** - Metadata + lifecycle (format/mount/sync/fsck)
- **`fs_inode.c`** - Inode I/O + direct block mapping
- **`fs_dir.c`** - Hash-based directory lookup + slack space optimization
- **`fs_file.c`** - Path resolution + read/write/seek/open/close

#### Shell Integration (`src/shell/cmd_fs.c`)

```bash
fs init 128     # Format RAM FS
fs mount        # Mount (auto-recovery after reboot!)
fs mkdir /test
fs touch /file.txt
fs write /file.txt "Hello FS!"
fs cat /file.txt
fs ls /
fs sync         # Persist checkpoints
fs info         # Superblock + runtime stats
```

---

## [0.3.0] - 2025-12-02

### Added - Watchdog Timer System

- Hardware watchdog timer driver (8 second default timeout)
- Multi-layer feed protection (shell, REPL, scripts)
- Crash recovery detection and reporting
- SageLang bindings (`wdt_enable`, `wdt_disable`, `wdt_feed`, `wdt_get_timeout`)

### Added - Script Storage System

- Flash-based script storage (up to 8 slots, 4KB flash sector)
- Auto-boot support via config storage
- Shell commands: `script save/list/run/delete/show/autoboot/noboot`

### Added - System Information Module

- CPU, memory, temperature, board ID, uptime monitoring
- SageLang bindings (`sys_version`, `sys_uptime`, `sys_temp`, `sys_clock`, etc.)

### Added - GPIO Integration

- Platform-independent GPIO HAL (init, read, write, toggle, pull config)
- SageLang bindings (`gpio_init`, `gpio_write`, `gpio_read`, `gpio_toggle`, `gpio_set_pull`)

### Added - Timing Functions

- SageLang bindings (`sleep`, `sleep_us`, `time_ms`, `time_us`)

### Added - Configuration Storage

- Flash-backed key-value store (16 entries, 128-byte values)
- SageLang bindings (`config_set`, `config_get`, `config_has`, `config_remove`, `config_save`, `config_load`)

### Added - Memory Management Core

- 32KB heap with linked-list allocator and coalescing
- Guard bytes, fragmentation tracking, leak detection
- Shell commands: `memory stats/available/leaks/test/defrag/threshold`

### Added - User & Permission System

- Multi-user with capability-based access control
- Unix-style permission bits (rwx, owner/group/other)
- Shell commands: `users list/get/exists`, `perms decode/check/presets`

---

## [0.2.0] - 2024-11-28

### Added

- SageLang integration with full REPL
- Interactive shell with command history
- USB and UART dual I/O support
- Embedded garbage collection
- 64KB heap for scripts

---

## [0.1.0] - 2024-11-20

### Added

- Initial littleOS kernel
- Basic UART communication
- Minimal shell interface
- Pico SDK integration
- Boot sequence

---

## Legend

- **Added** - New features
- **Changed** - Changes to existing functionality
- **Deprecated** - Soon-to-be removed features
- **Removed** - Removed features
- **Fixed** - Bug fixes
- **Security** - Vulnerability fixes
