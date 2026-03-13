# littleOS Quick Reference

Quick reference for all littleOS shell commands and SageLang functions.

---

## Shell Commands

### System

| Command | Description |
| ------- | ----------- |
| `help` | Show all commands grouped by category |
| `version` | OS version, SageLang version, supervisor status |
| `clear` | Clear terminal screen |
| `reboot` | Software reboot via watchdog |
| `history` | Show command history |
| `health` | Quick system health check |
| `stats` | Detailed system statistics |
| `supervisor` | Core 1 supervisor (start/stop/status/alerts) |
| `dmesg` | Kernel messages (-f follow, -l level, -c clear) |
| `fetch` | System info display (neofetch-style) |
| `top` | Live system monitor (htop-style) |

### Processes & Memory

| Command | Description |
| ------- | ----------- |
| `tasks` | Task scheduler management |
| `memory` | Heap diagnostics (stats/leaks/test/defrag/threshold) |
| `profile` | Runtime profiling |

### Filesystem & Text

| Command | Description |
| ------- | ----------- |
| `fs` | Filesystem (init/mount/mkdir/touch/cat/write/ls/sync/info/fsck) |
| `cat <file>` | Display file contents |
| `echo <text>` | Print text (supports `>` redirect) |
| `head <file>` | Show first lines |
| `tail <file>` | Show last lines |
| `wc <file>` | Word/line/byte count |
| `grep <pat> <file>` | Search patterns |
| `hexdump <file>` | Hex dump |
| `tee <file>` | Duplicate output to file |
| `proc` | Process filesystem (/proc) |
| `dev` | Device filesystem (/dev) |

### Hardware

| Command | Description |
| ------- | ----------- |
| `hw` | I2C/SPI/PWM/ADC peripherals |
| `pio` | PIO programmable I/O |
| `dma` | DMA engine control |
| `usb` | USB device mode |
| `pinout` | Visual GPIO pin diagram |
| `i2cscan` | I2C bus scanner |
| `wire` | Interactive I2C/SPI REPL |
| `pwmtune` | PWM frequency/duty tuner |
| `adc` | ADC read/stream/stats |
| `gpiowatch` | GPIO state monitor |
| `neopixel` | WS2812 NeoPixel LED control |
| `display` | SSD1306 OLED display |

### Networking (Pico W)

| Command | Description |
| ------- | ----------- |
| `net` | WiFi/TCP/UDP (scan/connect/status/ping/http) |
| `mqtt` | MQTT client (connect/pub/sub) |
| `remote` | Remote shell over TCP |
| `ota` | Over-the-air firmware updates |

### Scripting & Services

| Command | Description |
| ------- | ----------- |
| `sage` | SageLang REPL / `-e "code"` inline |
| `script` | Flash script storage (save/list/run/autoboot) |
| `pkg` | Package manager |
| `sensor` | Sensor framework |
| `power` | Power management and sleep |
| `cron` | Scheduled tasks |
| `ipc` | Inter-process communication |
| `users` | User account management |
| `perms` | Permission utilities |

### Shell Built-ins

| Command | Description |
| ------- | ----------- |
| `env` | View/set environment variables |
| `alias` | Command aliases |
| `export` | Set environment variable |
| `screen` | Terminal multiplexer |
| `man` | Manual pages |

### Debug & Diagnostics

| Command | Description |
| ------- | ----------- |
| `logcat` | Structured logging with filters |
| `trace` | Execution trace buffer |
| `watchpoint` | Memory watchpoints |
| `benchmark` | Performance benchmarks (cpu/mem/gpio/fs) |
| `selftest` | Hardware self-test suite |
| `coredump` | Crash dump viewer |
| `syslog` | Persistent system log |

---

## SageLang Native Functions

### GPIO

| Function | Description | Example |
| -------- | ----------- | ------- |
| `gpio_init(pin, is_output)` | Initialize pin | `gpio_init(25, true)` |
| `gpio_write(pin, value)` | Set output | `gpio_write(25, true)` |
| `gpio_read(pin)` | Read input | `let v = gpio_read(15)` |
| `gpio_toggle(pin)` | Toggle output | `gpio_toggle(25)` |
| `gpio_set_pull(pin, mode)` | Set pull resistor | `gpio_set_pull(15, 1)` |

Pull modes: `0` = None, `1` = Pull-up, `2` = Pull-down

### System Info

| Function | Returns | Description |
| -------- | ------- | ----------- |
| `sys_version()` | string | OS version |
| `sys_uptime()` | number | Uptime in seconds |
| `sys_temp()` | number | CPU temperature (C) |
| `sys_clock()` | number | CPU clock in MHz |
| `sys_free_ram()` | number | Free RAM in KB |
| `sys_total_ram()` | number | Total RAM in KB |
| `sys_board_id()` | string | Unique board ID |
| `sys_info()` | dict | All system metrics |
| `sys_print()` | nil | Print formatted report |

### Timing

| Function | Description | Example |
| -------- | ----------- | ------- |
| `sleep(ms)` | Delay milliseconds | `sleep(1000)` |
| `sleep_us(us)` | Delay microseconds | `sleep_us(100)` |
| `time_ms()` | Milliseconds since boot | `let t = time_ms()` |
| `time_us()` | Microseconds since boot | `let t = time_us()` |

### Configuration

| Function | Description | Example |
| -------- | ----------- | ------- |
| `config_set(key, value)` | Set config | `config_set("name", "pico")` |
| `config_get(key)` | Get value (or null) | `let v = config_get("name")` |
| `config_has(key)` | Check existence | `if config_has("name")` |
| `config_remove(key)` | Delete key | `config_remove("old")` |
| `config_save()` | Write to flash | `config_save()` |
| `config_load()` | Read from flash | `config_load()` |

### Watchdog

| Function | Description |
| -------- | ----------- |
| `wdt_enable(timeout_ms)` | Enable watchdog timer |
| `wdt_disable()` | Disable watchdog |
| `wdt_feed()` | Reset watchdog timer |
| `wdt_get_timeout()` | Get current timeout value |

---

## SageLang Syntax

### Variables and Types

```sagelang
let x = 42
let name = "Alice"
let flag = true
let items = [1, 2, 3]
let info = {"key": "value"}
```

### Control Flow

```sagelang
if (x > 10):
    print("Large")
else:
    print("Small")

while (x < 100):
    x = x + 1

for (item in items):
    print(item)
```

### Functions and Classes

```sagelang
proc greet(name):
    print("Hello, " + name)

class Sensor:
    proc init(self, pin):
        self.pin = pin
        gpio_init(pin, false)
    proc read(self):
        return gpio_read(self.pin)
```

### Generators

```sagelang
proc fibonacci(n):
    let a = 0, b = 1
    for i in range(n):
        yield a
        let temp = a + b
        a = b
        b = temp
```

---

## Common Patterns

### Blink LED

```sagelang
gpio_init(25, true)
while (true):
    gpio_toggle(25)
    sleep(500)
```

### Read Button

```sagelang
gpio_init(15, false)
gpio_set_pull(15, 1)
while (true):
    if (!gpio_read(15)):
        print("Pressed!")
    sleep(100)
```

### System Monitor

```sagelang
while (true):
    let temp = sys_temp()
    let free = sys_free_ram()
    print("Temp: " + str(temp) + "C, RAM: " + str(free) + " KB")
    sleep(5000)
```

---

## Shell Features

- **Pipes:** `proc cpuinfo | grep MHz`
- **Redirect:** `echo "data" > /file.txt`
- **Env vars:** `export VAR=value`, use `$VAR` in commands
- **Aliases:** `alias ll="fs ls /"`
- **Repeat:** `!!` repeats last command
- **Tab completion:** Press Tab on partial command name
- **History:** UP/DOWN arrows, `history` to list

---

## RP2040 Pin Reference

- GPIO 25: Built-in LED (Pico), CYW43 controlled (Pico W)
- GPIO 0: UART TX
- GPIO 1: UART RX
- GPIO 0-22: General purpose I/O
- GPIO 26-29: ADC capable (ADC0-ADC3)
- Logic levels: 3.3V, max 12mA per pin, 50mA total

---

## Build Quick Reference

```bash
# Standard Pico
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build && cmake .. && make -j$(nproc)

# Pico W (with WiFi)
mkdir build_w && cd build_w && cmake -DLITTLEOS_PICO_W=ON .. && make -j$(nproc)

# Flash
cp littleos.uf2 /media/$USER/RPI-RP2/

# Connect
screen /dev/ttyACM0 115200
```

---

**Version**: 0.6.0
**Platform**: Raspberry Pi Pico / Pico W (RP2040)
**Last Updated**: March 2026
