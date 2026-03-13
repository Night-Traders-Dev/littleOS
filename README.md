# littleOS

**A feature-rich operating system for Raspberry Pi Pico, Pico 2, and Pico W with embedded SageLang scripting, WiFi networking, DVI video output, and 60+ shell commands**

[![Version](https://img.shields.io/badge/version-0.7.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040%20%7C%20RP2350-red.svg)](https://www.raspberrypi.com/documentation/microcontrollers/)
[![Pico W](https://img.shields.io/badge/Pico_W-WiFi-orange.svg)](https://www.raspberrypi.com/documentation/microcontrollers/)

## Overview

littleOS brings a **Unix-like shell environment** to **bare-metal RP2040 and RP2350** microcontrollers with **SageLang scripting**, **WiFi networking** (Pico W / Pico 2 W), **DVI video output** (RP2350 HSTX), and a comprehensive set of hardware and system tools.

### Supported Boards

| Board | Chip | Core | WiFi | HSTX DVI |
|-------|------|------|------|----------|
| Raspberry Pi Pico | RP2040 | ARM Cortex-M0+ | - | - |
| Raspberry Pi Pico W | RP2040 | ARM Cortex-M0+ | CYW43 | - |
| Raspberry Pi Pico 2 | RP2350 | ARM Cortex-M33 | - | Yes |
| Raspberry Pi Pico 2 | RP2350 | RISC-V Hazard3 | - | Yes |
| Raspberry Pi Pico 2 W | RP2350 | ARM Cortex-M33 | CYW43 | Yes |
| Raspberry Pi Pico 2 W | RP2350 | RISC-V Hazard3 | CYW43 | Yes |
| Adafruit Feather RP2350 | RP2350 | ARM Cortex-M33 | - | Yes |
| Adafruit Feather RP2350 | RP2350 | RISC-V Hazard3 | - | Yes |

### Key Features

| Category | Features |
|----------|----------|
| **Shell** | 60+ commands, pipes, aliases, env vars, tab completion, history, man pages |
| **Networking** | WiFi (Pico W / Pico 2 W), TCP/UDP sockets, DNS, MQTT, ping, HTTP, remote shell, OTA |
| **Hardware** | GPIO, I2C, SPI, PWM, ADC, DMA, PIO, NeoPixel, OLED display, DVI output (RP2350) |
| **Filesystem** | F2FS-inspired RAM FS with crash recovery, procfs, devfs |
| **Scripting** | SageLang REPL, flash script storage, auto-boot scripts |
| **System** | Watchdog, multicore supervisor, scheduler, cron, IPC, power management |
| **Debug** | logcat, trace, watchpoints, benchmarks, selftest, coredump, syslog |
| **Security** | Multi-user, capability-based permissions, Unix-style rwx |
| **Multi-Chip** | RP2040 and RP2350 from a single codebase with board auto-detection |

## Quick Start

### Prerequisites
```bash
# Ubuntu/Debian (ARM targets)
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# For RISC-V targets (RP2350 Hazard3), also install:
# sudo apt install gcc-riscv64-unknown-elf

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

# Interactive build (select board from menu)
./build.sh

# Or build a specific board directly:
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build

# Raspberry Pi Pico (RP2040, default)
cmake -DLITTLEOS_BOARD=pico ..

# Raspberry Pi Pico W (RP2040 + WiFi)
cmake -DLITTLEOS_BOARD=pico_w ..

# Raspberry Pi Pico 2 (RP2350, ARM Cortex-M33)
cmake -DLITTLEOS_BOARD=pico2 ..

# Raspberry Pi Pico 2 (RP2350, RISC-V Hazard3)
cmake -DLITTLEOS_BOARD=pico2_riscv ..

# Raspberry Pi Pico 2 W (RP2350 + WiFi, ARM)
cmake -DLITTLEOS_BOARD=pico2_w ..

# Adafruit Feather RP2350 (ARM Cortex-M33)
cmake -DLITTLEOS_BOARD=adafruit_feather_rp2350 ..

make -j$(nproc)

# Flash (hold BOOTSEL button, connect USB)
cp littleos.uf2 /media/$USER/RPI-RP2/

# Connect via UART
screen /dev/ttyACM0 115200
```

### Batch Build
```bash
./build.sh --rp2040-all     # Build all RP2040 boards (Pico, Pico W)
./build.sh --rp2350-all     # Build all RP2350 boards (Pico 2, Pico 2 W, Feather)
./build.sh --all            # Build everything
```

### First Session
```bash
Welcome to littleOS Shell!
Type 'help' for available commands

> version
littleOS v0.7.0 - RP2350
With SageLang v0.8.0
Supervisor: Active

> fetch
# System info display (neofetch-style)

> pinout
# Visual GPIO pin diagram (board-specific)

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
pinout            # GPIO pin visualizer (board-specific layout)
i2cscan           # I2C bus scanner
wire              # Interactive I2C/SPI REPL
pwmtune           # PWM frequency/duty tuner
adc               # ADC read/stream/stats
gpiowatch         # GPIO state monitor
neopixel          # WS2812 NeoPixel LED control
display           # SSD1306 OLED display
dvi               # DVI display via HSTX (RP2350 only)
```

### Networking (Pico W / Pico 2 W)
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

## DVI Video Output (RP2350)

RP2350-based boards with HSTX (High-Speed Serial Transmit) support DVI video output on GPIO 12-19:

```bash
> dvi init 320x240      # Initialize 320x240 mode (pixel-doubled)
> dvi test              # Show test pattern (color bars)
> dvi fill 0xE0         # Fill with red (RGB332)
> dvi start             # Start DVI output
> dvi status            # Show resolution, frame count
> dvi stop              # Stop output
```

Supported modes: 640x480@60Hz, 320x240@60Hz (pixel-doubled). Pixel formats: RGB332 (8-bit), RGB565 (16-bit).

## Pico W / Pico 2 W Networking

Build with a WiFi-enabled board (`pico_w`, `pico2_w`, `pico2_w_riscv`) to enable networking:

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

**lwIP stack** with conservative memory settings optimized for RP2040/RP2350:
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
│   │   ├── dmesg.c               # Kernel message ring buffer
│   │   ├── ipc.c                 # Inter-process communication
│   │   └── memory_segmented.c    # Segmented memory allocator
│   ├── drivers/
│   │   ├── uart.c                # UART driver
│   │   ├── watchdog.c            # Watchdog timer
│   │   ├── supervisor.c          # Core 1 health monitor
│   │   ├── scheduler.c           # Task scheduler
│   │   ├── net.c                 # WiFi/TCP/UDP/DNS networking
│   │   ├── mqtt.c                # MQTT client
│   │   ├── remote_shell.c        # TCP remote shell
│   │   ├── ota.c                 # Over-the-air updates
│   │   ├── sensor.c              # Sensor framework
│   │   ├── display.c             # SSD1306 OLED driver
│   │   ├── neopixel.c            # WS2812 NeoPixel driver
│   │   └── fs/                   # F2FS-style filesystem
│   ├── hal/
│   │   ├── gpio.c                # GPIO hardware abstraction
│   │   ├── adc.c                 # ADC (analog-to-digital)
│   │   ├── dma.c                 # DMA engine
│   │   ├── pio.c                 # PIO state machines
│   │   ├── i2c.c                 # I2C bus
│   │   ├── spi.c                 # SPI bus
│   │   ├── pwm.c                 # PWM output
│   │   ├── flash.c               # Flash memory
│   │   ├── power.c               # Power management
│   │   ├── usb_device.c          # USB device support
│   │   └── hstx_dvi.c            # HSTX DVI output (RP2350)
│   ├── sys/
│   │   ├── system_info.c         # System monitoring
│   │   ├── permissions.c         # Capability-based permissions
│   │   ├── littlefetch.c         # System info display
│   │   ├── profiler.c            # Runtime profiler
│   │   ├── shell_env.c           # Environment variables + aliases
│   │   ├── procfs.c              # Process filesystem
│   │   ├── devfs.c               # Device filesystem
│   │   └── ...                   # logcat, trace, cron, syslog, etc.
│   ├── sage/
│   │   ├── sage_embed.c          # SageLang runtime glue
│   │   ├── sage_gpio.c           # GPIO bindings
│   │   ├── sage_system.c         # System bindings
│   │   └── ...                   # time, config, watchdog bindings
│   └── shell/
│       ├── shell.c               # Shell main loop + dispatch
│       └── cmd_*.c               # 50+ command handlers
├── include/
│   ├── board/
│   │   ├── board_config.h        # Board/chip auto-detection
│   │   ├── pinout_pico.h         # Pico 40-pin layout
│   │   └── pinout_feather_rp2350.h  # Feather pinout
│   └── hal/
│       ├── hstx_dvi.h            # HSTX DVI driver API
│       └── ...                   # gpio, dma, pio, adc, power, etc.
├── third_party/
│   └── sagelang/                 # SageLang submodule
├── docs/                         # Documentation
├── CMakeLists.txt                # Build configuration
├── build.sh                      # Interactive build script
└── README.md
```

## Board Abstraction

littleOS uses a single codebase for all supported boards. The Pico SDK provides chip-specific constants via `hardware/platform_defs.h`, and littleOS adds board metadata via `include/board/board_config.h`:

| Macro | RP2040 | RP2350 |
|-------|--------|--------|
| `CHIP_MODEL_STR` | "RP2040" | "RP2350" |
| `CHIP_RAM_SIZE` | 264 KB | 520 KB |
| `CHIP_CORE_STR` | "ARM Cortex-M0+" | "ARM Cortex-M33" or "Hazard3 RISC-V" |
| `NUM_DMA_CHANNELS` | 12 | 16 |
| `NUM_PIOS` | 2 | 3 |
| `NUM_BANK0_GPIOS` | 30 | 48 |

## Memory Layout

```text
RP2040 (Pico / Pico W):
  SRAM:  264 KB (0x20000000 - 0x20042000)
  Flash: 2 MB
  Free RAM: ~121-151 KB (depending on WiFi)

RP2350 (Pico 2 / Pico 2 W / Feather):
  SRAM:  520 KB (0x20000000 - 0x20082000)
  Flash: 2-8 MB (board-dependent)
  Free RAM: ~350+ KB
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
      -> net_init (WiFi boards only)
      -> MQTT, pkg, tmux
      -> logcat, trace, coredump, syslog
      -> Watchdog enable (8s)
      -> Supervisor launch (Core 1)
      -> shell_run()
```

## Hardware Requirements

- **Board:** Any supported board (see table above)
- **Connection:** UART serial (GPIO 0/1) at 115200 baud
- **Optional:** I2C devices, SPI devices, NeoPixel LEDs, SSD1306 OLED, DVI monitor (RP2350)

## Documentation

- **[CHANGELOG.md](CHANGELOG.md)** - Version history
- **[docs/littleOS_Guide.md](docs/littleOS_Guide.md)** - Comprehensive system guide
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
| RP2350 RISC-V build fails | Install `gcc-riscv64-unknown-elf` toolchain |

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Resources

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)
- [Raspberry Pi Pico Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)

## License

MIT License - see [LICENSE](LICENSE) for details.

---

**Built for embedded education | Powered by SageLang and littleOS Core | v0.7.0**
