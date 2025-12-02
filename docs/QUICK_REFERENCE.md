# littleOS Quick Reference

Quick reference for all available littleOS commands and functions.

---

## GPIO Functions

Control hardware GPIO pins on the RP2040.

| Function | Description | Example |
|----------|-------------|----------|
| `gpio_init(pin, is_output)` | Initialize GPIO pin | `gpio_init(25, true);` |
| `gpio_write(pin, value)` | Set pin output state | `gpio_write(25, true);` |
| `gpio_read(pin)` | Read pin input state | `let state = gpio_read(15);` |
| `gpio_toggle(pin)` | Toggle pin output | `gpio_toggle(25);` |
| `gpio_set_pull(pin, mode)` | Set pull resistor | `gpio_set_pull(15, 1);` |

**Pull modes:** `0` = None, `1` = Pull-up, `2` = Pull-down

**See also:** [GPIO_INTEGRATION.md](GPIO_INTEGRATION.md)

---

## System Information Functions

Get system metrics and hardware information.

| Function | Returns | Description |
|----------|---------|-------------|
| `sys_version()` | string | OS version ("0.2.0") |
| `sys_uptime()` | number | Uptime in seconds |
| `sys_temp()` | number | Temperature in °C |
| `sys_clock()` | number | CPU clock in MHz |
| `sys_free_ram()` | number | Free RAM in KB |
| `sys_total_ram()` | number | Total RAM in KB |
| `sys_board_id()` | string | Unique board ID (16 hex chars) |
| `sys_info()` | dict | Complete system info |
| `sys_print()` | nil | Print formatted report |

**See also:** [SYSTEM_INFO.md](SYSTEM_INFO.md)

---

## SageLang Built-in Functions

Standard library functions available in SageLang.

### I/O Functions

| Function | Description | Example |
|----------|-------------|----------|
| `print(value)` | Print to console | `print("Hello");` |
| `input()` | Read user input | `let name = input();` |

### Type Conversion

| Function | Description | Example |
|----------|-------------|----------|
| `tonumber(value)` | Convert to number | `let n = tonumber("123");` |
| `str(value)` | Convert to string | `let s = str(42);` |

### Array Functions

| Function | Description | Example |
|----------|-------------|----------|
| `len(array)` | Get array length | `let count = len(arr);` |
| `push(array, value)` | Add to end | `push(arr, 42);` |
| `pop(array)` | Remove from end | `let last = pop(arr);` |
| `range(start, end)` | Generate range | `let nums = range(0, 10);` |
| `slice(array, start, end)` | Extract subarray | `let sub = slice(arr, 0, 5);` |

### String Functions

| Function | Description | Example |
|----------|-------------|----------|
| `split(str, delimiter)` | Split string | `let words = split("a,b,c", ",");` |
| `join(array, separator)` | Join array | `let str = join(words, " ");` |
| `replace(str, old, new)` | Replace text | `let s = replace("hi", "i", "ello");` |
| `upper(str)` | Convert to uppercase | `let up = upper("hello");` |
| `lower(str)` | Convert to lowercase | `let low = lower("HELLO");` |
| `strip(str)` | Remove whitespace | `let clean = strip(" text ");` |

### Dictionary Functions

| Function | Description | Example |
|----------|-------------|----------|
| `dict_keys(dict)` | Get all keys | `let keys = dict_keys(d);` |
| `dict_values(dict)` | Get all values | `let vals = dict_values(d);` |
| `dict_has(dict, key)` | Check if key exists | `let exists = dict_has(d, "name");` |
| `dict_delete(dict, key)` | Remove key | `dict_delete(d, "old_key");` |

### Time Functions

| Function | Description | Example |
|----------|-------------|----------|
| `clock()` | Get current time | `let t = clock();` |
| `sleep(ms)` | Sleep milliseconds | `sleep(1000);` |

### Garbage Collection

| Function | Description | Example |
|----------|-------------|----------|
| `gc_collect()` | Force garbage collection | `gc_collect();` |
| `gc_stats()` | Get GC statistics | `let stats = gc_stats();` |
| `gc_enable()` | Enable automatic GC | `gc_enable();` |
| `gc_disable()` | Disable automatic GC | `gc_disable();` |

---

## Common Patterns

### Blink LED

```sagelang
gpio_init(25, true);
while (true) {
    gpio_toggle(25);
    sleep(500);
}
```

### Read Button

```sagelang
gpio_init(15, false);
gpio_set_pull(15, 1);

while (true) {
    if (!gpio_read(15)) {
        print("Button pressed!");
    }
    sleep(100);
}
```

### Monitor System

```sagelang
while (true) {
    let temp = sys_temp();
    let free = sys_free_ram();
    print("Temp: " + str(temp) + "°C, RAM: " + str(free) + " KB");
    sleep(5000);
}
```

### Temperature Alert

```sagelang
let threshold = 45;
while (true) {
    let temp = sys_temp();
    if (temp > threshold) {
        print("⚠️  HIGH TEMP: " + str(temp) + "°C");
        gpio_init(25, true);
        gpio_write(25, true);  // Turn on warning LED
    } else {
        gpio_write(25, false);
    }
    sleep(2000);
}
```

### Memory Usage Bar

```sagelang
let total = sys_total_ram();
let free = sys_free_ram();
let used = total - free;
let percent = (used / total) * 100;

let bars = percent / 5;
let bar = "[";
let i = 0;
while (i < 20) {
    if (i < bars) {
        bar = bar + "█";
    } else {
        bar = bar + "░";
    }
    i = i + 1;
}
bar = bar + "]";
print(bar + " " + str(percent) + "%");
```

---

## RP2040 Pin Reference

### Special Pins

- **GPIO 25**: Built-in LED (Pico board)
- **GPIO 0-22**: General purpose I/O on edge connectors
- **GPIO 26-29**: Can be used as ADC inputs (ADC0-ADC3)

### Important Notes

- Logic levels: 3.3V (HIGH), 0V (LOW)
- **Do NOT apply 5V** to any pin
- Max current per pin: 12mA (recommended), 16mA (absolute max)
- Total current all GPIO: 50mA max
- Valid pin range: 0-29

---

## SageLang Syntax Basics

### Variables

```sagelang
let x = 42;
let name = "Alice";
let flag = true;
let items = [1, 2, 3];
let info = {"key": "value"};
```

### Control Flow

```sagelang
// If statement
if (x > 10) {
    print("Large");
} else {
    print("Small");
}

// While loop
while (x < 100) {
    x = x + 1;
}

// For loop
for (item in items) {
    print(item);
}
```

### Functions

```sagelang
proc greet(name) {
    print("Hello, " + name + "!");
}

proc add(a, b) {
    return a + b;
}

greet("World");
let sum = add(5, 3);
```

---

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `sage` | Enter SageLang REPL |
| `version` | Show OS version |
| `clear` | Clear screen |

---

## Getting Help

- **Full Documentation**: See `/docs/` directory
- **GPIO Guide**: [GPIO_INTEGRATION.md](GPIO_INTEGRATION.md)
- **System Info Guide**: [SYSTEM_INFO.md](SYSTEM_INFO.md)
- **SageLang Quickstart**: [SAGELANG_QUICKSTART.md](../SAGELANG_QUICKSTART.md)
- **Contributing**: [CONTRIBUTING.md](../CONTRIBUTING.md)

---

## Quick Troubleshooting

### GPIO Issues

- **LED won't light**: Check pin is initialized as output: `gpio_init(pin, true)`
- **Button not responding**: Enable pull-up resistor: `gpio_set_pull(pin, 1)`
- **Pin validation errors**: RP2040 only has GPIO 0-29

### System Info Issues

- **Temperature reads 0**: Call `sys_temp()` a second time (ADC initialization)
- **Free RAM seems wrong**: This is an estimate; use for trends, not exact values
- **Board ID all zeros**: Try power cycle; may indicate hardware issue

### General Issues

- **SageLang commands not found**: Ensure `SAGE_ENABLED=1` in build
- **Out of memory**: Check with `sys_free_ram()`, call `gc_collect()`
- **System freezes**: May be infinite loop; check your script logic

---

## Version Information

- **littleOS Version**: 0.2.0
- **SageLang Version**: 0.8.0
- **Target Platform**: Raspberry Pi Pico (RP2040)
- **Last Updated**: December 2, 2025
