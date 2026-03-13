# littleOS Shell Features

## Overview

The littleOS shell provides a Unix-like command environment with 60+ commands, pipes, aliases, environment variables, tab completion, and command history. All commands support `man <cmd>` for built-in documentation.

## Command Reference

### System

| Command | Description |
|---------|-------------|
| `help` | Show all commands grouped by category |
| `version` | OS version, SageLang version, supervisor status |
| `clear` | Clear terminal screen (ANSI escape) |
| `reboot` | Software reboot via watchdog |
| `history` | Show command history |
| `exit` | Logout |
| `health` | Quick system health check |
| `stats` | Detailed system statistics |
| `supervisor` | Core 1 supervisor control (start/stop/status/alerts) |
| `dmesg` | Kernel message buffer (-f follow, -l level, -c clear, -s stats) |
| `fetch` | System info display (neofetch-style) |
| `top` | Live system monitor (htop-style, auto-refreshing) |

### Processes & Memory

| Command | Description |
|---------|-------------|
| `tasks` | Task scheduler management |
| `memory` | Heap diagnostics (stats/available/leaks/test/defrag/threshold) |
| `profile` | Runtime profiling (function timing and call counts) |

### Users & Security

| Command | Description |
|---------|-------------|
| `users` | User account management (list/get/exists) |
| `perms` | Permission utilities (check/decode/presets) |

### Filesystem & Text

| Command | Description |
|---------|-------------|
| `fs` | F2FS-style filesystem (init/mount/mkdir/touch/cat/write/ls/sync/info/fsck) |
| `cat` | Display file contents |
| `echo` | Print text (supports `>` redirect) |
| `head` | Show first lines of file |
| `tail` | Show last lines of file |
| `wc` | Word/line/byte count |
| `grep` | Search for patterns in files |
| `hexdump` | Hex dump of file |
| `tee` | Duplicate output to file |

### Virtual Filesystems

| Command | Description |
|---------|-------------|
| `proc` | Process filesystem (/proc/cpuinfo, /proc/meminfo, etc.) |
| `dev` | Device filesystem (/dev/gpio, /dev/uart, etc.) |

### Hardware

| Command | Description |
|---------|-------------|
| `hw` | Hardware peripherals (I2C/SPI/PWM/ADC) |
| `pio` | PIO programmable I/O state machines |
| `dma` | DMA engine control |
| `usb` | USB device mode (CDC/HID/MSC) |
| `pinout` | Visual GPIO pin diagram |
| `i2cscan` | I2C bus scanner |
| `wire` | Interactive I2C/SPI REPL |
| `pwmtune` | PWM frequency/duty cycle tuner |
| `adc` | ADC read/stream/stats |
| `gpiowatch` | GPIO state monitor (watch pins for changes) |
| `neopixel` | WS2812 NeoPixel LED control |
| `display` | SSD1306 OLED display driver |

### Networking (Pico W)

| Command | Description |
|---------|-------------|
| `net` | WiFi/TCP/UDP (scan/connect/status/ping/http/socket) |
| `mqtt` | MQTT IoT client (connect/pub/sub) |
| `remote` | Remote shell over TCP |
| `ota` | Over-the-air firmware updates |

### Scripting & Packages

| Command | Description |
|---------|-------------|
| `sage` | SageLang REPL / inline execution (`-e "code"`) |
| `script` | Flash script storage (save/list/run/delete/show/autoboot/noboot) |
| `pkg` | Package manager |

### Services

| Command | Description |
|---------|-------------|
| `sensor` | Sensor framework and logging |
| `power` | Power management and sleep modes |
| `cron` | Scheduled tasks (add/list/remove) |
| `ipc` | Inter-process communication channels |

### Debug & Diagnostics

| Command | Description |
|---------|-------------|
| `logcat` | Structured logging with tag/level filters |
| `trace` | Execution trace buffer |
| `watchpoint` | Memory watchpoints (break on read/write) |
| `benchmark` | Performance benchmarks (cpu/mem/gpio/fs) |
| `selftest` | Hardware self-test suite |
| `coredump` | Crash dump viewer (survives reboot) |
| `syslog` | Persistent system log (survives reboot) |

### Shell Built-ins

| Command | Description |
|---------|-------------|
| `env` | View/set environment variables |
| `alias` | Command aliases |
| `export` | Set environment variable |
| `screen` | Terminal multiplexer (split panes) |
| `man` | Manual pages for all commands |

---

## Shell Capabilities

### Command History

The shell maintains a circular buffer of the last 10 commands, navigable with arrow keys.

| Key | Action |
|-----|--------|
| Up Arrow | Previous command |
| Down Arrow | Next command |
| Backspace | Delete character |
| Enter | Execute command |
| Tab | Auto-complete command name |

- Consecutive duplicate commands are not stored
- Empty commands are not added to history
- Use `history` to view all stored commands
- Use `!!` to repeat the last command

### Pipes

Chain commands together with `|`:

```bash
> proc cpuinfo | grep MHz
> echo "hello world" | wc
```

### Output Redirect

Redirect output to a file with `>`:

```bash
> echo "config=value" > /config.txt
```

### Environment Variables

```bash
> export DEVICE=pico001
> echo $DEVICE
pico001
> env
DEVICE=pico001
```

Variables are expanded in all commands. Set defaults at boot with `shell_env_init()`.

### Aliases

```bash
> alias ll="fs ls /"
> alias info="health"
> ll
# Executes: fs ls /
```

### Tab Completion

Press Tab to auto-complete command names. If multiple matches exist, all candidates are shown.

### ANSI Escape Support

Arrow keys send multi-byte ANSI escape sequences:

- Up Arrow: `ESC [ A` (0x1B 0x5B 0x41)
- Down Arrow: `ESC [ B` (0x1B 0x5B 0x42)

The shell uses a 3-state parser (NORMAL -> ESC -> CSI) to handle these sequences.

---

## Terminal Configuration

Recommended settings:

| Setting | Value |
|---------|-------|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Flow Control | None |
| Local Echo | Off |
| ANSI/VT100 | Enabled |

### Tested Terminals

| Terminal | Status |
|----------|--------|
| PuTTY | Works |
| screen | Works |
| minicom | Works |
| TeraTerm | Works |
| Windows Terminal | Works |
| Arduino Serial Monitor | Limited ANSI support |

---

## Technical Details

### Memory Usage

- History buffer: ~2.5 KB (10 commands x 256 bytes)
- Command parsing: stack-based, no dynamic allocation
- Tab completion: scans command table (no extra memory)

### Performance

- Navigation speed: <1ms
- History lookup: O(1) constant time
- Tab completion: O(n) command table scan
- Command dispatch: O(n) table lookup

---

**Version**: 0.6.0
**Last Updated**: March 2026
