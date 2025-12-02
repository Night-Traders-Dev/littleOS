# Changelog

All notable changes to littleOS will be documented in this file.

## [Unreleased]

### Added - System Information Module (2024-12-02)

#### New System Information API
- **`include/system_info.h`** - System information interface
- **`src/system_info.c`** - RP2040 system information implementation
  - CPU information (model, clock speed, cores, revision)
  - Memory statistics (total/free/used RAM, flash size)
  - System uptime tracking
  - Internal temperature sensor reading
  - Unique board ID retrieval
  - Version and build information

#### SageLang Native Functions
- **`src/sage_system.c`** - System information bindings for SageLang
  - `sys_version()` - Get OS version string
  - `sys_uptime()` - Get uptime in seconds
  - `sys_temp()` - Get temperature in Celsius
  - `sys_clock()` - Get CPU clock speed in MHz
  - `sys_free_ram()` - Get free RAM in KB
  - `sys_total_ram()` - Get total RAM in KB
  - `sys_board_id()` - Get unique board identifier
  - `sys_info()` - Get comprehensive info dictionary
  - `sys_print()` - Print formatted system report

#### Integration Changes
- **`src/sage_embed.c`** - Automatic system function registration
- **`include/sage_embed.h`** - Added `sage_register_system_functions()` declaration
- **`CMakeLists.txt`** - Updated build system to include:
  - `src/system_info.c` in littleos_core
  - `src/sage_system.c` when SageLang is enabled
  - Linked against `hardware_adc`, `hardware_clocks`, `hardware_watchdog`, `pico_unique_id`, `pico_bootrom`

#### Documentation
- **`docs/SYSTEM_INFO.md`** - Comprehensive documentation
  - Complete API reference for all 9 functions
  - 4+ working examples (monitor, memory report, temperature logger, dashboard)
  - C API reference
  - Technical details (sensor specs, performance metrics)
  - Troubleshooting guide

### Technical Details

#### Available Metrics
- **CPU**: Model (RP2040), clock speed, core count, revision
- **Memory**: Total/free/used RAM (264KB), flash size (2MB)
- **Sensors**: Internal temperature sensor (±5°C accuracy)
- **Uptime**: Millisecond-precision system uptime
- **Identity**: 64-bit unique chip ID
- **Version**: OS version and build timestamp

#### Performance
- All functions sub-millisecond except `sys_print()` (~1ms)
- Temperature reading: ~100μs (ADC conversion time)
- No dynamic memory allocation
- Minimal stack usage (<200 bytes)

### Usage Examples

#### System Monitor (SageLang)
```sagelang
while (true) {
    let temp = sys_temp();
    let free = sys_free_ram();
    print("Temp: " + str(temp) + "°C, Free RAM: " + str(free) + " KB");
    sleep(5000);
}
```

#### Get System Info Dictionary
```sagelang
let info = sys_info();
print("OS: " + info["version"]);
print("CPU: " + info["cpu_model"] + " @ " + str(info["cpu_mhz"]) + " MHz");
print("Temp: " + str(info["temperature"]) + "°C");
```

#### C API Usage
```c
#include "system_info.h"

float temp = system_get_temperature();
if (temp > 50.0f) {
    printf("High temperature warning!\r\n");
}
```

### Testing

Tested on Raspberry Pi Pico (RP2040):
- CPU information retrieval
- Memory statistics accuracy
- Temperature sensor readings
- Uptime tracking over extended periods
- Board ID uniqueness verification

### Breaking Changes
None - this is a new feature addition.

### Dependencies
- Pico SDK with `hardware_adc`, `hardware_clocks`, `pico_unique_id`
- System functions only available when `SAGE_ENABLED=1`

### Future Enhancements
- Voltage monitoring (VSYS, VBUS)
- Flash filesystem statistics
- Process/task information
- Network statistics (when connectivity added)
- Historical metric tracking

---

### Added - GPIO Integration (2024-12-02)

#### New Hardware Abstraction Layer
- **`include/hal/gpio.h`** - GPIO HAL interface definition
- **`src/hal/gpio.c`** - GPIO HAL implementation for RP2040
  - Pin initialization (input/output)
  - Digital read/write operations
  - Pin toggling
  - Pull-up/pull-down resistor configuration
  - Pin range validation (GPIO 0-29)

#### SageLang Native Functions
- **`src/sage_gpio.c`** - Native function bindings for SageLang
  - `gpio_init(pin, is_output)` - Initialize GPIO pin
  - `gpio_write(pin, value)` - Set pin output state
  - `gpio_read(pin)` - Read pin input state
  - `gpio_toggle(pin)` - Toggle pin output
  - `gpio_set_pull(pin, mode)` - Configure pull resistors

#### Integration Changes
- **`src/sage_embed.c`** - Automatic GPIO function registration during SageLang initialization
- **`include/sage_embed.h`** - Added `sage_register_gpio_functions()` declaration
- **`CMakeLists.txt`** - Updated build system to include:
  - `src/hal/gpio.c` in littleos_core
  - `src/sage_gpio.c` when SageLang is enabled
  - Linked against `hardware_gpio` library

#### Documentation
- **`docs/GPIO_INTEGRATION.md`** - Comprehensive GPIO documentation
  - Architecture overview
  - Complete API reference for SageLang functions
  - C API reference
  - 4+ complete working examples
  - Pin reference for Raspberry Pi Pico
  - Troubleshooting guide

### Technical Details

#### Architecture
The GPIO integration uses a three-layer architecture:

1. **Hardware Layer**: Pico SDK (`hardware/gpio.h`)
2. **HAL Layer**: Platform abstraction (`hal/gpio.h`, `src/hal/gpio.c`)
3. **Language Binding**: SageLang native functions (`src/sage_gpio.c`)

This design allows:
- Easy porting to other RP2040-based boards
- Consistent API across C and SageLang
- Input validation and error handling
- Future platform abstraction (STM32, ESP32, etc.)

#### Memory Impact
- HAL implementation: ~2KB code
- SageLang bindings: ~5KB code
- No runtime memory allocation
- Stack usage: <100 bytes per function call

#### Performance
- Direct SDK calls (minimal overhead)
- No buffering or caching
- Immediate hardware access
- Pin validation adds ~5 CPU cycles

### Usage Examples

#### Blink LED (SageLang)
```sagelang
gpio_init(25, true);
while (true) {
    gpio_toggle(25);
    sleep(500);
}
```

#### Button Input (SageLang)
```sagelang
gpio_init(15, false);
gpio_set_pull(15, 1);
let pressed = gpio_read(15);
```

#### C API Usage
```c
#include "hal/gpio.h"

gpio_hal_init(25, GPIO_DIR_OUT);
gpio_hal_write(25, true);
```

### Testing

All GPIO functions have been tested on:
- Raspberry Pi Pico (RP2040)
- Built-in LED (GPIO 25)
- External buttons and LEDs

### Breaking Changes
None - this is a new feature addition.

### Dependencies
- Requires Pico SDK with `hardware_gpio` library
- SageLang GPIO functions only available when `SAGE_ENABLED=1`

### Future Enhancements
- PWM support for LED dimming and servo control
- Hardware interrupt support
- I2C/SPI peripheral bindings
- ADC (analog input) support
- GPIO event callbacks in SageLang

---

## [0.1.0] - Previous Release

### Added
- Initial littleOS kernel
- SageLang integration
- UART communication
- Basic shell interface
- Script storage system

---

## Format

This changelog follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format.

Types of changes:
- **Added** for new features
- **Changed** for changes in existing functionality
- **Deprecated** for soon-to-be removed features
- **Removed** for now removed features
- **Fixed** for any bug fixes
- **Security** for vulnerability fixes
