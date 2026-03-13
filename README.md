# littleOS for RP2040

**A feature-rich operating system for Raspberry Pi Pico (and Pico W) with embedded SageLang scripting, WiFi networking, and 60+ shell commands**

[![Version](https://img.shields.io/badge/version-0.6.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040-red.svg)](https://www.raspberrypi.com/documentation/microcontrollers/)
[![Pico W](https://img.shields.io/badge/Pico_W-WiFi-orange.svg)](https://www.raspberrypi.com/documentation/microcontrollers/)

## Overview

littleOS brings a **Unix-like shell environment** to **bare-metal RP2040** with **SageLang scripting**, **WiFi networking** (Pico W), and a comprehensive set of hardware and system tools.

### Key Features

| Category | Features |
|----------|----------|
| **Shell** | 60+ commands, pipes, aliases, env vars, tab completion, history, man pages |
| **Networking** | WiFi (Pico W), TCP/UDP sockets, DNS, MQTT, ping, HTTP, remote shell, OTA |
| **Hardware** | GPIO, I2C, SPI, PWM, ADC, DMA, PIO, NeoPixel, OLED display |
| **Filesystem** | F2FS-inspired RAM FS with crash recovery, procfs, devfs |
| **Scripting** | SageLang REPL, flash script storage, auto-boot scripts |
| **System** | Watchdog, multicore supervisor, scheduler, cron, IPC, power management |
| **Debug** | logcat, trace, watchpoints, benchmarks, selftest, coredump, syslog |
| **Security** | Multi-user, capability-based permissions, Unix-style rwx |

## Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# macOS
brew install cmake arm-none-eabi-gcc

# Arch Linux
sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib
```

### Build & Flash
```bash
# Clone with SageLang
git clone --recursive https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS

# Standard Pico build
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)

# Pico W build (with WiFi)
mkdir build_w && cd build_w
cmake -DLITTLEOS_PICO_W=ON ..
make -j$(nproc)

# Flash (hold BOOTSEL button, connect USB)
cp littleos.uf2 /media/$USER/RPI-RP2/

# Connect via UART
screen /dev/ttyACM0 115200
```

### First Session
```bash
Welcome to littleOS Shell!
Type 'help' for available commands

> version
littleOS v0.6.0 - RP2040
With SageLang v0.8.0
Supervisor: Active

> fetch
# System info display (neofetch-style)

> pinout
# Visual GPIO pin diagram

> man memory
# Built-in manual page
```

## Shell Commands

littleOS provides 60+ commands organized by category. Use `man <cmd>` for detailed help on any command.

### System
```bash
help              # Show all commands (grouped by category)
version           # OS and SageLang version
clear             # Clear screen
reboot            # Software reboot (via watchdog)
history           # Command history
health            # Quick system health check
stats             # Detailed system statistics
supervisor        # Core 1 supervisor (start|stop|status|alerts)
dmesg             # Kernel message ring buffer (-f follow, -l level, -c clear)
fetch             # System info display (neofetch-style)
top               # Live system monitor (htop-style)
```

### Processes & Memory
```bash
tasks             # Task scheduler management
memory            # Heap diagnostics (stats|leaks|test|defrag|threshold)
profile           # Runtime profiling
```

### Users & Security
```bash
users             # User account management (list|get|exists)
perms             # Permission utilities (check|decode|presets)
```

### Filesystem & Text
```bash
fs                # F2FS-style filesystem (init|mount|mkdir|touch|cat|write|ls|sync|info|fsck)
cat <file>        # Display file contents
echo <text>       # Print text (supports > redirect)
head <file>       # Show first lines
tail <file>       # Show last lines
wc <file>         # Word/line/byte count
grep <pat> <file> # Search patterns in files
hexdump <file>    # Hex dump
tee <file>        # Duplicate output to file
```

### Virtual Filesystems
```bash
proc              # Process filesystem (/proc)
dev               # Device filesystem (/dev)
```

### Hardware
```bash
hw                # Hardware peripherals (I2C/SPI/PWM/ADC)
pio               # PIO programmable I/O
dma               # DMA engine control
usb               # USB device mode (CDC/HID/MSC)
pinout            # GPIO pin visualizer
i2cscan           # I2C bus scanner
wire              # Interactive I2C/SPI REPL
pwmtune           # PWM frequency/duty tuner
adc               # ADC read/stream/stats
gpiowatch         # GPIO state monitor
neopixel          # WS2812 NeoPixel LED control
display           # SSD1306 OLED display
```

### Networking (Pico W)
```bash
net               # WiFi/TCP/UDP (scan|connect|status|ping|http|socket)
mqtt              # MQTT IoT client (connect|pub|sub)
remote            # Remote shell over TCP
ota               # Over-the-air firmware updates
```

### Scripting & Packages
```bash
sage              # SageLang REPL / inline execution (-e "code")
script            # Flash script storage (save|list|run|delete|autoboot)
pkg               # Package manager
```

### Services
```bash
sensor            # Sensor framework and logging
power             # Power management and sleep modes
cron              # Scheduled tasks (add|list|remove)
ipc               # Inter-process communication channels
```

### Debug & Diagnostics
```bash
logcat            # Structured logging with tag/level filters
trace             # Execution trace buffer
watchpoint        # Memory watchpoints (break on read/write)
benchmark         # Performance benchmarks (cpu|mem|gpio|fs)
selftest          # Hardware self-test suite
coredump          # Crash dump viewer
syslog            # Persistent system log (survives reboot)
```

### Shell Features
```bash
env               # View/set environment variables
alias             # Command aliases (alias ll="ls -la")
export            # Set environment variable
screen            # Terminal multiplexer (split panes)
man               # Manual pages for all commands
```

**Shell capabilities:** pipes (`cmd1 | cmd2`), output redirect (`echo hello > file`), environment variable expansion (`$VAR`), alias expansion, `!!` to repeat last command, tab completion, UP/DOWN arrow history.

## Pico W Networking

Build with `-DLITTLEOS_PICO_W=ON` to enable WiFi networking:

```bash
> net scan                     # Scan for WiFi networks
> net connect MySSID password  # Connect to WiFi
> net status                   # Show IP address
> net ping 8.8.8.8             # ICMP ping
> net http google.com /        # HTTP GET request
> mqtt connect broker.io 1883  # MQTT broker
> mqtt pub topic "hello"       # Publish message
> remote start 2323            # Start TCP shell server
```

**lwIP stack** with conservative memory settings optimized for RP2040:
- TCP, UDP, DHCP, DNS, ICMP, ARP
- 4KB memory pool, 8 packet buffers
- Background processing via `pico_cyw43_arch_lwip_threadsafe_background`

## SageLang Scripting

### Interactive REPL
```bash
> sage
sage> let x = 42
sage> print x * 2
84
sage> exit

> sage -e "print 2 + 2"
4
```

### Language Features
```sagelang
# Variables, functions, classes, generators
let items = [1, 2, 3]
let data = {"key": "value"}

proc fibonacci(n):
    let a = 0, b = 1
    for i in range(n):
        yield a
        let temp = a + b
        a = b
        b = temp

class Sensor:
    proc init(self, pin):
        self.pin = pin
        gpio_init(pin, false)
    proc read(self):
        return gpio_read(self.pin)
```

### Native Functions

| Category | Functions |
|----------|-----------|
| **GPIO** | `gpio_init`, `gpio_write`, `gpio_read`, `gpio_toggle`, `gpio_set_pull` |
| **System** | `sys_version`, `sys_uptime`, `sys_temp`, `sys_clock`, `sys_free_ram`, `sys_total_ram`, `sys_board_id`, `sys_info`, `sys_print` |
| **Timing** | `sleep`, `sleep_us`, `time_ms`, `time_us` |
| **Config** | `config_set`, `config_get`, `config_has`, `config_remove`, `config_save`, `config_load` |
| **Watchdog** | `wdt_enable`, `wdt_disable`, `wdt_feed`, `wdt_get_timeout` |

### Script Storage
```bash
> script save blink       # Save script to flash
> script list             # List stored scripts
> script run blink        # Execute script
> script autoboot blink   # Run on boot
> script noboot           # Disable auto-boot
```

## Project Structure

```text
littleOS/
├── boot/
│   └── boot.c                    # System entry point
├── src/
│   ├── kernel/
│   │   ├── kernel.c              # Kernel main + boot sequence
│   │   └── dmesg.c               # Kernel message ring buffer
│   ├── drivers/
│   │   ├── uart.c                # UART driver
│   │   ├── watchdog.c            # Watchdog timer
│   │   ├── supervisor.c          # Core 1 health monitor
│   │   ├── scheduler.c           # Task scheduler
│   │   ├── multicore.c           # Multicore support
│   │   ├── net.c                 # WiFi/TCP/UDP/DNS networking
│   │   ├── mqtt.c                # MQTT client
│   │   ├── remote_shell.c        # TCP remote shell
│   │   ├── ota.c                 # Over-the-air updates
│   │   ├── sensor.c              # Sensor framework
│   │   ├── sensor_validation.c   # Sensor data validation
│   │   ├── display.c             # SSD1306 OLED driver
│   │   ├── neopixel.c            # WS2812 NeoPixel driver
│   │   └── fs/                   # F2FS-style filesystem
│   │       ├── fs_core.c         # Superblock, checkpoints, NAT/SIT
│   │       ├── fs_inode.c        # Inode I/O + block mapping
│   │       ├── fs_dir.c          # Directory operations
│   │       └── fs_file.c         # File read/write/seek
│   ├── sys/
│   │   ├── system_info.c         # System monitoring
│   │   ├── permissions.c         # Capability-based permissions
│   │   ├── littlefetch.c         # System info display
│   │   ├── profiler.c            # Runtime profiler
│   │   ├── shell_env.c           # Environment variables + aliases
│   │   ├── procfs.c              # Process filesystem
│   │   ├── devfs.c               # Device filesystem
│   │   ├── pkg.c                 # Package manager
│   │   ├── cron.c                # Cron scheduler
│   │   ├── tmux.c                # Terminal multiplexer
│   │   ├── logcat.c              # Structured logging
│   │   ├── trace.c               # Execution tracing
│   │   ├── watchpoint.c          # Memory watchpoints
│   │   ├── coredump.c            # Crash dump storage
│   │   └── syslog.c              # Persistent system log
│   ├── storage/
│   │   ├── config_storage.c      # Key-value flash storage
│   │   └── script_storage.c      # Script flash storage
│   ├── hal/
│   │   └── gpio.c                # GPIO hardware abstraction
│   ├── sage/
│   │   ├── sage_embed.c          # SageLang runtime glue
│   │   ├── sage_gpio.c           # GPIO bindings
│   │   ├── sage_system.c         # System bindings
│   │   ├── sage_time.c           # Timing bindings
│   │   ├── sage_config.c         # Config bindings
│   │   ├── sage_watchdog.c       # Watchdog bindings
│   │   └── sage_multicore.c      # Multicore support
│   └── shell/
│       ├── shell.c               # Shell main loop + dispatch
│       └── cmd_*.c               # 45 command handlers
├── include/                      # Public headers
├── third_party/
│   └── sagelang/                 # SageLang submodule (src/c/ + src/sage/)
├── docs/                         # Documentation
├── CMakeLists.txt                # Build configuration
├── build.sh                      # Interactive build script
└── README.md
```

## Memory Layout

```text
Standard Pico Build:
  Text (code+rodata):  ~361 KB flash
  BSS (globals):       ~105 KB RAM
  Free RAM:            ~151 KB

Pico W Build (WiFi):
  Text (code+rodata):  ~679 KB flash
  BSS (globals):       ~135 KB RAM
  Free RAM:            ~121 KB

Flash (2 MB):
  littleOS Binary:     361-679 KB
  Script Storage:      4 KB
  Configuration:       4 KB
  Free Space:          ~1.3-1.6 MB
```

## Boot Sequence

```
Pico SDK crt0.S (clocks, memory)
  -> boot.c main() (stdio init)
    -> kernel_main()
      -> memory_init, dmesg_init
      -> watchdog reset check
      -> UART, scheduler, config, users
      -> SageLang init + autoboot script
      -> Permissions, IPC, OTA, DMA, power
      -> Sensor, profiler, shell env
      -> procfs, devfs, cron
      -> net_init (Pico W only)
      -> MQTT, pkg, tmux
      -> logcat, trace, coredump, syslog
      -> Watchdog enable (8s)
      -> Supervisor launch (Core 1)
      -> shell_run()
```

## Hardware Requirements

- **Board:** Raspberry Pi Pico or Pico W (any RP2040-based board)
- **Connection:** UART serial (GPIO 0/1) at 115200 baud
- **Optional:** I2C devices, SPI devices, NeoPixel LEDs, SSD1306 OLED

## Documentation

- **[CHANGELOG.md](CHANGELOG.md)** - Version history
- **[docs/QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Command cheat sheet
- **[docs/BOOT_SEQUENCE.md](docs/BOOT_SEQUENCE.md)** - Boot process
- **[docs/SHELL_FEATURES.md](docs/SHELL_FEATURES.md)** - Shell capabilities
- **[docs/SAGELANG_INTEGRATION.md](docs/SAGELANG_INTEGRATION.md)** - SageLang embedding
- **[docs/GPIO_INTEGRATION.md](docs/GPIO_INTEGRATION.md)** - Hardware control
- **[docs/SYSTEM_INFO.md](docs/SYSTEM_INFO.md)** - System monitoring
- **[docs/SCRIPT_STORAGE.md](docs/SCRIPT_STORAGE.md)** - Flash storage
- **[docs/SUPERVISOR.md](docs/SUPERVISOR.md)** - Core 1 supervisor
- **[docs/MULTICORE.md](docs/MULTICORE.md)** - Multicore support
- **[docs/DMESG.md](docs/DMESG.md)** - Kernel messages

## Troubleshooting

| Problem | Solution |
|---------|----------|
| System won't boot | Hold BOOTSEL, reconnect USB, re-flash UF2 |
| Watchdog resets | Add `sleep(10)` in tight loops, check `dmesg` |
| No serial output | Verify UART connection (not USB), 115200 baud |
| FS errors | Run `fs fsck`, or `fs init <blocks>` to reinitialize |
| Memory issues | Check `memory stats`, run `memory defrag` |
| WiFi won't connect | Verify SSID/password, check `net status` |
| Script errors | Use `script show <name>`, check `memory stats` |

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Resources

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)
- [Raspberry Pi Pico Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)

## License

MIT License - see [LICENSE](LICENSE) for details.

---

**Built for embedded education | Powered by SageLang and littleOS Core | v0.6.0**
