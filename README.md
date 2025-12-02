# littleOS for RP2040

**A minimal, crash-resistant operating system for Raspberry Pi Pico with embedded SageLang scripting**

[![Version](https://img.shields.io/badge/version-0.3.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-RP2040-red.svg)](https://www.raspberrypi.com/documentation/microcontrollers/)

## Overview

littleOS is an educational operating system that brings high-level scripting to bare metal RP2040. Write interactive hardware control programs in SageLang—a clean, indentation-based language with classes, generators, and automatic memory management.

### Key Features

- 🛡️ **Watchdog Protection** - Automatic recovery from hangs and crashes (8s timeout)
- 🎯 **Hardware Integration** - Direct GPIO, timer, and sensor access from scripts
- 💾 **Persistent Storage** - Save scripts and configurations to flash
- 🔄 **Auto-boot Scripts** - Run programs automatically on startup
- 📟 **System Monitoring** - Real-time temperature, memory, and uptime tracking
- 🚀 **Interactive REPL** - Live coding with immediate feedback
- 🎓 **Educational** - Clear architecture perfect for learning embedded systems

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

# Build
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)

# Flash (hold BOOTSEL button, connect USB)
cp littleos.uf2 /media/$USER/RPI-RP2/

# Connect
screen /dev/ttyACM0 115200
```

### First Commands

```
Welcome to littleOS Shell!

> help
> version
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

**Automatic crash recovery** - System automatically reboots after 8 seconds of inactivity:

```sagelang
# This will trigger watchdog reset
while(true) {}  # System reboots after 8s
```

On recovery, you'll see:
```
*** RECOVERED FROM CRASH ***
System was reset by watchdog timer
```

Protection is active in:
- Shell command loop
- SageLang REPL
- Script execution
- All system operations

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

> storage autoboot blink
# Will run on next boot
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

### Available Native Functions

**GPIO (see [docs/GPIO_INTEGRATION.md](docs/GPIO_INTEGRATION.md)):**
- `gpio_init(pin, is_output)` - Initialize pin
- `gpio_write(pin, value)` - Set output
- `gpio_read(pin)` - Read input
- `gpio_toggle(pin)` - Toggle output
- `gpio_set_pull(pin, mode)` - Pull resistors (0=none, 1=up, 2=down)

**System Info (see [docs/SYSTEM_INFO.md](docs/SYSTEM_INFO.md)):**
- `sys_version()` - OS version
- `sys_uptime()` - Uptime in seconds
- `sys_temp()` - CPU temperature (°C)
- `sys_clock()` - Clock speed (MHz)
- `sys_free_ram()` - Free RAM (KB)
- `sys_total_ram()` - Total RAM (KB)
- `sys_board_id()` - Unique board ID
- `sys_info()` - Full info dictionary
- `sys_print()` - Print formatted report

**Timing:**
- `sleep(ms)` - Delay milliseconds
- `sleep_us(us)` - Delay microseconds
- `time_ms()` - Milliseconds since boot
- `time_us()` - Microseconds since boot

**Configuration:**
- `config_set(key, value)` - Set config value
- `config_get(key)` - Get config value (or null)
- `config_has(key)` - Check if key exists
- `config_remove(key)` - Remove key
- `config_save()` - Write to flash
- `config_load()` - Read from flash

**Watchdog:**
- `wdt_enable(timeout_ms)` - Enable watchdog
- `wdt_disable()` - Disable watchdog
- `wdt_feed()` - Reset timer
- `wdt_get_timeout()` - Get timeout value

## Shell Commands

### Core Commands

```bash
help              # Show available commands
version           # System and SageLang version
reboot            # Software reboot
clear             # Clear screen
```

### SageLang Commands

```bash
sage              # Start interactive REPL
sage -e "code"    # Execute inline code
sage -m           # Show memory stats
sage --help       # SageLang help
```

### Storage Commands

```bash
storage save <name>     # Save new script
storage list            # List all scripts
storage run <name>      # Execute script
storage delete <name>   # Delete script
storage show <name>     # Display script
storage autoboot <name> # Set auto-boot script
storage noboot          # Disable auto-boot
```

## Project Structure

```
littleOS/
├── boot/
│   └── boot.c                    # System entry & initialization
├── src/
│   ├── kernel.c                  # Kernel main loop
│   ├── watchdog.c                # Watchdog timer
│   ├── system_info.c             # System monitoring
│   ├── config_storage.c          # Persistent config
│   ├── script_storage.c          # Script flash storage
│   ├── hal/
│   │   └── gpio.c                # GPIO hardware abstraction
│   ├── sage_embed.c              # SageLang runtime
│   ├── sage_gpio.c               # GPIO bindings
│   ├── sage_system.c             # System bindings
│   ├── sage_time.c               # Timing bindings
│   ├── sage_config.c             # Config bindings
│   ├── sage_watchdog.c           # Watchdog bindings
│   └── shell/
│       ├── shell.c               # Shell main loop
│       ├── cmd_sage.c            # Sage command
│       └── cmd_storage.c         # Storage commands
├── include/
│   ├── watchdog.h
│   ├── system_info.h
│   ├── config_storage.h
│   ├── script_storage.h
│   ├── sage_embed.h
│   └── hal/
│       └── gpio.h
├── third_party/
│   └── sagelang/                 # SageLang submodule
├── docs/                         # Detailed documentation
└── examples/                     # Example scripts
```

## Documentation

### Quick References
- [CHANGELOG.md](CHANGELOG.md) - Version history
- [docs/QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md) - Command cheat sheet

### System Details
- [docs/BOOT_SEQUENCE.md](docs/BOOT_SEQUENCE.md) - Boot process
- [docs/SHELL_FEATURES.md](docs/SHELL_FEATURES.md) - Shell capabilities
- [docs/SYSTEM_INFO.md](docs/SYSTEM_INFO.md) - System monitoring API
- [docs/SCRIPT_STORAGE.md](docs/SCRIPT_STORAGE.md) - Flash storage

### Integration Guides
- [docs/GPIO_INTEGRATION.md](docs/GPIO_INTEGRATION.md) - Hardware control
- [docs/SAGELANG_INTEGRATION.md](docs/SAGELANG_INTEGRATION.md) - Language embedding

### SageLang Language
- [third_party/sagelang/README.md](third_party/sagelang/README.md) - Full language docs
- [third_party/sagelang/examples/](third_party/sagelang/examples/) - Language examples

## Hardware Requirements

- **Board:** Raspberry Pi Pico or any RP2040-based board
- **USB:** For programming and serial communication
- **Optional:** UART adapter for hardware serial (GPIO 0/1)

**Pin Reference (Raspberry Pi Pico):**
```
GPIO 25 - Built-in LED
GPIO 0  - UART TX (optional)
GPIO 1  - UART RX (optional)
GPIO 0-29 - Available for general use
```

## Memory Layout

```
RP2040 RAM (264 KB total):
├─ littleOS Kernel      ~100 KB
├─ Stack                ~100 KB
└─ SageLang Heap         64 KB

Flash (2 MB):
├─ littleOS Binary      ~150 KB
├─ Script Storage         4 KB
├─ Configuration          4 KB
└─ Free Space          ~1.8 MB
```

## Performance

- **Boot time:** <1 second to shell
- **Script execution:** Direct interpretation (no compilation)
- **GPIO operations:** <10μs latency
- **Temperature reading:** ~100μs
- **Watchdog check:** <5μs overhead

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

## Troubleshooting

### System Won't Boot
- Hold BOOTSEL and reconnect USB to enter bootloader mode
- Re-flash the UF2 file
- Check serial connection (115200 baud, 8N1)

### Watchdog Resets
- Normal for infinite loops without `sleep()` or `wdt_feed()`
- Add `sleep(10)` in tight loops
- Disable with `wdt_disable()` for debugging

### Script Errors
- Check syntax (indentation matters!)
- Verify GPIO pins are valid (0-29)
- Monitor memory with `sage -m`

### Serial Connection
- Linux: Check `/dev/ttyACM*` permissions
- Windows: Verify COM port in Device Manager
- macOS: Look for `/dev/tty.usbmodem*`

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

**Areas for Contribution:**
- Hardware drivers (I2C, SPI, PWM, ADC)
- Additional SageLang examples
- Documentation improvements
- Platform ports (ESP32, STM32)
- Testing and bug reports

## Resources

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)
- [Raspberry Pi Pico Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)

## License

MIT License - see [LICENSE](LICENSE) for details.

---

**Built with ❤️ for embedded education | Powered by SageLang**
