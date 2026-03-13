---
title: "littleOS"
subtitle: "A Comprehensive Guide"
author: "Jacob Yates"
date: "March 2026"
toc: true
---


# littleOS: A Comprehensive Guide

## Executive Summary

**littleOS** is a **microkernel operating system** written in C for the Raspberry Pi Pico family of microcontrollers. It targets the RP2040 (ARM Cortex-M0+) and RP2350 (ARM Cortex-M33 / RISC-V Hazard3) chips, providing a Unix-inspired shell environment, cooperative multitasking, an F2FS-inspired filesystem, a hardware abstraction layer, networking (WiFi on Pico W boards), DVI video output (RP2350 via HSTX), and an embedded SageLang scripting interpreter. The system runs bare-metal on the Pico SDK with no external RTOS, fitting the entire OS — kernel, shell, filesystem, drivers, and scripting runtime — into 264 KB of SRAM (RP2040) or 520 KB (RP2350) with 2–8 MB of flash storage.

This guide documents the complete architecture, subsystem design, shell command reference, HAL interfaces, build system, and hardware details derived from the full C source implementation.

---

## Part 1: Architecture Overview and Design Philosophy

### 1.1 Design Goals

littleOS is designed as a **practical embedded operating system** that:

- **Provides a Unix-like shell experience** on bare-metal microcontrollers, with commands like `ls`, `cat`, `grep`, `top`, `cron`, and environment variables.
- **Abstracts hardware differences** between RP2040 and RP2350 through a board configuration layer and platform-aware HAL.
- **Supports scripting** via the embedded SageLang interpreter, enabling runtime GPIO control, configuration, and automation without reflashing.
- **Prioritizes safety** with a dual-core supervisor model (Core 1 monitors Core 0), hardware watchdog, capability-based permissions, and crash recovery via persistent coredumps.
- **Fits in constrained memory** with careful BSS budgeting (105 KB on RP2040), bump allocators, and compile-time buffer sizing.

### 1.2 System Characteristics

| Feature | Details |
|---------|---------|
| **Language** | C11 (kernel, HAL, drivers), C17 (SageLang integration) |
| **Target Chips** | RP2040 (Cortex-M0+), RP2350 (Cortex-M33 or Hazard3 RISC-V) |
| **Supported Boards** | Pico, Pico W, Pico 2, Pico 2 W, Adafruit Feather RP2350 (8 build targets) |
| **SDK** | Raspberry Pi Pico SDK (handles boot, linker, clocks, per-platform toolchain) |
| **Multitasking** | Cooperative scheduler, 16 tasks max, priority-based with security contexts |
| **Filesystem** | F2FS-inspired RAM filesystem with NAT, SIT, dual checkpoints, CRC32 |
| **Shell** | 50+ commands, pipes, redirection, aliases, env vars, tab completion, history |
| **Scripting** | SageLang interpreter (Python-like syntax, GC, GPIO/timer/config bindings) |
| **Networking** | WiFi (CYW43), TCP/UDP sockets, DNS, HTTP GET, MQTT, remote shell (Pico W only) |
| **Video Output** | DVI via HSTX peripheral, 640x480@60Hz, TMDS encoding (RP2350 only) |
| **Security** | Unix-style UIDs/GIDs, capability flags, per-resource permissions |
| **Monitoring** | Core 1 supervisor, hardware watchdog (8s), temperature/memory alerts |

### 1.3 Execution Model

littleOS operates as a **single-binary bare-metal kernel**:

1. **Boot** (`boot/boot.c`) — `main()` calls `kernel_init()` then `shell_run()`
2. **Kernel Init** (`src/kernel/kernel.c`) — 29-step initialization sequence
3. **Shell Loop** (`src/shell/shell.c`) — UART-based REPL with command dispatch
4. **Supervisor** (`src/drivers/supervisor.c`) — Core 1 health monitor runs independently
5. **Scheduler** (`src/drivers/scheduler.c`) — Cooperative task switching from shell or scripts

The system is **not preemptive** — tasks yield explicitly via `scheduler_yield()` or return from their entry function. The watchdog timer (8-second timeout) ensures recovery from hangs, and the supervisor on Core 1 monitors temperature, memory, and heartbeats from Core 0.

### 1.4 Module Dependency Graph

```
boot/boot.c                          [Entry point → kernel_init() → shell_run()]
  │
  ├─ src/kernel/kernel.c             [29-step init sequence, boot orchestration]
  │    ├─ kernel/dmesg.c             [Ring buffer kernel log, 64 messages × 96 chars]
  │    ├─ kernel/memory_segmented.c  [Bump allocator: 32KB kernel + 32KB interpreter heaps]
  │    └─ kernel/ipc.c               [Message channels, semaphores, shared memory]
  │
  ├─ src/shell/shell.c               [Command dispatch, history, pipes, redirection]
  │    ├─ shell/cmd_fs.c             [Filesystem commands]
  │    ├─ shell/cmd_hw.c             [Hardware peripheral access]
  │    ├─ shell/cmd_net.c            [WiFi/network commands]
  │    ├─ shell/cmd_sage.c           [SageLang REPL/eval]
  │    ├─ shell/cmd_display_dvi.c    [DVI output control (RP2350)]
  │    └─ ... (40+ command modules)
  │
  ├─ src/drivers/                    [Hardware drivers]
  │    ├─ uart.c                     [UART I/O with permission checks]
  │    ├─ supervisor.c               [Core 1 health monitor]
  │    ├─ scheduler.c                [Cooperative task scheduler]
  │    ├─ watchdog.c                 [Hardware watchdog timer]
  │    ├─ fs/fs_core.c              [Filesystem metadata, mount, fsck]
  │    ├─ fs/fs_file.c              [File I/O, path resolution]
  │    ├─ fs/fs_dir.c               [Directory operations]
  │    ├─ fs/fs_inode.c             [Inode management]
  │    ├─ net.c                      [WiFi, TCP/UDP, DNS, HTTP (Pico W)]
  │    ├─ mqtt.c                     [MQTT IoT client]
  │    ├─ sensor.c                   [Sensor framework]
  │    └─ neopixel.c, display.c      [LED/OLED drivers]
  │
  ├─ src/hal/                        [Hardware Abstraction Layer]
  │    ├─ gpio.c                     [GPIO init/read/write/toggle]
  │    ├─ dma.c                      [DMA transfers, memcpy, callbacks]
  │    ├─ pio.c                      [PIO state machines, WS2812, UART TX]
  │    ├─ adc.c                      [ADC sampling, temperature]
  │    ├─ power.c                    [Sleep modes, clock scaling, peripherals]
  │    ├─ hstx_dvi.c                 [HSTX DVI output (RP2350 only)]
  │    ├─ i2c.c, spi.c, pwm.c       [Bus/peripheral drivers]
  │    ├─ flash.c                    [Flash read/write/erase]
  │    └─ usb_device.c              [USB CDC/HID/MSC]
  │
  ├─ src/sys/                        [System services]
  │    ├─ permissions.c              [UID/GID, capabilities, access control]
  │    ├─ system_info.c              [CPU, memory, temp, uptime]
  │    ├─ littlefetch.c              [neofetch-style system display]
  │    ├─ shell_env.c                [Environment variables, aliases, prompt]
  │    ├─ profiler.c                 [Function-level timing, CPU stats]
  │    ├─ procfs.c                   [/proc virtual filesystem]
  │    ├─ devfs.c                    [/dev virtual filesystem]
  │    ├─ cron.c                     [Scheduled task execution]
  │    ├─ logcat.c, trace.c          [Structured logging, execution tracing]
  │    ├─ coredump.c, syslog.c       [Crash dumps, persistent logs]
  │    └─ watchpoint.c               [Memory watchpoints]
  │
  ├─ src/sage/                       [SageLang embeddings]
  │    ├─ sage_embed.c               [Interpreter init, eval, REPL, heartbeat]
  │    ├─ sage_gpio.c                [GPIO native functions for scripts]
  │    ├─ sage_system.c              [System info native functions]
  │    ├─ sage_time.c                [Timing native functions]
  │    ├─ sage_config.c              [Config storage native functions]
  │    └─ sage_watchdog.c            [Watchdog native functions]
  │
  ├─ src/storage/                    [Persistent storage]
  │    ├─ script_storage.c           [Flash-based script slots]
  │    └─ config_storage.c           [Flash-backed key-value store]
  │
  ├─ src/usr/                        [User management]
  │    └─ users_config.c             [User database, UID/GID mapping]
  │
  └─ include/board/                  [Board abstraction]
       ├─ board_config.h             [Auto-detect chip, board, features]
       ├─ pinout_pico.h              [Pico 40-pin DIP pinout table]
       └─ pinout_feather_rp2350.h    [Feather form factor pinout table]
```

---

## Part 2: Supported Hardware

### 2.1 Chip Comparison

| Feature | RP2040 | RP2350 |
|---------|--------|--------|
| **CPU Cores** | 2x ARM Cortex-M0+ @ 133 MHz | 2x ARM Cortex-M33 @ 150 MHz or 2x Hazard3 RISC-V |
| **SRAM** | 264 KB | 520 KB |
| **Flash** | External QSPI (2–16 MB) | External QSPI (2–16 MB) |
| **GPIO Pins** | 30 (GP0–GP29) | 48 (GP0–GP47) |
| **ADC Channels** | 5 (4 external + temp) | 5 (4 external + temp) |
| **DMA Channels** | 12 | 16 |
| **PIO Blocks** | 2 (4 SMs each) | 3 (4 SMs each) |
| **HSTX** | No | Yes (DVI/TMDS output) |
| **FPU** | No (soft float) | Yes (single-precision, M33 only) |
| **Security** | None | ARM TrustZone (M33), secure boot |
| **USB** | USB 1.1 Host/Device | USB 1.1 Host/Device |

### 2.2 Supported Boards

| Board Target | Chip | CPU | WiFi | HSTX | Flash | Form Factor |
|-------------|------|-----|------|------|-------|-------------|
| `pico` | RP2040 | Cortex-M0+ | No | No | 2 MB | 40-pin DIP |
| `pico_w` | RP2040 | Cortex-M0+ | CYW43439 | No | 2 MB | 40-pin DIP |
| `pico2` | RP2350 | Cortex-M33 | No | Yes | 4 MB | 40-pin DIP |
| `pico2_riscv` | RP2350 | Hazard3 RISC-V | No | Yes | 4 MB | 40-pin DIP |
| `pico2_w` | RP2350 | Cortex-M33 | CYW43439 | Yes | 4 MB | 40-pin DIP |
| `pico2_w_riscv` | RP2350 | Hazard3 RISC-V | CYW43439 | Yes | 4 MB | 40-pin DIP |
| `adafruit_feather_rp2350` | RP2350 | Cortex-M33 | No | Yes | 8 MB | Feather |
| `adafruit_feather_rp2350_riscv` | RP2350 | Hazard3 RISC-V | No | Yes | 8 MB | Feather |

### 2.3 Board Detection

Board identity is resolved at compile time through SDK-defined macros in `include/board/board_config.h`:

```c
#if defined(ADAFRUIT_FEATHER_RP2350)
    #define BOARD_NAME      "Adafruit Feather RP2350"
    #define BOARD_HAS_HSTX  1
    #define BOARD_HAS_WIFI  0
#elif defined(RASPBERRYPI_PICO2_W)
    #define BOARD_NAME      "Raspberry Pi Pico 2 W"
    #define BOARD_HAS_HSTX  1
    #define BOARD_HAS_WIFI  1
#elif defined(RASPBERRYPI_PICO2)
    #define BOARD_NAME      "Raspberry Pi Pico 2"
    #define BOARD_HAS_HSTX  1
    #define BOARD_HAS_WIFI  0
#elif defined(PICO_W)
    #define BOARD_NAME      "Raspberry Pi Pico W"
    #define BOARD_HAS_HSTX  0
    #define BOARD_HAS_WIFI  1
#else
    #define BOARD_NAME      "Raspberry Pi Pico"
    #define BOARD_HAS_HSTX  0
    #define BOARD_HAS_WIFI  0
#endif
```

Chip-level constants are derived from the SDK's `hardware/platform_defs.h`:

| Constant | RP2040 Value | RP2350 Value | Source |
|----------|-------------|-------------|--------|
| `NUM_BANK0_GPIOS` | 30 | 48 | SDK platform_defs.h |
| `NUM_DMA_CHANNELS` | 12 | 16 | SDK platform_defs.h |
| `NUM_PIOS` | 2 | 3 | SDK platform_defs.h |
| `NUM_PIO_STATE_MACHINES` | 4 | 4 | SDK platform_defs.h |
| `PICO_RP2350` | 0 | 1 | SDK platform macro |
| `PICO_RISCV` | 0 | 1 (if RISC-V) | SDK platform macro |

---

## Part 3: Building littleOS

### 3.1 Build Requirements

- **Pico SDK** (set `PICO_SDK_PATH` environment variable)
- **ARM toolchain**: `arm-none-eabi-gcc` (for RP2040 and RP2350 ARM targets)
- **RISC-V toolchain**: `riscv32-unknown-elf-gcc` (for RP2350 RISC-V targets, optional)
- **CMake** >= 3.13
- **SageLang submodule**: `git submodule update --init --recursive`

### 3.2 Single Board Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk

mkdir build && cd build
cmake -DLITTLEOS_BOARD=pico ..
make -j$(nproc)
```

The `LITTLEOS_BOARD` variable selects the target. Valid values: `pico`, `pico_w`, `pico2`, `pico2_riscv`, `pico2_w`, `pico2_w_riscv`, `adafruit_feather_rp2350`, `adafruit_feather_rp2350_riscv`.

### 3.3 Batch Builds

The `build.sh` script supports batch builds:

```bash
./build.sh --rp2040-all    # Build pico + pico_w
./build.sh --rp2350-all    # Build pico2, pico2_riscv, pico2_w, pico2_w_riscv,
                           #   adafruit_feather_rp2350, adafruit_feather_rp2350_riscv
./build.sh --all           # Build all 8 targets
./build.sh                 # Interactive board selection menu
```

Each target produces output in `build_<board>/` with `.uf2`, `.elf`, `.bin`, and `.map` files.

### 3.4 Build-Time Configuration

| CMake Variable | Default | Description |
|---------------|---------|-------------|
| `LITTLEOS_BOARD` | `pico` | Target board (see 3.2) |
| `LITTLEOS_ENABLE_USER_ACCOUNT` | `OFF` | Enable non-root user account |
| `LITTLEOS_USER_UID` | `1000` | Custom user UID |
| `LITTLEOS_USER_NAME` | `user` | Custom username |
| `LITTLEOS_USER_UMASK` | `0022` | Default umask |
| `LITTLEOS_USER_CAPABILITIES` | `0` | Capability bitmask |
| `LITTLEOS_STARTUP_TASK_UID` | `0` | UID for startup task (0 = root) |
| `LITTLEOS_DEBUG_USERS` | `OFF` | Enable user database debug output |

Example with user account enabled:

```bash
cmake -DLITTLEOS_BOARD=pico \
      -DLITTLEOS_ENABLE_USER_ACCOUNT=ON \
      -DLITTLEOS_USER_NAME=kraken \
      -DLITTLEOS_USER_UID=1000 \
      -DLITTLEOS_USER_CAPABILITIES=0x0F ..
```

### 3.5 Flashing

**UF2 drag-and-drop** (all boards):
1. Hold BOOTSEL button while connecting USB
2. Copy `littleos.uf2` to the mounted USB drive

**Picotool** (alternative):
```bash
picotool load build/littleos.uf2
picotool reboot
```

### 3.6 Serial Console

Connect to UART0 (GP0 = TX, GP1 = RX) at 115200 baud:

```bash
minicom -b 115200 -D /dev/ttyACM0
# or
screen /dev/ttyACM0 115200
```

### 3.7 Emulation with Bramble

littleOS can be run in the Bramble RP2040 emulator:

```bash
bramble build/littleos.uf2 -clock 125 -stdin
```

This enables interactive shell access without physical hardware.

---

## Part 4: Kernel Boot Sequence

### 4.1 Initialization Order

The kernel performs a 29-step initialization in `kernel_init()` (`src/kernel/kernel.c`). The order is critical — memory must initialize first, logging second, then hardware, then services:

| Step | Function | Subsystem |
|------|----------|-----------|
| 1 | `memory_init()` | Kernel + interpreter heaps (32 KB each) |
| 2 | `dmesg_init()` | Kernel message ring buffer |
| 3 | `littleos_uart_init()` | UART with permission checks |
| 4 | `scheduler_init()` | Task scheduler (16 slots) |
| 5 | `config_init()` | Flash-backed key-value store |
| 6 | `users_init()` | User database |
| 7 | `sage_init()` | SageLang interpreter context |
| 8 | `script_storage_init()` | Flash script slots |
| 9 | Autoboot script | Execute if configured |
| 10 | `users_root_context()` | Root security context |
| 11 | `init_device_permissions()` | Device/subsystem permissions |
| 12 | `ipc_init()` | Message channels, semaphores |
| 13 | `memory_accounting_init()` | Per-task memory tracking |
| 14 | `ota_init()` | Over-the-air update framework |
| 15 | `dma_hal_init()` | DMA engine |
| 16 | `power_init()` | Power management |
| 17 | `sensor_init()` | Sensor framework |
| 18 | `profiler_init()` | Runtime profiler |
| 19 | `shell_env_init()` | Environment variables, aliases |
| 20 | `procfs_init()` | /proc virtual filesystem |
| 21 | `devfs_init()` | /dev virtual filesystem |
| 22 | `cron_init()` | Scheduled tasks |
| 23 | `net_init()` | WiFi (Pico W only) |
| 24 | `mqtt_init()` | MQTT client |
| 25 | `pkg_init()` | Package manager |
| 26 | `tmux_init()` | Terminal multiplexer |
| 27 | Debug subsystem init | logcat, trace, coredump, syslog |
| 28 | `wdt_enable(8000)` | Hardware watchdog (8s timeout) |
| 29 | `supervisor_init()` | Core 1 health monitor |

After initialization completes, the kernel prints the welcome banner and enters `shell_run()`.

### 4.2 Dual-Core Model

- **Core 0**: Runs the kernel, shell, scheduler, and all user tasks
- **Core 1**: Runs the supervisor — an independent health monitor that checks temperature, memory usage, watchdog status, and Core 0 heartbeats every 100 ms

The cores communicate through the RP2040/RP2350 hardware FIFO (multicore mailbox). Core 0 sends periodic heartbeats; if Core 1 detects a stall, it can trigger alerts or recovery actions.

---

## Part 5: Memory Management

### 5.1 Memory Layout

**RP2040 (264 KB SRAM):**

```
0x20000000 ┌──────────────────────────┐
           │ .data / .bss             │ ~105 KB (static allocations)
           ├──────────────────────────┤
           │ Kernel Heap              │ 32 KB (LITTLEOS_KERNEL_HEAP_SIZE)
           ├──────────────────────────┤
           │ Interpreter Heap         │ 32 KB (LITTLEOS_INTERPRETER_HEAP_SIZE)
           ├──────────────────────────┤
           │ Stack + Free             │ ~95 KB remaining
0x20042000 └──────────────────────────┘
```

**RP2350 (520 KB SRAM):**

```
0x20000000 ┌──────────────────────────┐
           │ .data / .bss             │ ~105 KB (static allocations)
           ├──────────────────────────┤
           │ Kernel Heap              │ 32 KB
           ├──────────────────────────┤
           │ Interpreter Heap         │ 32 KB
           ├──────────────────────────┤
           │ Stack + Free             │ ~351 KB remaining
0x20082000 └──────────────────────────┘
```

### 5.2 Bump Allocator

littleOS uses a **segmented bump allocator** (`src/kernel/memory_segmented.c`). Each heap (kernel and interpreter) is a contiguous region with a bump pointer that advances on allocation. There is no individual `free()` — the interpreter heap can be bulk-reset between script executions via `interpreter_heap_reset()`.

**Key constants:**

| Constant | Value | Description |
|----------|-------|-------------|
| `LITTLEOS_KERNEL_HEAP_SIZE` | 0x8000 (32 KB) | Kernel heap size |
| `LITTLEOS_INTERPRETER_HEAP_SIZE` | 0x8000 (32 KB) | SageLang interpreter heap |
| Alignment | 8 bytes | ARM alignment requirement |

**Allocation API:**

```c
void  memory_init(void);
void *kernel_malloc(size_t size);
void *kernel_calloc(size_t count, size_t size);
void *interpreter_malloc(size_t size);
void *interpreter_calloc(size_t count, size_t size);
void  interpreter_heap_reset(void);            // Bulk free
size_t interpreter_heap_remaining(void);       // Free bytes
```

**Statistics:**

```c
typedef struct {
    size_t kernel_used, kernel_free, kernel_peak;
    size_t interpreter_used, interpreter_free, interpreter_peak;
    float  kernel_usage_pct, interpreter_usage_pct;
    uint32_t kernel_alloc_count, interpreter_alloc_count;
} MemoryStats;

void memory_get_stats(MemoryStats *stats);
```

### 5.3 Per-Task Memory Accounting

The system tracks allocations per task (up to 16 tracked tasks) for the `top` command and leak detection:

```c
void memory_account_alloc(uint16_t task_id, size_t bytes);
void memory_account_free(uint16_t task_id, size_t bytes);
bool memory_account_get(uint16_t task_id, size_t *allocated, size_t *peak);
```

---

## Part 6: Task Scheduler

### 6.1 Cooperative Multitasking

The scheduler (`src/drivers/scheduler.c`) provides cooperative multitasking with priority-based ordering. Tasks run until they explicitly yield or terminate — there is no preemption timer.

**Configuration:**

| Constant | Value | Description |
|----------|-------|-------------|
| `LITTLEOS_MAX_TASKS` | 16 | Maximum concurrent tasks |
| `LITTLEOS_TASK_STACK_SIZE` | 4096 | Stack per task (bytes) |
| `LITTLEOS_MAX_TASK_NAME` | 32 | Task name length |

### 6.2 Task States

```
TASK_STATE_IDLE        → Not scheduled
TASK_STATE_READY       → Eligible to run
TASK_STATE_RUNNING     → Currently executing on CPU
TASK_STATE_BLOCKED     → Waiting for I/O or IPC
TASK_STATE_SUSPENDED   → Manually paused
TASK_STATE_TERMINATED  → Finished or killed
```

### 6.3 Task Priorities

```
TASK_PRIORITY_LOW      = 0    (background work)
TASK_PRIORITY_NORMAL   = 1    (default)
TASK_PRIORITY_HIGH     = 2    (time-sensitive)
TASK_PRIORITY_CRITICAL = 3    (system-critical)
```

### 6.4 Task Descriptor

Each task carries a full descriptor:

```c
typedef struct {
    uint16_t  task_id;
    char      name[32];
    task_state_t    state;
    task_priority_t priority;
    uint8_t   core_affinity;        // 0=Core0, 1=Core1, 2=Any
    task_entry_t    entry_func;
    void           *arg;
    task_security_ctx_t sec_ctx;    // uid, gid, euid, egid, umask, capabilities
    uint8_t  *stack_base;
    size_t    stack_size;
    size_t    memory_allocated;
    size_t    memory_peak;
    uint32_t  created_at_ms;
    uint32_t  total_runtime_ms;
    uint32_t  context_switches;
    uint32_t  time_slice_ms;
    uint32_t  time_remaining_ms;
} task_descriptor_t;
```

### 6.5 Scheduler API

```c
void     scheduler_init(void);
uint16_t task_create(const char *name, task_entry_t entry, void *arg,
                     task_priority_t priority, uint8_t core, uint16_t uid);
bool     task_terminate(uint16_t task_id);
bool     task_suspend(uint16_t task_id);
bool     task_resume(uint16_t task_id);
bool     task_get_descriptor(uint16_t task_id, task_descriptor_t *desc);
uint16_t task_get_current(void);
uint16_t task_get_count(void);
void     scheduler_yield(void);
```

---

## Part 7: Shell

### 7.1 Shell Architecture

The shell (`src/shell/shell.c`) is a UART-based REPL that reads lines, tokenizes them into argc/argv, resolves aliases, expands environment variables, handles pipes and redirection, and dispatches to the matching command handler.

**Shell buffer sizes:**

| Buffer | Size | Description |
|--------|------|-------------|
| Command input | 256 bytes | `MAX_CMD_LEN` |
| History | 10 entries | `HISTORY_SIZE` |
| Max arguments | 32 | `MAX_ARGS` |
| Pipe buffer | 2048 bytes | Inter-command pipe |

### 7.2 Shell Features

- **Command history**: UP/DOWN arrows navigate, `!!` repeats last, `!n` repeats nth
- **Tab completion**: Completes command names on TAB press
- **Pipes**: `cmd1 | cmd2` pipes stdout of cmd1 to stdin of cmd2
- **Output redirection**: `cmd > file` (overwrite), `cmd >> file` (append)
- **Environment variables**: `$VAR` and `${VAR}` expansion in commands
- **Aliases**: `alias ll="ls -la"` — recursive expansion up to depth 5
- **Custom prompt**: PS1 format with `\u` (user), `\h` (host), `\w` (cwd), `\$` (privilege)

**Control characters:**

| Key | Action |
|-----|--------|
| Ctrl+C | Interrupt current command |
| Ctrl+D | EOF / logout (at empty prompt) |
| Ctrl+L | Clear screen |
| Ctrl+W | Delete word |
| Ctrl+U | Clear line |
| Ctrl+A N/P | Next/previous tmux pane |

### 7.3 Command Reference

#### System Commands

| Command | Description |
|---------|-------------|
| `help` | List all available commands |
| `version` | Show littleOS and SageLang versions |
| `clear` | Clear terminal screen |
| `history` | Show command history |
| `reboot` | Restart the system |
| `exit` | Log out current user |
| `health` | System health summary |
| `stats` | System statistics |
| `supervisor` | Core 1 supervisor status |
| `dmesg` | Kernel message log |
| `fetch` | neofetch-style system info display |
| `top` | Live system monitor (htop-style) |
| `profile` | Runtime profiler output |

#### User and Permission Commands

| Command | Subcommands | Description |
|---------|-------------|-------------|
| `users` | `list`, `get`, `exists` | User database management |
| `perms` | `decode`, `check`, `presets` | Permission bit manipulation |

#### Filesystem Commands

| Command | Subcommands | Description |
|---------|-------------|-------------|
| `fs` | `init`, `mount`, `mkdir`, `touch`, `write`, `cat`, `ls`, `rm`, `sync`, `info`, `fsck` | F2FS-inspired filesystem |

#### Text Processing Commands

| Command | Description |
|---------|-------------|
| `cat` | Display file contents |
| `echo` | Print text (supports redirect) |
| `head` | Show first N lines |
| `tail` | Show last N lines |
| `wc` | Word/line/byte count |
| `grep` | Pattern search in files |
| `hexdump` | Hex dump of file contents |
| `tee` | Duplicate output to file and stdout |

#### Hardware Commands

| Command | Description |
|---------|-------------|
| `hw` | I2C, SPI, PWM, ADC peripheral access |
| `pio` | PIO state machine control |
| `dma` | DMA transfer management |
| `usb` | USB device mode (CDC, HID, MSC) |
| `pinout` | Visual GPIO pin diagram for current board |
| `i2cscan` | I2C bus scanner |
| `wire` | Interactive I2C/SPI REPL |
| `pwmtune` | PWM frequency/duty tuner |
| `adc` | ADC read/stream/stats |
| `gpiowatch` | GPIO state monitor |
| `neopixel` | WS2812 NeoPixel LED control |
| `display` | SSD1306 OLED display driver |
| `dvi` | DVI output via HSTX (RP2350 only) |

#### Networking Commands (Pico W only)

| Command | Description |
|---------|-------------|
| `net` | WiFi scan/connect/status, TCP/UDP, DNS, HTTP, ping |
| `mqtt` | MQTT IoT client (connect, publish, subscribe) |
| `remote` | Remote shell over TCP |
| `ota` | Over-the-air firmware update |

#### Scripting Commands

| Command | Description |
|---------|-------------|
| `sage` | SageLang REPL and code evaluation |
| `script` | Flash-based script storage (save/list/run/delete/autoboot) |
| `pkg` | Package manager framework |

#### Service Commands

| Command | Description |
|---------|-------------|
| `tasks` | Task scheduler management |
| `memory` | Heap stats, leak detection, defrag |
| `sensor` | Sensor framework control |
| `power` | Sleep modes, clock scaling |
| `ipc` | Inter-process communication |
| `cron` | Scheduled task execution |
| `env` | Environment variable management |
| `alias` | Command alias management |
| `export` | Set environment variables |
| `screen` | Terminal multiplexer |
| `man` | Built-in manual pages |

#### Virtual Filesystem Commands

| Command | Description |
|---------|-------------|
| `proc` | /proc filesystem (cpuinfo, meminfo, etc.) |
| `dev` | /dev filesystem (gpio, uart, etc.) |

#### Debug and Diagnostics Commands

| Command | Description |
|---------|-------------|
| `logcat` | Structured logging with tag/level filters |
| `trace` | Execution trace buffer |
| `watchpoint` | Memory watchpoints (break on read/write) |
| `benchmark` | Performance benchmarks (cpu, mem, gpio, fs) |
| `selftest` | Hardware self-test suite |
| `coredump` | Crash dump viewer (survives soft reboot) |
| `syslog` | Persistent system log (survives soft reboot) |

---

## Part 8: Filesystem

### 8.1 Design

The filesystem (`src/drivers/fs/`) is inspired by F2FS (Flash-Friendly File System) and operates entirely in RAM with optional flash persistence. It features:

- **Log-structured writes** via NAT (Node Address Table)
- **Wear leveling** via SIT (Segment Information Table)
- **Crash recovery** via dual checkpoints (CP0/CP1)
- **CRC32 integrity** on superblock, checkpoints, and inodes
- **Hash-based directory lookup** with slack space optimization

### 8.2 On-Disk Layout

```
Block 0:  Superblock (magic=0xF2FE, version, geometry, CRC32)
Block 1:  Checkpoint 0 (free count, segment pointers, orphan list)
Block 2:  Checkpoint 1 (alternate checkpoint for crash recovery)
Block 3+: NAT blocks (inode-to-physical-block mapping)
          SIT blocks (segment validity and age tracking)
          Main area (inode blocks + data blocks)
```

### 8.3 Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `FS_BLOCK_SIZE` | 512 bytes | Fundamental I/O unit |
| `FS_SEGMENT_SIZE` | 4096 bytes | Wear-leveling unit (8 blocks) |
| `FS_BLOCKS_PER_SEGMENT` | 8 | Blocks per segment |
| `FS_DEFAULT_MAX_INODES` | 256 | Maximum file/directory count |
| `FS_DIRECT_BLOCKS` | 10 | Direct block pointers per inode |
| `FS_INDIRECT_PTRS` | 128 | Indirect pointers (512 / 4) |
| `FS_MAGIC` | 0xF2FE | Superblock magic number |
| `FS_ROOT_INODE` | 2 | Root directory inode number |

### 8.4 Inode Structure

```c
typedef struct {
    uint16_t magic;
    uint16_t inode_version;
    uint16_t mode;            // FS_MODE_REG (0x8000) or FS_MODE_DIR (0x4000)
    uint32_t size;
    uint32_t atime, mtime, ctime;
    uint16_t link_count;
    uint32_t direct[10];      // Direct block pointers
    uint32_t indirect;        // Single indirect block
    uint32_t double_indirect; // Double indirect block
    uint32_t inode_num;
    uint32_t parent_inode;
    uint32_t generation;
    uint32_t crc32;
} fs_inode_t;
```

### 8.5 File API

```c
int  fs_format(fs_context_t *fs, uint32_t total_blocks);
int  fs_mount(fs_context_t *fs);
int  fs_open(fs_context_t *fs, const char *path, uint16_t flags, int *fd);
int  fs_read(fs_context_t *fs, int fd, void *buf, size_t count);
int  fs_write(fs_context_t *fs, int fd, const void *buf, size_t count);
int  fs_seek(fs_context_t *fs, int fd, int32_t offset, int whence);
int  fs_close(fs_context_t *fs, int fd);
int  fs_mkdir(fs_context_t *fs, const char *path);
int  fs_unlink(fs_context_t *fs, const char *path);
int  fs_sync(fs_context_t *fs);
int  fs_fsck(fs_context_t *fs);
```

**Open flags:** `FS_O_RDONLY` (0x0000), `FS_O_WRONLY` (0x0001), `FS_O_RDWR` (0x0002), `FS_O_APPEND` (0x0004), `FS_O_CREAT` (0x0008), `FS_O_TRUNC` (0x0010).

### 8.6 Shell Usage

```bash
fs init 128         # Format with 128 blocks (64 KB)
fs mount            # Mount filesystem
fs mkdir /home
fs touch /home/hello.txt
fs write /home/hello.txt "Hello from littleOS!"
fs cat /home/hello.txt
fs ls /home
fs sync             # Flush checkpoints
fs info             # Superblock stats
fs fsck             # Integrity check
```

---

## Part 9: Hardware Abstraction Layer

### 9.1 GPIO

The GPIO HAL (`src/hal/gpio.c`) provides a platform-independent interface. Pin ranges are determined at compile time from `NUM_BANK0_GPIOS` (30 on RP2040, 48 on RP2350).

```c
bool gpio_hal_init(uint pin, gpio_dir_t dir);
void gpio_hal_write(uint pin, bool value);
bool gpio_hal_read(uint pin);
void gpio_hal_toggle(uint pin);
void gpio_hal_set_pull(uint pin, gpio_pull_t pull);
void gpio_hal_get_pin_range(uint *min, uint *max);
```

**Directions:** `GPIO_DIR_IN`, `GPIO_DIR_OUT`
**Pull modes:** `GPIO_PULL_NONE`, `GPIO_PULL_UP`, `GPIO_PULL_DOWN`

### 9.2 DMA

The DMA HAL (`src/hal/dma.c`) abstracts DMA transfers with automatic channel claiming and callback support. Channel count is `NUM_DMA_CHANNELS` from the SDK.

```c
int  dma_hal_init(void);
int  dma_hal_claim(int channel);             // -1 = auto-assign
int  dma_hal_start(int channel, const dma_transfer_t *transfer);
int  dma_hal_wait(int channel, uint32_t timeout_ms);
bool dma_hal_busy(int channel);
int  dma_hal_abort(int channel);
int  dma_hal_memcpy(int channel, void *dst, const void *src, size_t len);
int  dma_hal_set_callback(int channel, dma_callback_t cb, void *user_data);
```

**Transfer sizes:** `DMA_XFER_SIZE_8` (byte), `DMA_XFER_SIZE_16` (halfword), `DMA_XFER_SIZE_32` (word)

**Sniffer modes:** CRC32, CRC32R (bit-reversed), CRC16, SUM

### 9.3 PIO

The PIO HAL (`src/hal/pio.c`) manages programmable I/O state machines. RP2040 has 2 PIO blocks; RP2350 has 3.

```c
int  pio_hal_init(void);
int  pio_hal_load_program(uint pio_block, const uint16_t *insns, uint len, int origin);
int  pio_hal_sm_config(uint pio_block, uint sm, const pio_sm_config_t *config);
int  pio_hal_sm_start(uint pio_block, uint sm);
int  pio_hal_sm_stop(uint pio_block, uint sm);
int  pio_hal_sm_put(uint pio_block, uint sm, uint32_t data);
int  pio_hal_sm_get(uint pio_block, uint sm, uint32_t *data);
```

**Built-in programs:**
- `pio_hal_blink_program()` — Toggle a pin at a configurable frequency
- `pio_hal_ws2812_init()` / `pio_hal_ws2812_put()` — NeoPixel WS2812 driver
- `pio_hal_uart_tx_init()` / `pio_hal_uart_tx_put()` — Software UART TX

### 9.4 ADC

The ADC HAL (`src/hal/adc.c`) provides 12-bit analog-to-digital conversion.

```c
int      adc_hal_init(void);
int      adc_hal_channel_init(uint8_t channel);
uint16_t adc_hal_read_raw(uint8_t channel);           // 0–4095
float    adc_hal_read_voltage(uint8_t channel);        // 0.0–3.3V
float    adc_hal_read_temp_c(void);                    // On-die temperature
uint16_t adc_hal_read_averaged(uint8_t channel, uint16_t samples);
uint8_t  adc_hal_channel_to_gpio(uint8_t channel);     // Channel → GPIO pin
```

| Constant | Value | Description |
|----------|-------|-------------|
| `NUM_ADC_CHANNELS` | 5 | GP26–GP29 + temperature |
| `ADC_MAX_VALUE` | 4095 | 12-bit resolution |
| `ADC_VREF` | 3.3V | Reference voltage |
| `ADC_TEMP_CHANNEL` | 4 | On-die temperature sensor |

### 9.5 Power Management

The power HAL (`src/hal/power.c`) controls sleep modes, clock frequency, voltage, and peripheral power gating.

**Sleep modes:**

| Mode | Constant | Description |
|------|----------|-------------|
| Active | `POWER_MODE_ACTIVE` | Normal operation |
| Light sleep | `POWER_MODE_LIGHT_SLEEP` | WFI, instant wake |
| Deep sleep | `POWER_MODE_DEEP_SLEEP` | RAM retained, slow wake |
| Dormant | `POWER_MODE_DORMANT` | Minimum power, GPIO/RTC wake only |

**Clock presets:**

| Preset | Frequency | Use Case |
|--------|-----------|----------|
| `POWER_CLOCK_FULL` | 125 MHz | Normal operation |
| `POWER_CLOCK_HALF` | 62.5 MHz | Reduced power |
| `POWER_CLOCK_QUARTER` | 31.25 MHz | Low power |
| `POWER_CLOCK_LOW` | 12 MHz | Minimum active |
| `POWER_CLOCK_ULTRA` | 6 MHz | Ultra-low power |

```c
int      power_init(void);
int      power_sleep(power_mode_t mode);
int      power_sleep_ms(uint32_t duration_ms);
int      power_sleep_until_gpio(uint gpio_pin, power_wake_edge_t edge);
int      power_set_clock(uint32_t freq_khz);
uint32_t power_get_clock(void);
int      power_set_voltage(float voltage);     // 0.80V – 1.30V
int      power_disable_peripheral(power_peripheral_t id);
int      power_enable_peripheral(power_peripheral_t id);
```

### 9.6 HSTX DVI Output (RP2350 Only)

The HSTX DVI driver (`src/hal/hstx_dvi.c`) provides DVI video output using the RP2350's High-Speed Serial Transmitter peripheral. It performs TMDS (Transition-Minimized Differential Signaling) encoding in hardware and uses DMA for framebuffer scanout.

**GPIO mapping:**

| GPIO | HSTX Lane | Signal |
|------|-----------|--------|
| 12–13 | Lane 2 | Blue (TMDS differential pair) |
| 14–15 | Lane 1 | Green (TMDS differential pair) |
| 16–17 | Lane 0 | Red (TMDS differential pair) |
| 18–19 | Clock | TMDS clock (differential pair) |

**Display modes:**

| Mode | Resolution | Pixel Clock | Bandwidth |
|------|-----------|-------------|-----------|
| `DVI_MODE_640x480_60HZ` | 640x480 @ 60 Hz | 25.175 MHz | Standard VGA |
| `DVI_MODE_320x240_60HZ` | 320x240 @ 60 Hz | 25.175 MHz | Pixel-doubled |

**Pixel formats:**

| Format | Bits | Description |
|--------|------|-------------|
| `DVI_PIXEL_RGB332` | 8 bpp | 3R + 3G + 2B (307 KB framebuffer at 640x480) |
| `DVI_PIXEL_RGB565` | 16 bpp | 5R + 6G + 5B (614 KB framebuffer at 640x480) |

**VGA timing (640x480@60Hz):**

```
Horizontal: 640 active + 16 front porch + 96 sync + 48 back porch = 800 total
Vertical:   480 active + 10 front porch +  2 sync + 33 back porch = 525 total
Pixel clock: 25.175 MHz → 60 Hz refresh rate
```

**API:**

```c
int  hstx_dvi_init(dvi_mode_t mode, dvi_pixel_format_t format);
int  hstx_dvi_set_framebuffer(uint8_t *fb, size_t fb_size);
int  hstx_dvi_start(void);    // Begin DMA scanout
int  hstx_dvi_stop(void);
bool hstx_dvi_is_active(void);
int  hstx_dvi_get_status(dvi_status_t *status);
void hstx_dvi_fill(uint8_t *fb, size_t fb_size, uint32_t color);
void hstx_dvi_test_pattern(uint8_t *fb, uint16_t width, uint16_t height,
                           dvi_pixel_format_t format);
```

**Shell usage:**

```bash
dvi init 640x480 rgb332     # Initialize DVI output
dvi test                    # Display color bar test pattern
dvi fill 0xFF0000           # Fill screen with red
dvi start                   # Begin output
dvi stop                    # Stop output
dvi status                  # Show resolution, format, framerate
```

---

## Part 10: Kernel Logging (dmesg)

### 10.1 Ring Buffer Design

The kernel message log (`src/kernel/dmesg.c`) is a fixed-size ring buffer that stores timestamped, leveled messages. It follows the Linux `dmesg` convention with 8 severity levels.

**Configuration:**

| Constant | Value | Description |
|----------|-------|-------------|
| `DMESG_BUFFER_SIZE` | 64 | Maximum messages in ring buffer |
| `DMESG_MSG_MAX` | 96 | Maximum message length (chars) |

### 10.2 Log Levels

```
DMESG_LEVEL_EMERG   = 0    System is unusable
DMESG_LEVEL_ALERT   = 1    Immediate action required
DMESG_LEVEL_CRIT    = 2    Critical conditions
DMESG_LEVEL_ERR     = 3    Error conditions
DMESG_LEVEL_WARN    = 4    Warning conditions
DMESG_LEVEL_NOTICE  = 5    Normal but significant
DMESG_LEVEL_INFO    = 6    Informational
DMESG_LEVEL_DEBUG   = 7    Debug-level messages
```

### 10.3 API

```c
void     dmesg_init(void);
void     dmesg_log(int level, const char *fmt, ...);
uint32_t dmesg_get_count(void);
uint32_t dmesg_get_uptime(void);       // ms since boot
void     dmesg_print_all(void);
void     dmesg_print_level(int min_level);
void     dmesg_clear(void);
```

**Convenience macros:**

```c
dmesg_info("Kernel initialized");
dmesg_warn("Memory usage at %d%%", pct);
dmesg_err("Failed to mount filesystem");
dmesg_crit("Watchdog timeout imminent");
```

---

## Part 11: Inter-Process Communication

### 11.1 IPC Primitives

The IPC subsystem (`src/kernel/ipc.c`) provides three communication mechanisms:

1. **Message Channels** — Named FIFO queues for task-to-task messaging
2. **Semaphores** — Counting semaphores for synchronization
3. **Shared Memory** — Named memory regions with locking

### 11.2 Configuration

| Constant | Value | Description |
|----------|-------|-------------|
| `IPC_MAX_MSG_SIZE` | 64 bytes | Maximum message payload |
| `IPC_MAX_CHANNELS` | 4 | Maximum message channels |
| `IPC_CHANNEL_DEPTH` | 8 | Messages per channel queue |
| `IPC_CHANNEL_NAME_LEN` | 16 | Channel name length |
| `IPC_MAX_SEMAPHORES` | 8 | Maximum semaphores |
| `IPC_MAX_SHMEM_REGIONS` | 4 | Maximum shared memory regions |
| `IPC_SHMEM_MAX_SIZE` | 1024 bytes | Maximum shared memory size |

### 11.3 Message API

```c
void ipc_init(void);
int  ipc_channel_create(const char *name);
int  ipc_send(int channel_id, uint16_t sender, uint16_t msg_type,
              const void *payload, uint16_t len, uint8_t priority);
int  ipc_recv(int channel_id, ipc_message_t *msg);   // Non-blocking
int  ipc_peek(int channel_id, ipc_message_t *msg);
int  ipc_pending(int channel_id);
int  ipc_channel_find(const char *name);
```

**Message priorities:** `IPC_PRIORITY_LOW` (0), `IPC_PRIORITY_NORMAL` (1), `IPC_PRIORITY_HIGH` (2), `IPC_PRIORITY_URGENT` (3)

**Message flags:** `IPC_FLAG_NONE`, `IPC_FLAG_URGENT`, `IPC_FLAG_REPLY`, `IPC_FLAG_BROADCAST`

### 11.4 Semaphore API

```c
int ipc_sem_create(const char *name, uint32_t initial, uint32_t max);
int ipc_sem_post(int sem_id);
int ipc_sem_wait(int sem_id);
int ipc_sem_trywait(int sem_id);
```

### 11.5 Shared Memory API

```c
int   ipc_shmem_create(const char *name, size_t size, uint16_t owner);
void *ipc_shmem_attach(int shm_id);
int   ipc_shmem_lock(int shm_id, uint16_t task_id);
int   ipc_shmem_unlock(int shm_id, uint16_t task_id);
int   ipc_shmem_read(int shm_id, size_t offset, void *data, size_t len);
int   ipc_shmem_write(int shm_id, size_t offset, const void *data, size_t len);
```

---

## Part 12: Permissions and Security

### 12.1 User Model

littleOS implements a Unix-inspired permission system with UIDs, GIDs, and capability flags.

**UID ranges:**

| Range | Purpose |
|-------|---------|
| 0 | Root (full access) |
| 1–999 | System accounts |
| 1000+ | User accounts |
| 0xFFFF | Invalid UID |

**GID assignments:**

| GID | Group |
|-----|-------|
| 0 | root |
| 1 | system |
| 2 | drivers |
| 100 | users |

### 12.2 Permission Bits

Permissions follow the Unix `rwx` model with owner, group, and other triplets:

```
PERM_READ  = 4 (0400)
PERM_WRITE = 2 (0200)
PERM_EXEC  = 1 (0100)
```

**Common presets:** `PERM_0644` (rw-r--r--), `PERM_0755` (rwxr-xr-x), `PERM_0700` (rwx------), `PERM_0600` (rw-------)

### 12.3 Capability Flags

Capabilities provide fine-grained privilege control independent of UID:

| Flag | Bit | Permission |
|------|-----|------------|
| `CAP_SYS_ADMIN` | 0 | System administration |
| `CAP_SYS_BOOT` | 1 | Reboot/shutdown |
| `CAP_GPIO_WRITE` | 2 | GPIO pin control |
| `CAP_UART_CONFIG` | 3 | UART configuration |
| `CAP_TASK_SPAWN` | 4 | Create tasks |
| `CAP_TASK_KILL` | 5 | Terminate tasks |
| `CAP_MEM_LOCK` | 6 | Lock memory regions |
| `CAP_NET_ADMIN` | 7 | Network administration |
| `CAP_ALL` | all | Full capabilities (0xFFFFFFFF) |

### 12.4 Permission API

```c
bool perm_check(const task_security_ctx_t *ctx, uint16_t resource_perm,
                uint16_t required_perm);
bool perm_has_capability(const task_security_ctx_t *ctx, uint32_t capability);
void perm_grant_capability(task_security_ctx_t *ctx, uint32_t capability);
void perm_revoke_capability(task_security_ctx_t *ctx, uint32_t capability);
bool perm_seteuid(task_security_ctx_t *ctx, uint16_t new_euid);
bool perm_setegid(task_security_ctx_t *ctx, uint16_t new_egid);
bool perm_chown(task_security_ctx_t *ctx, void *resource, uint16_t uid, uint16_t gid);
bool perm_chmod(task_security_ctx_t *ctx, void *resource, uint16_t perms);
```

---

## Part 13: Networking (Pico W)

### 13.1 WiFi Stack

Networking is available on Pico W and Pico 2 W boards via the CYW43439 WiFi chip. The stack uses lwIP (Lightweight IP) in threadsafe background mode.

**Configuration:**

| Constant | Value | Description |
|----------|-------|-------------|
| `NET_MAX_SOCKETS` | 8 | Maximum concurrent sockets |
| `NET_SSID_MAX` | 32 | WiFi SSID length |
| `NET_HOSTNAME_MAX` | 32 | Device hostname length |
| `NET_MAX_SCAN_RESULTS` | 10 | WiFi scan result limit |

### 13.2 WiFi API

```c
int  net_init(void);
int  net_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);
int  net_wifi_disconnect(void);
int  net_wifi_scan(net_scan_result_t *results, int max_results);
int  net_get_info(net_info_t *info);
int  net_set_hostname(const char *hostname);
```

### 13.3 Socket API

```c
int  net_socket_create(net_socket_type_t type);    // TCP or UDP
int  net_socket_close(int sock_id);
int  net_socket_connect(int sock_id, const char *ip, uint16_t port, uint32_t timeout_ms);
int  net_socket_listen(int sock_id, uint16_t port);
int  net_socket_send(int sock_id, const void *data, size_t len);
int  net_socket_recv(int sock_id, void *data, size_t max_len);
int  net_socket_sendto(int sock_id, const char *ip, uint16_t port,
                       const void *data, size_t len);    // UDP
```

### 13.4 Utility API

```c
int  net_dns_lookup(const char *hostname, char *ip);
int  net_ping(const char *ip, uint32_t timeout_ms);      // Returns RTT in ms
int  net_http_get(const char *url, char *response_buf, size_t buf_size);
```

### 13.5 Shell Usage

```bash
net scan                          # Scan for WiFi networks
net connect MySSID MyPassword     # Connect to WiFi
net status                        # Show IP, signal, connection state
net ping 8.8.8.8                  # Ping Google DNS
net dns google.com                # DNS lookup
net http http://example.com       # HTTP GET request
mqtt connect broker.hivemq.com    # Connect MQTT broker
mqtt pub test/topic "Hello IoT"   # Publish message
mqtt sub test/topic               # Subscribe to topic
remote start 23                   # Start remote shell on port 23
```

---

## Part 14: SageLang Integration

### 14.1 Embedded Interpreter

SageLang is a Python-inspired scripting language embedded in littleOS via the `src/sage/` module. It provides runtime GPIO control, timing, configuration, and system queries without reflashing firmware.

**Runtime configuration:**

| Parameter | Value | Description |
|-----------|-------|-------------|
| Interpreter heap | 32 KB | Dedicated heap, bulk-resettable |
| Execution timeout | 5000 ms | Maximum script runtime |
| Heartbeat interval | 250 ms | Watchdog feed during execution |
| GC | Mark-and-sweep | Automatic with manual trigger |

### 14.2 Native Function Bindings

SageLang scripts can call native C functions registered at init:

**GPIO functions:**
```python
gpio_init(pin, direction)     # 0=input, 1=output
gpio_write(pin, value)        # Set pin high/low
gpio_read(pin)                # Read pin state
gpio_toggle(pin)              # Toggle pin
gpio_set_pull(pin, pull)      # 0=none, 1=up, 2=down
```

**System functions:**
```python
sys_version()                 # littleOS version string
sys_uptime()                  # Uptime in milliseconds
sys_temp()                    # CPU temperature (Celsius)
sys_clock()                   # Clock frequency (Hz)
sys_reboot()                  # Reboot system
```

**Timing functions:**
```python
sleep(seconds)                # Delay (seconds)
sleep_us(microseconds)        # Delay (microseconds)
time_ms()                     # Current time (milliseconds)
time_us()                     # Current time (microseconds)
```

**Configuration functions:**
```python
config_set(key, value)        # Store key-value pair
config_get(key)               # Retrieve value
config_has(key)               # Check if key exists
config_remove(key)            # Delete key
config_save()                 # Persist to flash
config_load()                 # Load from flash
```

**Watchdog functions:**
```python
wdt_enable(timeout_ms)        # Enable watchdog
wdt_disable()                 # Disable watchdog
wdt_feed()                    # Reset countdown
wdt_get_timeout()             # Get current timeout
```

### 14.3 Shell Usage

```bash
sage                              # Enter interactive REPL
sage -e "print(sys_version())"    # Evaluate expression
sage -e "gpio_init(25, 1); gpio_write(25, 1)"    # Blink LED

script save blink "gpio_init(25,1); while true: gpio_toggle(25); sleep(0.5)"
script list                       # List saved scripts
script run blink                  # Execute saved script
script autoboot blink             # Run on boot
```

### 14.4 Heartbeat System

During script execution, the SageLang runtime calls `supervisor_heartbeat()` and `wdt_feed()` every 250 ms to prevent the watchdog from triggering on long-running scripts. This is transparent to the script author.

---

## Part 15: Supervisor and Watchdog

### 15.1 Core 1 Supervisor

The supervisor (`src/drivers/supervisor.c`) runs independently on Core 1 and monitors system health every 100 ms:

**Monitored parameters:**

| Check | Warning Threshold | Critical Threshold |
|-------|-------------------|-------------------|
| Memory usage | 80% | — |
| CPU temperature | 70 C | 80 C |
| Watchdog feeds | — | Not fed within timeout |
| Core 0 heartbeat | — | Stalled (no heartbeats) |
| Stack overflow | — | Guard bytes corrupted |

**Health status levels:**

```
HEALTH_OK        = 0    All systems nominal
HEALTH_WARNING   = 1    Non-critical issue detected
HEALTH_CRITICAL  = 2    Immediate attention required
HEALTH_EMERGENCY = 3    System failure imminent
```

**API:**

```c
void              supervisor_init(void);          // Launch on Core 1
void              supervisor_stop(void);
bool              supervisor_is_running(void);
bool              supervisor_get_metrics(supervisor_metrics_t *metrics);
system_health_t   supervisor_get_health(void);
void              supervisor_heartbeat(void);     // Called from Core 0
void              supervisor_report_memory(size_t allocated);
const char       *supervisor_health_string(system_health_t status);
```

### 15.2 Hardware Watchdog

The watchdog timer (`src/drivers/watchdog.c`) provides hardware-level hang recovery:

| Parameter | Value |
|-----------|-------|
| Minimum timeout | 1 ms |
| Maximum timeout | 8388 ms (~8.3 seconds) |
| Default timeout | 5000 ms |
| Boot timeout | 8000 ms (set in kernel_init) |

```c
bool     wdt_init(uint32_t timeout_ms);
bool     wdt_enable(uint32_t timeout_ms);
void     wdt_feed(void);                    // Must call before timeout
void     wdt_disable(void);
bool     wdt_is_enabled(void);
uint32_t wdt_get_time_remaining_ms(void);
void     wdt_reboot(uint32_t delay_ms);     // Force reboot
watchdog_reset_reason_t wdt_get_reset_reason(void);
```

**Reset reasons:** `WATCHDOG_RESET_NONE`, `WATCHDOG_RESET_TIMEOUT`, `WATCHDOG_RESET_FORCED`

The shell, REPL, and SageLang runtime all feed the watchdog automatically. If the system hangs (infinite loop, deadlock), the watchdog triggers a hardware reset after the timeout period.

---

## Part 16: Persistent Storage

### 16.1 Configuration Storage

The config store (`src/storage/config_storage.c`) provides a flash-backed key-value database:

| Constant | Value |
|----------|-------|
| `CONFIG_MAX_KEY_LEN` | 32 chars |
| `CONFIG_MAX_VALUE_LEN` | 128 chars |
| `CONFIG_MAX_ENTRIES` | 16 |
| `CONFIG_AUTOBOOT_SCRIPT_SIZE` | 1024 bytes |

```c
bool           config_init(void);
config_result_t config_set(const char *key, const char *value);
config_result_t config_get(const char *key, char *value, size_t size);
config_result_t config_delete(const char *key);
bool           config_exists(const char *key);
bool           config_save(void);        // Write to flash
bool           config_load(void);        // Read from flash
int            config_count(void);
bool           config_set_autoboot(const char *script);
bool           config_has_autoboot(void);
```

### 16.2 Script Storage

Script storage (`src/storage/script_storage.c`) saves SageLang scripts to flash for persistence across reboots:

```c
void         script_storage_init(void);
bool         script_save(const char *name, const char *code);
const char  *script_load(const char *name);
bool         script_delete(const char *name);
int          script_count(void);
size_t       script_memory_used(void);
bool         script_exists(const char *name);
void         script_clear_all(void);
```

---

## Part 17: Debug and Diagnostics

### 17.1 Structured Logging (logcat)

Tag-based logging with level filters, similar to Android's logcat:

| Constant | Value |
|----------|-------|
| `LOGCAT_MAX_ENTRIES` | 64 |
| `LOGCAT_MAX_MSG_LEN` | 64 chars |

### 17.2 Execution Tracing

Function-level timing with entry/exit timestamps:

| Constant | Value |
|----------|-------|
| `TRACE_MAX_ENTRIES` | 128 |
| `TRACE_MAX_NAME_LEN` | 16 chars |

### 17.3 Memory Watchpoints

Break on read/write to specified address ranges. Useful for debugging memory corruption.

### 17.4 Benchmarks

Built-in performance benchmarks: CPU (integer/float ops), memory (alloc/free throughput), GPIO (toggle rate), filesystem (read/write bandwidth).

### 17.5 Self-Test

Hardware self-test suite covering RAM integrity, flash read/write, GPIO loopback, ADC accuracy, and timer precision.

### 17.6 Crash Recovery

**Coredump**: Stores crash state (registers, stack, PC) in `.uninitialized_data` section that survives soft reboot. View with `coredump` command after restart.

**Syslog**: Persistent system log in `.uninitialized_data` section. Survives soft reboot for post-mortem analysis.

---

## Part 18: Runtime Profiler

### 18.1 Profiler Architecture

The profiler (`src/sys/profiler.c`) tracks function-level execution time and per-task CPU usage.

**Configuration:**

| Constant | Value |
|----------|-------|
| `PROFILER_MAX_SECTIONS` | 16 |
| `PROFILER_NAME_LEN` | 24 chars |
| `PROFILER_HISTORY_LEN` | 64 samples |

### 18.2 Section Profiling

```c
int  profiler_init(void);
int  profiler_section_register(const char *name);   // Returns section_id
void profiler_section_enter(int section_id);
void profiler_section_exit(int section_id);
int  profiler_section_stats(int section_id, profiler_section_t *stats);
```

**Convenience macro:**

```c
PROFILE_SECTION("my_function", {
    // Code to profile
    do_expensive_work();
});
```

Each section tracks: call count, total time, min/max/avg/last execution time, and a rolling history buffer of 64 samples.

### 18.3 Task CPU Tracking

```c
void profiler_task_switch(uint16_t old_task, uint16_t new_task);
int  profiler_get_task_stats(profiler_task_stats_t *stats, int max);
int  profiler_get_system_stats(profiler_system_stats_t *stats);
void profiler_print_report(void);
void profiler_print_tasks(void);
```

System stats include: overall CPU usage, idle time, total context switches, IRQ count, IRQ latency (max and average), and profiler overhead.

---

## Part 19: Virtual Filesystems

### 19.1 procfs (/proc)

The process filesystem provides read-only system information files:

| Path | Content |
|------|---------|
| `/proc/cpuinfo` | CPU model, cores, clock speed, chip revision |
| `/proc/meminfo` | Total/free/used RAM, flash size |
| `/proc/uptime` | System uptime in seconds |
| `/proc/version` | littleOS version and build date |
| `/proc/temperature` | Current CPU temperature |
| `/proc/gpio` | GPIO pin states and functions |
| `/proc/tasks` | Active task list with states |

### 19.2 devfs (/dev)

The device filesystem provides device node access:

| Path | Device |
|------|--------|
| `/dev/gpio` | GPIO pin control |
| `/dev/uart0` | UART0 serial port |
| `/dev/uart1` | UART1 serial port |
| `/dev/i2c0` | I2C bus 0 |
| `/dev/i2c1` | I2C bus 1 |
| `/dev/spi0` | SPI bus 0 |
| `/dev/spi1` | SPI bus 1 |
| `/dev/adc` | ADC channels |
| `/dev/pwm` | PWM outputs |

---

## Part 20: System Services

### 20.1 Cron (Scheduled Tasks)

The cron system (`src/sys/cron.c`) executes shell commands or scripts at regular intervals:

| Constant | Value |
|----------|-------|
| `CRON_MAX_JOBS` | 4 |

```bash
cron add 60 "sensor read temp"    # Run every 60 seconds
cron list                         # Show active jobs
cron remove 0                     # Remove job by ID
```

### 20.2 Environment Variables

Default environment on boot:

| Variable | Default Value |
|----------|---------------|
| `USER` | `root` |
| `HOME` | `/` |
| `HOSTNAME` | `littleos` |
| `PS1` | `\u@\h:\w\$ ` |
| `PATH` | `/bin` |

```bash
env                              # List all variables
export EDITOR=nano               # Set variable
echo $HOSTNAME                   # Expand variable
```

### 20.3 Shell Aliases

```bash
alias ll="ls -la"                # Create alias
alias                            # List all aliases
alias -r ll                      # Remove alias
```

Maximum 8 aliases, recursive expansion up to depth 5.

### 20.4 Terminal Multiplexer (screen)

Split-pane terminal multiplexer:
- Ctrl+A N — Next pane
- Ctrl+A P — Previous pane

### 20.5 Manual Pages

Built-in documentation for all commands:

```bash
man fs         # Filesystem manual
man gpio       # GPIO manual
man net        # Networking manual
```

---

## Part 21: System Information

### 21.1 System Info API

```c
bool  system_get_cpu_info(cpu_info_t *info);       // clock, cores, model, revision
bool  system_get_memory_info(memory_info_t *info); // RAM total/free/used, flash
bool  system_get_uptime(uptime_info_t *info);      // ms, seconds, minutes, hours, days
float system_get_temperature(void);                 // Celsius
bool  system_get_board_id(char *buffer, size_t size);
const char *system_get_version(void);               // "0.7.0"
const char *system_get_build_date(void);
```

### 21.2 littlefetch

The `fetch` command displays a neofetch-style system summary:

```
      ___          OS:    littleOS v0.7.0
     /   \         Host:  Raspberry Pi Pico 2
    | lit |        CPU:   ARM Cortex-M33 (Dual Core)
    | OS  |        Chip:  RP2350
     \___/         RAM:   520 KB SRAM
                   Flash: 4 MB
                   Temp:  32.5 C
                   Up:    0d 1h 23m 45s
```

The display automatically adapts to the detected board and chip using `board_config.h` values.

---

## Part 22: Version History

| Version | Date | Key Features |
|---------|------|-------------|
| 0.7.0 | 2026-03-13 | RP2350 multi-board support, HSTX DVI output, board abstraction, 8 build targets |
| 0.6.0 | 2025-03-13 | Debug suite, hardware tools, networking, shell enhancements, text processing |
| 0.5.0 | 2025-02-15 | Task scheduler, IPC, OTA, DMA, PIO, USB, power management, profiler |
| 0.4.0 | 2025-12-29 | F2FS-inspired filesystem with crash recovery |
| 0.3.0 | 2025-12-02 | Watchdog, script storage, system info, GPIO HAL, config storage, memory management, permissions |
| 0.2.0 | 2024-11-28 | SageLang integration, interactive shell, REPL |
| 0.1.0 | 2024-11-20 | Initial kernel, UART, minimal shell |

---

## External References

- Raspberry Pi Pico SDK: https://github.com/raspberrypi/pico-sdk
- RP2040 Datasheet: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- RP2350 Datasheet: https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf
- Pico C SDK Documentation: https://www.raspberrypi.com/documentation/pico-sdk/
- SageLang Repository: https://github.com/Night-Traders-Dev/SageLang
- Bramble Emulator: https://github.com/Night-Traders-Dev/Bramble
- Adafruit Feather RP2350: https://www.adafruit.com/product/6000
- lwIP: https://savannah.nongnu.org/projects/lwip/
- F2FS Design: https://www.kernel.org/doc/html/latest/filesystems/f2fs.html
- TMDS Encoding: https://en.wikipedia.org/wiki/Transition-minimized_differential_signaling

---

## Closing Notes

littleOS is built to make microcontrollers feel like Unix workstations. Every subsystem — from the filesystem to the permission model to the shell — follows established operating system conventions scaled down to fit in kilobytes rather than gigabytes. The board abstraction layer ensures that the same shell commands, scripts, and drivers work across RP2040 and RP2350 hardware with no source changes. The SageLang integration provides runtime programmability without reflashing, and the dual-core supervisor model ensures the system stays responsive even when user code misbehaves.

The design philosophy is pragmatic: use bump allocators instead of malloc/free, cooperative scheduling instead of preemptive, and compile-time buffer sizing instead of dynamic allocation. These trade-offs keep the implementation simple, predictable, and small enough to fit comfortably in the constrained memory of a microcontroller.
