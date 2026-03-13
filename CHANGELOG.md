# Changelog

All notable changes to littleOS. Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
