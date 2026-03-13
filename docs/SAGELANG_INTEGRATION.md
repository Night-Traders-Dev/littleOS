# SageLang Integration in littleOS

## Overview

[SageLang](https://github.com/Night-Traders-Dev/SageLang) is integrated into littleOS as a Git submodule, providing an interactive scripting environment on bare-metal RP2040.

**Features:** Classes, generators, exception handling, garbage collection, arrays, dictionaries, 30+ native functions, GPIO/system/timing/config/watchdog bindings.

## Current Status

**Version:** littleOS v0.6.0 with SageLang v0.8.0

All integration phases are complete:

- Phase 1: Preparation and Analysis
- Phase 2: Minimal Interpreter Port
- Phase 3: Shell Integration (REPL, inline execution)
- Phase 4: Hardware Bindings (GPIO, system, timing, config, watchdog)
- Phase 5: Script Storage (flash persistence, auto-boot)

## Submodule Layout

```text
third_party/sagelang/
├── src/
│   ├── c/          # C implementation (compiled for RP2040)
│   │   ├── lexer.c
│   │   ├── parser.c
│   │   ├── ast.c
│   │   ├── interpreter.c
│   │   ├── value.c
│   │   ├── env.c
│   │   ├── gc.c
│   │   ├── module.c
│   │   └── sage_thread.c
│   └── sage/       # Self-hosting Sage implementation
└── include/        # Headers (shared between C and Pico builds)
```

The CMake build compiles sources from `src/c/` with `PICO_BUILD=1` and `SAGE_NO_FFI=1` defined.

## Platform Detection

SageLang uses macros from `include/sage_thread.h`:

```c
#if defined(PICO_BUILD)
    #define SAGE_PLATFORM_PICO  1
    #define SAGE_HAS_THREADS    0   // No pthreads on RP2040
#else
    #define SAGE_PLATFORM_PICO  0
    #define SAGE_HAS_THREADS    1   // Desktop has pthreads
#endif
```

This guards:

- `__thread` TLS (replaced with `static int` on Pico)
- `thread_spawn_native` (returns error on Pico)
- REPL longjmp recovery (replaced with `exit(1)` on Pico)

## Embedding API

The embedding layer (`src/sage/sage_embed.c`) provides:

```c
sage_context_t* sage_init(void);
void sage_cleanup(sage_context_t* ctx);
sage_result_t sage_eval_string(sage_context_t* ctx, const char* source, size_t len);
sage_result_t sage_repl(sage_context_t* ctx);
```

## Native Function Bindings

### GPIO (`src/sage/sage_gpio.c`)

| Function | Description |
| -------- | ----------- |
| `gpio_init(pin, is_output)` | Initialize GPIO pin |
| `gpio_write(pin, value)` | Set output state |
| `gpio_read(pin)` | Read input state |
| `gpio_toggle(pin)` | Toggle output |
| `gpio_set_pull(pin, mode)` | Configure pull resistor |

### System (`src/sage/sage_system.c`)

| Function | Description |
| -------- | ----------- |
| `sys_version()` | OS version string |
| `sys_uptime()` | Uptime in seconds |
| `sys_temp()` | CPU temperature |
| `sys_clock()` | Clock speed (MHz) |
| `sys_free_ram()` | Free RAM (KB) |
| `sys_total_ram()` | Total RAM (KB) |
| `sys_board_id()` | Unique board ID |
| `sys_info()` | Dictionary of all metrics |
| `sys_print()` | Formatted system report |

### Timing (`src/sage/sage_time.c`)

| Function | Description |
| -------- | ----------- |
| `sleep(ms)` | Millisecond delay |
| `sleep_us(us)` | Microsecond delay |
| `time_ms()` | Milliseconds since boot |
| `time_us()` | Microseconds since boot |

### Configuration (`src/sage/sage_config.c`)

| Function | Description |
| -------- | ----------- |
| `config_set(key, value)` | Set config value |
| `config_get(key)` | Get config value |
| `config_has(key)` | Check if key exists |
| `config_remove(key)` | Delete key |
| `config_save()` | Write to flash |
| `config_load()` | Read from flash |

### Watchdog (`src/sage/sage_watchdog.c`)

| Function | Description |
| -------- | ----------- |
| `wdt_enable(timeout_ms)` | Enable watchdog |
| `wdt_disable()` | Disable watchdog |
| `wdt_feed()` | Reset timer |
| `wdt_get_timeout()` | Get timeout value |

## Shell Usage

```bash
# Interactive REPL
> sage
sage> let x = 42
sage> print x * 2
84
sage> exit

# Inline execution
> sage -e "print 2 + 2"
4

# Memory stats
> sage -m

# Help
> sage --help
```

## Script Storage

```bash
> script save blink
gpio_init(25, true)
while(true):
    gpio_toggle(25)
    sleep(500)
^D

> script list
> script run blink
> script autoboot blink   # Run on boot
> script noboot           # Disable
```

## Build Configuration

In `CMakeLists.txt`:

```cmake
set(SAGELANG_DIR ${CMAKE_SOURCE_DIR}/third_party/sagelang)

add_library(sagelang STATIC
    ${SAGELANG_DIR}/src/c/lexer.c
    ${SAGELANG_DIR}/src/c/parser.c
    ${SAGELANG_DIR}/src/c/ast.c
    ${SAGELANG_DIR}/src/c/interpreter.c
    ${SAGELANG_DIR}/src/c/value.c
    ${SAGELANG_DIR}/src/c/env.c
    ${SAGELANG_DIR}/src/c/gc.c
    ${SAGELANG_DIR}/src/c/module.c
    ${SAGELANG_DIR}/src/c/sage_thread.c
)

target_include_directories(sagelang PUBLIC ${SAGELANG_DIR}/include)
target_compile_definitions(sagelang PUBLIC PICO_BUILD=1 SAGE_NO_FFI=1)
target_link_libraries(sagelang PUBLIC pico_stdlib)
```

## Resources

- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)
- [SageLang Guide](../third_party/sagelang/documentation/SageLang_Guide.md)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)

---

**Last Updated**: March 2026
