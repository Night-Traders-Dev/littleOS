# littleOS for RP2040

**A minimal, crash-resistant operating system for Raspberry Pi Pico with embedded SageLang scripting**

[![Version](https://img.shields.io/badge/version-0.3.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040-red.svg)](https://www.raspberrypi.com/documentation/microcontrollers/)

## Overview

littleOS is an educational operating system that brings high-level scripting to bare metal RP2040. Write interactive hardware control programs in SageLang—a clean, indentation-based language with classes, generators, and automatic memory management.

### Key Features

- 🛡️ **Watchdog Protection** – Automatic recovery from hangs and crashes (8s timeout)
- 🎯 **Hardware Integration** – Direct GPIO, timer, and sensor access from scripts
- 💾 **Persistent Storage** – Save scripts and configurations to flash
- 🔄 **Auto-boot Scripts** – Run programs automatically on startup
- 📟 **System Monitoring** – Real-time temperature, memory, and uptime tracking
- 🧠 **Memory Management Core** – Centralized heap tracking and diagnostics for the OS and SageLang runtime
- 👤 **User & Permission System** – Multi-user support with capability-based access control
- 🚀 **Interactive REPL** – Live coding with immediate feedback
- 🎓 **Educational** – Clear architecture perfect for learning embedded systems

> **In Development:** A multi-core task scheduler is under active development. The memory management system is currently the priority while the scheduler is temporarily disabled.

## Quick Start

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# macOS
brew install cmake
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc

# Arch Linux
sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib
```

### Build & Flash

```bash
# Clone with SageLang
git clone --recursive https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS

# Build with default settings (root-only)
export PICO_SDK_PATH=/path/to/pico-sdk
./build.sh

# OR: Build with interactive user account setup
./build.sh

# Flash (hold BOOTSEL button, connect USB)
cp littleos.uf2 /media/$USER/RPI-RP2/

# Connect
screen /dev/ttyACM0 115200
```

### First Commands

```text
Welcome to littleOS Shell!
Type 'help' for available commands

> help
Available commands:
  help              - Show this help message
  version           - Show OS version
  clear             - Clear the screen
  reboot            - Reboot the system
  history           - Show command history
  health            - Quick system health check
  stats             - Detailed system statistics
  supervisor        - Supervisor control
  dmesg             - View kernel message buffer
  sage              - SageLang interpreter
  script            - Script management
  users             - User account management
  perms             - Permission and access control

> version
littleOS v0.3.0 - RP2040
With SageLang v0.8.0
Supervisor: Active

> sage
sage> print "Hello, RP2040!"
Hello, RP2040!
sage> let temp = sys_temp()
sage> print "CPU: " + str(temp) + "°C"
CPU: 28.5°C
sage> exit

> 
```

## System Features

### Watchdog Timer

**Automatic crash recovery** – System automatically reboots after 8 seconds of inactivity:

```sagelang
# This will trigger watchdog reset
while(true) {}  # System reboots after 8s
```

On recovery, you'll see:

```text
*** RECOVERED FROM CRASH ***
System was reset by watchdog timer
```

Protection is active in:

- Shell command loop
- SageLang REPL
- Script execution
- All core system operations

### User & Permission System

littleOS includes a **capability-based permission system** with built-in support for multiple users:

**Default configuration:**
- Root user (UID 0) with all capabilities
- Optional non-root user (configurable at build time)

**Build with custom user:**

```bash
./build.sh

# Interactive prompts:
# Enable non-root user account? [Y/n] y
# Username [appuser]: myapp
# UID [1000]: 1000
# Capability bitmask [0]: 0
```

**Shell commands:**

```bash
# List all users
> users list

# Get user info
> users get appuser
> users get 1000

# Check if user exists
> users exists appuser

# Show permission details
> perms decode 0755
Permission Mode: 0755
Rwx:    rwxr-xr-x

Owner: rwx (7)
Group: r-x (5)
Other: r-x (5)

# Check permission for a UID
> perms check 1000 0644 read
Permission Check:
  UID:    1000
  Mode:   0644 (rw-r--r--)
  Action: read
  Result: ALLOWED

# Show common presets
> perms presets
```

**Security context:**

Each task or script execution gets a security context:

```c
typedef struct {
    uid_t uid;           // User ID
    gid_t gid;           // Group ID
    uint32_t capabilities;  // Capability bitmask
    mode_t umask;        // File creation mask
} task_sec_ctx_t;
```

### Hardware Access

**GPIO Control:**

```sagelang
# Blink LED on GPIO 25
gpio_init(25, true)
while(true):
    gpio_toggle(25)
    sleep(500)
```

**System Monitoring:**

```sagelang
# Real-time system dashboard
while(true):
    let info = sys_info()
    print "Temp: " + str(info["temperature"]) + "°C"
    print "Free RAM: " + str(info["free_ram"]) + " KB"
    print "Uptime: " + str(sys_uptime()) + "s"
    sleep(5000)
```

**Configuration Storage:**

```sagelang
# Persistent key-value storage
config_set("device_name", "my_pico")
let name = config_get("device_name")
config_save()  # Write to flash
```

### Memory Management System

littleOS includes a **central memory management core** that provides:

**Heap Allocator:**
- 32 KB heap (configurable in `include/memory.h`)
- Linked-list based allocation with coalescing
- Guard bytes detect corruption
- Fragmentation tracking

**Diagnostics via Shell:**

```bash
# Show memory statistics
> memory stats
=== Memory Statistics ===
Total Heap:      32768 bytes
Current Usage:   2048 bytes (6.3%)
Available:       30720 bytes (93.7%)
Peak Usage:      4096 bytes (12.5%)

Allocation Stats:
Total Allocated: 8192 bytes
Total Freed:     6144 bytes
Num Allocations: 32
Num Frees:       24
Fragmentation:   12%

Heap Usage: [===                                      ] 6%

# Show available memory
> memory available
Memory Status:
  Available: 30720 bytes
  In Use:    2048 bytes
  Total:     32768 bytes

# Check for memory leaks
> memory leaks
Checking for memory leaks...
No suspected leaks found

# Test allocations
> memory test 256 10
Test: Allocating 10 blocks of 256 bytes each
  Allocated: 10/10 blocks
  Memory used: 2816 bytes
Freeing allocated blocks...
  Memory used after free: 0 bytes
Test complete

# Defragment heap
> memory defrag
Defragmenting memory...
Fragmentation improvement: 5%

# Set memory warning threshold
> memory threshold 85
Memory warning threshold set to 85%
```

**Planned Features (In Development):**
- Per-task memory accounting
- Memory pressure integration with supervisor
- Automatic leak detection and reporting

### Script Storage

Save scripts to flash memory:

```bash
> storage save blink
# Paste or type script, end with Ctrl+D
gpio_init(25, true)
while(true):
    gpio_toggle(25)
    sleep(500)
^D

> storage list
Scripts:
  0: blink (72 bytes)

> storage run blink
# LED starts blinking

> storage show blink
gpio_init(25, true)
while(true):
    gpio_toggle(25)
    sleep(500)

> storage delete blink
> storage autoboot blink
# Will run on next boot

> storage noboot
# Disable auto-boot
```

### Supervisor & System Health

Core 1 runs a **supervisor** that monitors system health:

```bash
> supervisor status
Supervisor: Active (Core 1)

> health
System Health Report
Status: OK
Uptime: 45230 ms
Memory Used: 2048 bytes (6.3%)
Peak: 4096 bytes (12.5%)
Allocs: 32
Frees: 24
Temperature: 28.5°C (Peak: 32.1°C)
Watchdog: Fed 150 ms ago
Core 0: Responsive (heartbeat 10 ms ago)
Events: Warnings: 0, Critical: 0, Recoveries: 0

> stats
System Statistics (Detailed)
[Extended output with full metrics]
```

### Kernel Message Buffer

Real-time kernel logging via `dmesg`:

```bash
# View all kernel messages
> dmesg

# Follow kernel messages (tail -f style)
> dmesg -f

# Show only warnings and above
> dmesg -l warning

# Clear buffer
> dmesg -c

# Get statistics
> dmesg -s
```

Output example:

```text
[00001] INFO: RP2040 littleOS kernel starting
[00002] INFO: Watchdog timer initialized (8s timeout)
[00003] INFO: Configuration storage initialized
[00004] INFO: User database initialized
[00005] INFO: Root security context created
[00006] INFO: UART0 device permissions configured (rw-rw----)
[00007] INFO: Memory management initialized
[00008] INFO: SageLang interpreter initialized
[00009] INFO: Script storage system initialized
[00010] INFO: Watchdog enabled - monitoring for system hangs
[00011] INFO: Supervisor launched on Core 1
[00012] INFO: Boot sequence complete - entering shell
```

## SageLang Reference

### Core Language

**Variables & Types:**

```sagelang
let x = 42
let name = "RP2040"
let pi = 3.14159
let active = true
let items = [1, 2, 3]
let data = {"key": "value"}
```

**Functions:**

```sagelang
proc calculate(x, y):
    return x * y + 10

let result = calculate(5, 3)
```

**Classes:**

```sagelang
class Sensor:
    proc init(self, pin):
        self.pin = pin
        gpio_init(pin, false)
    
    proc read(self):
        return gpio_read(self.pin)

let button = Sensor(15)
if button.read():
    print "Button pressed!"
```

**Generators:**

```sagelang
proc fibonacci(n):
    let a = 0
    let b = 1
    for i in range(n):
        yield a
        let temp = a + b
        a = b
        b = temp

for fib in fibonacci(10):
    print fib
```

### Available Native Functions

**GPIO (see docs/GPIO_INTEGRATION.md):**

- `gpio_init(pin, is_output)` – Initialize GPIO pin
- `gpio_write(pin, value)` – Set output level
- `gpio_read(pin)` – Read input level
- `gpio_toggle(pin)` – Toggle output
- `gpio_set_pull(pin, mode)` – 0=none, 1=up, 2=down

**System Info (see docs/SYSTEM_INFO.md):**

- `sys_version()` – OS version string
- `sys_uptime()` – Seconds since boot
- `sys_temp()` – CPU temperature (°C)
- `sys_clock()` – Clock speed (MHz)
- `sys_free_ram()` – Free RAM (KB)
- `sys_total_ram()` – Total RAM (KB)
- `sys_board_id()` – Unique board ID
- `sys_info()` – Dictionary of all metrics
- `sys_print()` – Print formatted system report

**Timing:**

- `sleep(ms)` – Delay milliseconds
- `sleep_us(us)` – Delay microseconds
- `time_ms()` – Milliseconds since boot
- `time_us()` – Microseconds since boot

**Configuration:**

- `config_set(key, value)` – Set config value
- `config_get(key)` – Get config value (or null)
- `config_has(key)` – Check if key exists
- `config_remove(key)` – Remove key
- `config_save()` – Write to flash
- `config_load()` – Read from flash

**Watchdog:**

- `wdt_enable(timeout_ms)` – Enable watchdog
- `wdt_disable()` – Disable watchdog
- `wdt_feed()` – Reset timer
- `wdt_get_timeout()` – Get timeout value

## Shell Commands

### Core Commands

```bash
help              # Show available commands
version           # System and SageLang version
reboot            # Software reboot
clear             # Clear screen
history           # Show command history
```

### System Monitoring

```bash
health            # Quick system health check
stats             # Detailed system statistics
supervisor        # Supervisor control (start|stop|status|alerts)
dmesg             # Kernel message buffer (see: dmesg --help)
```

### User & Permission Management

```bash
users             # User account commands (list|get|exists|help)
perms             # Permission utilities (check|decode|presets|help)
```

### Memory Management

```bash
memory            # Memory diagnostics
                  # Subcommands:
                  #   stats     - Show memory statistics
                  #   available - Show free memory
                  #   leaks     - Check for memory leaks
                  #   test <sz> <cnt> - Test allocations
                  #   defrag    - Compact heap
                  #   threshold <%> - Set warning level
```

### SageLang Commands

```bash
sage              # Start interactive REPL
sage -e "code"    # Execute inline code
sage --help       # SageLang help
```

### Storage Commands

```bash
storage save <name>     # Save new script
storage list            # List all scripts
storage run <name>      # Execute script
storage delete <name>   # Delete script
storage show <name>     # Display script content
storage autoboot <name> # Set auto-boot script
storage noboot          # Disable auto-boot
```

## Project Structure

```text
littleOS/
├── boot/
│   └── boot.c                    # System entry & initialization
├── src/
│   ├── kernel/
│   │   ├── kernel.c              # Kernel main loop
│   │   └── dmesg.c               # Kernel message ring buffer
│   ├── drivers/
│   │   ├── uart.c                # UART driver
│   │   ├── watchdog.c            # Watchdog timer
│   │   └── supervisor.c          # Supervisor core (Core 1)
│   ├── sys/
│   │   ├── system_info.c         # System monitoring
│   │   ├── permissions.c         # Capability + permissions system
│   │   ├── users_config.c        # User database (root + optional app user)
│   │   └── memory.c              # Memory management core
│   ├── storage/
│   │   ├── config_storage.c      # Persistent config
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
│   │   └── sage_multicore.c      # Multi-core support
│   └── shell/
│       ├── shell.c               # Shell main loop
│       ├── cmd_sage.c            # Sage commands
│       ├── cmd_script.c          # Script storage commands
│       ├── cmd_dmesg.c           # Dmesg commands
│       ├── cmd_supervisor.c      # Supervisor commands
│       ├── cmd_users.c           # User management commands
│       └── cmd_perms.c           # Permission commands
├── include/
│   ├── watchdog.h
│   ├── system_info.h
│   ├── config_storage.h
│   ├── script_storage.h
│   ├── permissions.h
│   ├── users_config.h
│   ├── memory.h
│   ├── sage_embed.h
│   └── hal/
│       └── gpio.h
├── third_party/
│   └── sagelang/                 # SageLang submodule
├── docs/                         # Detailed documentation
├── examples/                     # Example scripts
├── CMakeLists.txt                # Build configuration
├── build.sh                      # Interactive build script
└── README.md                     # This file
```

## Documentation

### Quick References

- **[CHANGELOG.md](CHANGELOG.md)** – Version history and changes
- **docs/QUICK_REFERENCE.md** – Command cheat sheet

### System Details

- **docs/BOOT_SEQUENCE.md** – Boot process and initialization
- **docs/SHELL_FEATURES.md** – Shell capabilities and behavior
- **docs/SYSTEM_INFO.md** – System monitoring API
- **docs/SCRIPT_STORAGE.md** – Flash storage and management

### Integration Guides

- **docs/GPIO_INTEGRATION.md** – Hardware control
- **docs/SAGELANG_INTEGRATION.md** – Language embedding and extending
- **docs/PERMISSIONS_SECURITY.md** – User system and capabilities
- **docs/MEMORY_MANAGEMENT.md** – Heap management (in development)

### SageLang Language

- **third_party/sagelang/README.md** – Full language documentation
- **third_party/sagelang/examples/** – Language examples and tutorials

## Hardware Requirements

- **Board:** Raspberry Pi Pico or any RP2040-based board
- **USB:** For programming and serial communication
- **Optional:** UART adapter for dedicated serial (GPIO 0/1)

**Pin Reference (Raspberry Pi Pico):**

```text
GPIO 25 - Built-in LED
GPIO 0  - UART TX (optional)
GPIO 1  - UART RX (optional)
GPIO 0–29 - Available for general use
```

## Memory Layout

```text
RP2040 RAM (264 KB total):
├─ littleOS Kernel       ~100 KB
├─ Stack                 ~100 KB
└─ SageLang Heap + OS     ~64 KB

Flash (2 MB):
├─ littleOS Binary       ~150 KB
├─ Script Storage          4 KB
├─ Configuration           4 KB
└─ Free Space           ~1.8 MB
```

## Performance

- **Boot time:** <1 second to shell
- **Script execution:** Direct interpretation (no JIT)
- **GPIO operations:** Typically <10 μs latency
- **Temperature reading:** ~100 μs
- **Watchdog overhead:** <5 μs per iteration
- **Memory allocation:** O(n) where n = free blocks

## Examples

### LED Patterns

```sagelang
proc blink_pattern(pin, pattern):
    gpio_init(pin, true)
    for duration in pattern:
        gpio_toggle(pin)
        sleep(duration)

# Morse code "SOS"
let sos = [200, 200, 200, 200, 200, 600,
           600, 200, 600, 200, 600, 600,
           200, 200, 200, 200, 200, 1000]
blink_pattern(25, sos)
```

### Temperature Logger

```sagelang
proc log_temperature(interval_ms):
    while(true):
        let temp = sys_temp()
        let uptime = sys_uptime()
        print str(uptime) + "s: " + str(temp) + "°C"
        
        if temp > 50.0:
            print "WARNING: High temperature!"
        
        sleep(interval_ms)

log_temperature(10000)  # Every 10 seconds
```

### Button-Controlled LED

```sagelang
class LED:
    proc init(self, pin):
        self.pin = pin
        gpio_init(pin, true)
    
    proc on(self):
        gpio_write(self.pin, true)
    
    proc off(self):
        gpio_write(self.pin, false)

class Button:
    proc init(self, pin):
        self.pin = pin
        gpio_init(pin, false)
        gpio_set_pull(pin, 1)  # Pull-up
    
    proc is_pressed(self):
        return gpio_read(self.pin) == false

let led = LED(25)
let button = Button(15)

while(true):
    if button.is_pressed():
        led.on()
    else:
        led.off()
    sleep(50)
```

### Configuration Persistence

```sagelang
# Device configuration example
config_set("name", "HomeMonitor")
config_set("location", "Kitchen")
config_set("sample_interval", 30000)
config_save()

# Read back
let name = config_get("name")
let interval = config_get("sample_interval")
print "Device: " + name + " (samples every " + str(interval) + "ms)"
```

## Troubleshooting

### System Won't Boot

- Hold BOOTSEL and reconnect USB to enter bootloader mode
- Re-flash the UF2 file
- Check serial connection (115200 baud, 8N1)
- Try: `screen /dev/ttyACM0 115200`

### Watchdog Resets

- Normal for infinite loops without `sleep()` or `wdt_feed()`
- Add `sleep(10)` in tight loops
- Monitor with `dmesg` to see reset messages
- Disable with `wdt_disable()` for debugging

### Script Errors

- Check syntax (indentation matters!)
- Verify GPIO pins are valid (0–29)
- Monitor memory with `memory stats`
- Use `storage show <name>` to debug scripts

### Serial Connection

- **Linux:** Check `/dev/ttyACM*` or `/dev/ttyUSB*` permissions
- **Windows:** Verify COM port in Device Manager
- **macOS:** Look for `/dev/tty.usbmodem*` or `/dev/cu.usbmodem*`
- Try: `sudo chmod 666 /dev/ttyACM0` (Linux)

### Memory Issues

- Check heap fragmentation with `memory stats`
- Run `memory defrag` to compact heap
- Monitor for leaks with `memory leaks`
- Set warning threshold with `memory threshold 80`

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas for Contribution:**

- Hardware drivers (I2C, SPI, PWM, ADC)
- Additional SageLang examples
- Documentation improvements
- Task scheduler (currently in development)
- Memory optimization
- Testing and bug reports
- Platform ports (ESP32, STM32)

## Roadmap

**Current (v0.3.0):**
- ✅ Memory management core
- ✅ User & permission system
- ✅ Watchdog protection
- ✅ SageLang integration
- 🔄 Task scheduler (in development)

**Planned:**
- Task-aware memory accounting
- Real-time scheduling preemption
- IPC mechanisms (message passing)
- I2C/SPI/PWM/ADC drivers
- Networking support
- Over-the-air updates

## Resources

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)
- [Raspberry Pi Pico Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)

## License

MIT License – see [LICENSE](LICENSE) for details.

---

**Built for embedded education | Powered by SageLang and littleOS Core**