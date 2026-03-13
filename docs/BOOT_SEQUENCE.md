# littleOS Boot Sequence

## Boot Flow

```text
Pico SDK Boot (crt0.S)
│  Initialize clocks, memory, BSS
│
▼
boot/boot.c - main()
│  stdio_init_all()
│  sleep_ms(1000)
│
▼
kernel_main() - src/kernel/kernel.c
│
├── Phase 1: Core Init
│   ├── memory_init()          # Heap allocator
│   ├── dmesg_init()           # Kernel message buffer
│   ├── Watchdog reset check   # Detect crash recovery
│   ├── littleos_uart_init()   # UART with permissions
│   ├── scheduler_init()       # Task scheduler
│   ├── config_init()          # Flash key-value store
│   └── users_init()           # User database + display
│
├── Phase 2: SageLang
│   ├── sage_init()            # Interpreter + native functions
│   ├── script_storage_init()  # Flash script storage
│   └── autoboot script        # Execute if configured
│
├── Phase 3: Security
│   ├── Root security context  # UID=0, GID=0
│   └── init_device_permissions()
│       ├── UART0:    root/drivers  0660
│       ├── Watchdog: root/system   0640
│       ├── Scheduler: root/system  0660
│       ├── Memory:   root/system   0600
│       ├── Config:   root/system   0640
│       ├── SageLang: root/users    0755
│       ├── Scripts:  root/users    0770
│       └── Supervisor: root/system 0600
│
├── Phase 4: Subsystems
│   ├── ipc_init()             # Inter-process communication
│   ├── memory_accounting_init()
│   ├── ota_init()             # Over-the-air updates
│   ├── dma_hal_init()         # DMA engine
│   ├── power_init()           # Power management
│   ├── sensor_init()          # Sensor framework
│   ├── profiler_init()        # Runtime profiler
│   └── shell_env_init()       # Env vars, aliases, prompt
│
├── Phase 5: Virtual FS & Services
│   ├── procfs_init()          # /proc filesystem
│   ├── devfs_init()           # /dev filesystem
│   ├── cron_init()            # Scheduled tasks
│   ├── net_init()             # WiFi (Pico W only)
│   ├── mqtt_init()            # MQTT client
│   ├── pkg_init()             # Package manager
│   └── tmux_init()            # Terminal multiplexer
│
├── Phase 6: Debug Subsystems
│   ├── logcat_init()          # Structured logging
│   ├── trace_init()           # Execution tracing
│   ├── coredump_init()        # Crash dumps
│   └── syslog_init()          # Persistent log
│
├── Phase 7: Watchdog & Supervisor
│   ├── wdt_enable(8000)       # 8 second timeout
│   └── supervisor_init()      # Core 1 health monitoring
│
▼
shell_run() - src/shell/shell.c
   Main command loop
```

## Boot Messages

### Initialization Output

```text
========================================
  RP2040 littleOS Kernel
  Built: Mar 13 2026 14:30:00
========================================

Initializing task scheduler...
Initializing user database...

=== User Account Configuration ===
Total accounts: 2
[0] root
    UID: 0, GID: 0, Capabilities: ALL
[1] appuser
    UID: 1000, GID: 1000, Capabilities: NONE
==================================

Initializing SageLang interpreter...
  SageLang ready
  Script storage initialized

Setting up device and subsystem permissions...
  UART0:      owner=root, group=drivers, mode=0660
  Watchdog:   owner=root, group=system, mode=0640
  ...

Watchdog: Active (8s timeout)
Supervisor: Core 1 monitoring system health

Boot sequence complete. Starting shell in 2 seconds...
```

### Crash Recovery

If the system was reset by watchdog:

```text
*** RECOVERED FROM CRASH ***
System was reset by watchdog timer
```

### Welcome Screen

After boot delay, screen clears and shows:

```text
========================================
  Welcome to littleOS Shell!
========================================
Type 'help' for available commands

Running as: root (UID=0, GID=0)
```

## Boot Timing

| Phase | Duration | Description |
| ----- | -------- | ----------- |
| SDK Boot | ~100ms | Hardware init |
| stdio + delay | ~1000ms | USB/UART enumeration |
| Core init | ~50ms | Memory, scheduler, users |
| SageLang init | ~50ms | Interpreter + native functions |
| Subsystem init | ~100ms | IPC, OTA, DMA, sensors, etc. |
| Boot message display | 2000ms | Show init status |
| **Total** | **~3.3s** | Power-on to shell prompt |

## Customization

### Change Boot Delay

In `src/kernel/kernel.c`:

```c
sleep_ms(2000);  // Change to desired milliseconds
```

### Disable Screen Clear

Comment out in `src/kernel/kernel.c`:

```c
// printf("\033[2J\033[H");
```

### Pico W Networking

WiFi init only runs when built with `-DLITTLEOS_PICO_W=ON`:

```c
#ifdef PICO_W
    net_init();
#endif
```

## Reboot

The `reboot` command uses the hardware watchdog:

```c
watchdog_enable(1, 1);  // 1ms timeout
while(1) {}             // Watchdog fires, full reset
```

This is a hardware reset - all peripherals, GPIO states, and memory are reset. Data in `.uninitialized_data` section survives (coredump, syslog).

---

**Version**: 0.6.0
**Last Updated**: March 2026
