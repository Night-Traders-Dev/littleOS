# Multi-Core Programming Guide

**Run SageLang scripts on both cores of the RP2040 simultaneously**

## Overview

The RP2040 has two ARM Cortex-M0+ cores running at 125MHz. littleOS provides a clean API to:
- Launch SageLang scripts on Core 1
- Communicate between cores via hardware FIFO
- Run independent tasks in parallel
- Maximize RP2040 performance

### Why Use Both Cores?

**Core 0 (Main):**
- Interactive shell
- User input handling
- System management

**Core 1 (Worker):**
- Background tasks
- Real-time control loops
- Data processing
- LED animations

**Result:** 2x processing power, responsive UI while doing work

---

## Quick Start

### Example 1: Blink LED on Core 1

```sagelang
# Core 0: Launch blinking LED on Core 1
core1_launch_code("
    gpio_init(25, true)
    while(true):
        gpio_toggle(25)
        sleep(500)
")

print "Core 1 is now blinking the LED!"
print "Core 0 is still responsive!"

# You can keep typing commands while Core 1 blinks
```

### Example 2: Communication Between Cores

**Core 0 (sends data):**
```sagelang
# Launch counter on Core 1
core1_launch_code("
    let count = 0
    while(true):
        core_send(count)
        count = count + 1
        sleep(1000)
")

# Core 0 receives and displays
while(true):
    if core_fifo_available():
        let value = core_receive()
        print "Got from Core 1: " + str(value)
    sleep(100)
```

---

## SageLang API Reference

### Core 1 Control

#### `core1_launch_script(name)` 
Launch a stored script on Core 1.

**Parameters:**
- `name` (string) - Name of stored script

**Returns:** `true` on success, `false` on failure

**Example:**
```sagelang
# First, save a script
storage save background
gpio_init(25, true)
while(true):
    gpio_toggle(25)
    sleep(1000)
^D

# Then launch it on Core 1
let success = core1_launch_script("background")
if success:
    print "Core 1 started!"
```

---

#### `core1_launch_code(code)`
Launch inline SageLang code on Core 1.

**Parameters:**
- `code` (string) - SageLang source code

**Returns:** `true` on success, `false` on failure

**Example:**
```sagelang
core1_launch_code("
    print 'Hello from Core 1!'
    sleep(1000)
    print 'Core 1 done!'
")
```

**Notes:**
- Only one script can run on Core 1 at a time
- Core 1 runs independently until completion
- Core 1 shares memory with Core 0 (be careful!)

---

#### `core1_stop()`
Stop Core 1 execution.

**Returns:** `true` if stopped, `false` if already idle

**Example:**
```sagelang
core1_launch_code("while(true): sleep(1000)")
sleep(5000)
core1_stop()
print "Core 1 stopped"
```

**Warning:** This is a hard stop - use for cleanup only

---

#### `core1_is_running()`
Check if Core 1 is executing code.

**Returns:** `true` if running, `false` otherwise

**Example:**
```sagelang
if core1_is_running():
    print "Core 1 is busy"
else:
    print "Core 1 is idle"
```

---

#### `core1_get_state()`
Get detailed Core 1 state.

**Returns:** Integer state code:
- `0` = IDLE (not running)
- `1` = RUNNING (executing code)
- `2` = ERROR (encountered error)
- `3` = STOPPED (finished execution)

**Example:**
```sagelang
let state = core1_get_state()
if state == 1:
    print "Core 1 running"
else if state == 2:
    print "Core 1 error!"
```

---

### Inter-Core Communication

The RP2040 provides a hardware FIFO (First-In-First-Out) buffer for communication:
- **8 entries deep** (each entry is 32-bit number)
- **Bidirectional** (Core 0 ↔ Core 1)
- **Hardware-accelerated** (very fast)

#### `core_send(data)`
Send 32-bit value to other core.

**Parameters:**
- `data` (number) - Value to send (0 to 4294967295)

**Returns:** `null`

**Behavior:** Blocks if FIFO is full (8 entries)

**Example:**
```sagelang
core_send(42)
core_send(123)
core_send(999)
```

---

#### `core_send_nb(data)`
Send value to other core (non-blocking).

**Parameters:**
- `data` (number) - Value to send

**Returns:** `true` if sent, `false` if FIFO full

**Example:**
```sagelang
let sent = core_send_nb(42)
if sent:
    print "Sent successfully"
else:
    print "FIFO full, try again later"
```

---

#### `core_receive()`
Receive 32-bit value from other core.

**Returns:** Number received from other core

**Behavior:** Blocks if FIFO is empty

**Example:**
```sagelang
print "Waiting for data..."
let value = core_receive()
print "Received: " + str(value)
```

---

#### `core_receive_nb()`
Receive value from other core (non-blocking).

**Returns:** Number if available, `null` if FIFO empty

**Example:**
```sagelang
let value = core_receive_nb()
if value != null:
    print "Got: " + str(value)
else:
    print "No data available"
```

---

#### `core_fifo_available()`
Check if FIFO has data.

**Returns:** Number of values available (0 or 1+)

**Example:**
```sagelang
if core_fifo_available():
    let data = core_receive()
```

---

#### `core_num()`
Get current core number.

**Returns:** `0` for Core 0, `1` for Core 1

**Example:**
```sagelang
let core = core_num()
print "Running on Core " + str(core)
```

---

## Complete Examples

### Example 1: Background LED Animation

```sagelang
# Core 0: Launch animation on Core 1
core1_launch_code("
    gpio_init(25, true)
    
    # Breathing effect
    while(true):
        # Fade in
        let i = 0
        while(i < 10):
            gpio_toggle(25)
            sleep(50)
            i = i + 1
        
        # Fade out
        i = 0
        while(i < 10):
            gpio_toggle(25)
            sleep(100)
            i = i + 1
")

print "LED animation running on Core 1"
print "Shell is still responsive!"

# You can still use the shell while LED animates
```

### Example 2: Sensor Monitor with LED Indicator

**Core 1 (monitor temperature):**
```sagelang
core1_launch_code("
    gpio_init(25, true)
    
    while(true):
        let temp = sys_temp()
        
        # Send temperature to Core 0
        core_send(int(temp))
        
        # Blink faster if hot
        if temp > 40.0:
            gpio_toggle(25)
            sleep(100)  # Fast blink
        else:
            gpio_toggle(25)
            sleep(500)  # Slow blink
")
```

**Core 0 (display temperatures):**
```sagelang
while(true):
    if core_fifo_available():
        let temp = core_receive()
        print "Temperature: " + str(temp) + "°C"
    sleep(1000)
```

### Example 3: Dual Counter

**Both cores count independently:**

**Core 1:**
```sagelang
core1_launch_code("
    let count = 0
    while(true):
        print '[Core 1] Count: ' + str(count)
        count = count + 1
        sleep(1000)
")
```

**Core 0:**
```sagelang
let count = 0
while(true):
    print "[Core 0] Count: " + str(count)
    count = count + 1
    sleep(1500)
```

**Output:**
```
[Core 0] Count: 0
[Core 1] Count: 0
[Core 1] Count: 1
[Core 0] Count: 1
[Core 1] Count: 2
[Core 1] Count: 3
[Core 0] Count: 2
...
```

### Example 4: Producer-Consumer Pattern

**Core 1 (producer):**
```sagelang
core1_launch_code("
    let sensor_id = 0
    while(true):
        # Simulate sensor reading
        let reading = int(sys_temp() * 10)
        
        # Send sensor data
        core_send(sensor_id)
        core_send(reading)
        
        sleep(500)
")
```

**Core 0 (consumer):**
```sagelang
while(true):
    if core_fifo_available():
        let id = core_receive()
        let value = core_receive()
        print "Sensor " + str(id) + ": " + str(value)
    sleep(100)
```

---

## Best Practices

### 1. Keep Core 1 Scripts Simple

✅ **Good:**
```sagelang
core1_launch_code("
    while(true):
        gpio_toggle(25)
        sleep(500)
")
```

❌ **Avoid:**
```sagelang
# Complex nested logic on Core 1
core1_launch_code("
    class Manager:
        # Lots of complex state...
")
```

### 2. Use Non-Blocking Receives

✅ **Good:**
```sagelang
let data = core_receive_nb()
if data != null:
    print data
```

❌ **Avoid:**
```sagelang
# This blocks forever if no data!
let data = core_receive()
```

### 3. Handle FIFO Overflow

✅ **Good:**
```sagelang
if core_send_nb(value):
    print "Sent"
else:
    print "FIFO full, dropping data"
```

❌ **Avoid:**
```sagelang
# Blocks if FIFO full!
core_send(value)
```

### 4. Watchdog on Both Cores

Core 1 scripts should feed the watchdog too:

```sagelang
core1_launch_code("
    while(true):
        # Do work...
        wdt_feed()  # Keep watchdog happy
        sleep(1000)
")
```

---

## Technical Details

### Memory Architecture

- **Shared RAM:** Both cores access the same 264KB SRAM
- **No MMU:** No memory protection between cores
- **No cache:** Coherency is automatic

**Implications:**
- Variables are shared (be careful!)
- No need for synchronization primitives for simple cases
- For complex sharing, use FIFO communication

### FIFO Specifications

- **Hardware:** SIO (Single-cycle I/O) block
- **Depth:** 8 entries per direction
- **Width:** 32 bits per entry
- **Latency:** ~1 cycle (8ns @ 125MHz)
- **Interrupt capable:** Yes (not exposed in SageLang)

### Performance

- **Core launch:** ~1ms overhead
- **FIFO send/receive:** <10μs
- **Context switch:** None (truly parallel)
- **Memory:** Each SageLang context ~10KB overhead

### Limitations

- **One script per core:** Can't run multiple scripts on Core 1
- **No preemption:** Core 1 runs until completion
- **Shared heap:** GC affects both cores
- **No mutex API:** Use FIFO for synchronization

---

## Troubleshooting

### Core 1 Won't Start

**Symptom:** `core1_launch_code()` returns `false`

**Solutions:**
- Check if Core 1 already running: `core1_is_running()`
- Stop existing script: `core1_stop()`
- Check syntax of script code

### FIFO Communication Fails

**Symptom:** `core_receive()` blocks forever

**Solutions:**
- Use `core_receive_nb()` instead
- Check `core_fifo_available()` first
- Verify both cores are sending/receiving

### System Hangs

**Symptom:** Both cores freeze

**Solutions:**
- Add `wdt_feed()` to Core 1 loops
- Avoid infinite loops without delays
- Check for deadlocks in communication

### Memory Issues

**Symptom:** Out of memory errors

**Solutions:**
- Each core has separate SageLang context (~10KB)
- Total heap is shared (64KB for both)
- Keep Core 1 scripts small
- Use `sage -m` to check memory

---

## C API Reference

### For Advanced Users

If you're extending littleOS in C:

```c
#include "multicore.h"

// Initialize
multicore_init();

// Launch script from storage
multicore_launch_script("my_script");

// Launch inline code
multicore_launch_code("print 'Hello'");

// Stop Core 1
multicore_stop();

// Check status
if (multicore_is_running()) {
    printf("Core 1 busy\n");
}

// FIFO communication
multicore_send(42);
uint32_t data = multicore_receive();

// Non-blocking
if (multicore_send_nb(123)) {
    printf("Sent\n");
}

uint32_t value;
if (multicore_receive_nb(&value)) {
    printf("Got: %u\n", value);
}
```

---

## Further Reading

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) - Chapter 2.3 (Multicore)
- [Pico SDK Multicore](https://raspberrypi.github.io/pico-sdk-doxygen/group__pico__multicore.html)
- [SIO Hardware](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#section.sio) - FIFO details

---

**Unlock the full power of RP2040! 🚀**
