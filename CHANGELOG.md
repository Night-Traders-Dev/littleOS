# Changelog

All notable changes to littleOS. Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.3.0] - 2025-12-02

### 🛡️ Added - Watchdog Timer System

**Comprehensive crash protection with automatic recovery**

#### Core Implementation
- **`src/watchdog.c`** - Hardware watchdog timer driver
  - Initialization and configuration (8 second default timeout)
  - Feed/reset functionality for keep-alive
  - Enable/disable control
  - Reset reason detection (distinguish crashes from normal reboots)
  - Configurable timeout periods
  
#### System Integration
- **Multi-layer protection:**
  - Shell main loop: Feeds every 1 second during idle
  - Command execution: Feeds before/after each command
  - SageLang REPL: Feeds every 1 second waiting for input
  - Script evaluation: Feeds before/after execution and between statements
  
- **Boot protection:**
  - Watchdog initialized but disabled during boot
  - Enabled only after full system initialization
  - Prevents false triggers during startup
  
- **Recovery indication:**
  - Clear "RECOVERED FROM CRASH" message on boot
  - User-visible indication of watchdog resets
  - Distinguishes crashes from intentional reboots

#### SageLang Bindings
- **`src/sage_watchdog.c`** - Watchdog control from scripts
  - `wdt_enable(timeout_ms)` - Enable with custom timeout
  - `wdt_disable()` - Disable watchdog
  - `wdt_feed()` - Manual keep-alive
  - `wdt_get_timeout()` - Query current timeout

#### Technical Details
- 8 second default timeout (configurable 1ms - 8388ms)
- Sub-millisecond feed overhead (<5μs)
- No dynamic memory allocation
- Minimal stack usage (~50 bytes)
- Hardware-based (RP2040 watchdog peripheral)

**Usage Example:**
```sagelang
# Enable custom 5-second timeout
wdt_enable(5000)

# Long-running task
while(true):
    # Do work...
    wdt_feed()  # Reset timer
    sleep(1000)
```

---

### 💾 Added - Script Storage System

**Persistent script storage in flash memory**

#### Core Implementation
- **`src/script_storage.c`** - Flash-based script storage
  - Save scripts with names (up to 8 slots)
  - List all stored scripts with sizes
  - Load and execute scripts by name
  - Delete scripts to free space
  - View script contents
  - 4KB flash sector allocation
  
#### Auto-boot Support
- **`src/config_storage.c`** - Enhanced with autoboot config
  - Store auto-boot script name
  - Automatic execution on system startup
  - Enable/disable auto-boot via config
  - Integrated with kernel boot sequence

#### Shell Commands
- **`src/shell/cmd_storage.c`** - Storage command handler
  - `storage save <name>` - Interactive script entry
  - `storage list` - Show all scripts
  - `storage run <name>` - Execute script
  - `storage delete <name>` - Remove script
  - `storage show <name>` - Display source
  - `storage autoboot <name>` - Set auto-boot
  - `storage noboot` - Disable auto-boot

#### Documentation
- **`docs/SCRIPT_STORAGE.md`** - Complete storage guide
  - API reference
  - Usage examples
  - Storage limits and constraints
  - Flash memory layout

**Usage Example:**
```bash
> storage save blink
# Paste script, end with Ctrl+D
gpio_init(25, true)
while(true):
    gpio_toggle(25)
    sleep(500)
^D
Script saved: blink (72 bytes)

> storage autoboot blink
Auto-boot enabled: blink

> reboot
# LED starts blinking automatically after boot
```

---

### 📟 Added - System Information Module

**Real-time hardware monitoring and diagnostics**

#### Core Implementation
- **`src/system_info.c`** - System information API
  - CPU: Model, clock speed, core count, silicon revision
  - Memory: Total/free/used RAM, flash size
  - Sensors: Internal temperature sensor
  - Identity: 64-bit unique board ID
  - Uptime: Millisecond-precision tracking
  - Version: OS version and build info

#### SageLang Bindings
- **`src/sage_system.c`** - System monitoring from scripts
  - `sys_version()` - OS version string
  - `sys_uptime()` - Uptime in seconds
  - `sys_temp()` - CPU temperature (°C, ±5°C accuracy)
  - `sys_clock()` - Clock speed (MHz)
  - `sys_free_ram()` - Free RAM (KB)
  - `sys_total_ram()` - Total RAM (KB)
  - `sys_board_id()` - Unique chip ID string
  - `sys_info()` - Complete info dictionary
  - `sys_print()` - Formatted system report

#### Documentation
- **`docs/SYSTEM_INFO.md`** - Complete monitoring guide
  - Full API reference
  - 4+ working examples
  - Performance characteristics
  - Troubleshooting

**Usage Example:**
```sagelang
# System monitor dashboard
while(true):
    let info = sys_info()
    print "===== System Status ====="
    print "Temp: " + str(info["temperature"]) + "°C"
    print "RAM: " + str(info["free_ram"]) + "/" + str(info["total_ram"]) + " KB"
    print "Uptime: " + str(sys_uptime()) + "s"
    print ""
    sleep(5000)
```

---

### 🎯 Added - GPIO Integration

**Complete hardware abstraction layer for GPIO control**

#### Hardware Abstraction
- **`src/hal/gpio.c`** - Platform-independent GPIO HAL
  - Pin initialization (input/output modes)
  - Digital read/write operations
  - Output toggling
  - Pull-up/pull-down resistor configuration
  - Input validation (GPIO 0-29)

#### SageLang Bindings
- **`src/sage_gpio.c`** - Hardware control from scripts
  - `gpio_init(pin, is_output)` - Configure pin direction
  - `gpio_write(pin, value)` - Set output state
  - `gpio_read(pin)` - Read input state
  - `gpio_toggle(pin)` - Toggle output
  - `gpio_set_pull(pin, mode)` - Configure pulls (0=none, 1=up, 2=down)

#### Documentation
- **`docs/GPIO_INTEGRATION.md`** - Complete hardware guide
  - Architecture overview
  - Full API reference
  - Pin reference for Pico
  - 4+ complete examples
  - Safety guidelines

**Usage Example:**
```sagelang
# Button-controlled LED
class LED:
    proc init(self, pin):
        self.pin = pin
        gpio_init(pin, true)
    
    proc toggle(self):
        gpio_toggle(self.pin)

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
        led.toggle()
        sleep(200)  # Debounce
    sleep(50)
```

---

### ⏱️ Added - Timing Functions

**Precise timing and delay utilities**

#### SageLang Bindings
- **`src/sage_time.c`** - Time utilities
  - `sleep(ms)` - Millisecond delay
  - `sleep_us(us)` - Microsecond delay  
  - `time_ms()` - Milliseconds since boot
  - `time_us()` - Microseconds since boot

**Usage Example:**
```sagelang
let start = time_ms()
# Do something...
let elapsed = time_ms() - start
print "Operation took " + str(elapsed) + "ms"
```

---

### ⚙️ Added - Configuration Storage

**Persistent key-value storage in flash**

#### Core Implementation
- **`src/config_storage.c`** - Flash-backed configuration
  - Key-value pairs (up to 16 entries)
  - String values (up to 64 bytes)
  - Flash persistence
  - Auto-boot script support
  - 4KB flash sector

#### SageLang Bindings
- **`src/sage_config.c`** - Configuration from scripts
  - `config_set(key, value)` - Set configuration
  - `config_get(key)` - Get value (returns null if missing)
  - `config_has(key)` - Check existence
  - `config_remove(key)` - Delete entry
  - `config_save()` - Persist to flash
  - `config_load()` - Load from flash

**Usage Example:**
```sagelang
# Device configuration
config_set("device_name", "sensor_node_01")
config_set("sample_rate", "1000")
config_save()

# Later...
let name = config_get("device_name")
let rate = int(config_get("sample_rate"))
```

---

### 📚 Documentation Improvements

- **Reorganized docs/** - Non-redundant, topic-focused guides
- **Comprehensive README** - Quick start to advanced features
- **API references** - Complete function documentation
- **Working examples** - Copy-paste ready code
- **Troubleshooting** - Common issues and solutions

---

### 🔧 Changed

- **Build system:** Updated CMakeLists.txt for all new modules
- **SageLang integration:** Automatic function registration
- **Kernel:** Watchdog initialization and boot sequence
- **Shell:** Added storage commands

---

### 🛠️ Technical Details

**Memory Impact:**
- Kernel + drivers: ~150KB flash, ~100KB RAM
- SageLang runtime: 64KB heap (configurable)
- Script storage: 4KB flash
- Configuration: 4KB flash

**Performance:**
- Boot time: <1 second to shell
- GPIO operations: <10μs
- Temperature reading: ~100μs
- Watchdog feed: <5μs
- Script execution: Direct interpretation

**Platform Support:**
- Tested on Raspberry Pi Pico (RP2040)
- Compatible with all RP2040-based boards
- Portable HAL design for future platforms

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
