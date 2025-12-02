# GPIO Integration for littleOS

## Overview

The GPIO (General Purpose Input/Output) hardware abstraction layer provides access to the RP2040's GPIO pins from both C code and SageLang scripts.

## Architecture

```
┌─────────────────────────────────────┐
│      SageLang Scripts               │
│  (User-facing GPIO functions)       │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│   sage_gpio.c                       │
│   (SageLang Native Functions)       │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│   hal/gpio.h & src/hal/gpio.c       │
│   (Hardware Abstraction Layer)      │
└─────────────────┬───────────────────┘
                  │
┌─────────────────▼───────────────────┐
│   Pico SDK (hardware/gpio.h)        │
│   (Low-level hardware access)       │
└─────────────────────────────────────┘
```

## Available Functions

### SageLang GPIO Functions

These functions are available in SageLang scripts:

#### `gpio_init(pin, is_output)`
Initialize a GPIO pin as input or output.

**Parameters:**
- `pin` (number): GPIO pin number (0-29 for RP2040)
- `is_output` (boolean): `true` for output, `false` for input

**Returns:** `boolean` - `true` on success, `false` on error

**Example:**
```sagelang
// Initialize pin 25 (built-in LED on Pico) as output
let success = gpio_init(25, true);
if (success) {
    print("LED pin initialized");
}

// Initialize pin 15 as input
gpio_init(15, false);
```

---

#### `gpio_write(pin, value)`
Set the output state of a GPIO pin.

**Parameters:**
- `pin` (number): GPIO pin number
- `value` (boolean): `true` for HIGH (3.3V), `false` for LOW (0V)

**Returns:** `nil`

**Example:**
```sagelang
// Turn LED on
gpio_write(25, true);

// Turn LED off
gpio_write(25, false);
```

---

#### `gpio_read(pin)`
Read the current state of a GPIO pin.

**Parameters:**
- `pin` (number): GPIO pin number

**Returns:** `boolean` - `true` if HIGH, `false` if LOW

**Example:**
```sagelang
// Read button state from pin 15
let button_pressed = gpio_read(15);
if (button_pressed) {
    print("Button is pressed!");
}
```

---

#### `gpio_toggle(pin)`
Toggle the output state of a GPIO pin (HIGH→LOW or LOW→HIGH).

**Parameters:**
- `pin` (number): GPIO pin number

**Returns:** `nil`

**Example:**
```sagelang
// Toggle LED state
gpio_toggle(25);
```

---

#### `gpio_set_pull(pin, mode)`
Configure internal pull-up or pull-down resistors.

**Parameters:**
- `pin` (number): GPIO pin number
- `mode` (number): 
  - `0` = No pull resistor
  - `1` = Pull-up (default HIGH)
  - `2` = Pull-down (default LOW)

**Returns:** `nil`

**Example:**
```sagelang
// Enable pull-up on input pin
gpio_init(15, false);      // Set as input
gpio_set_pull(15, 1);      // Enable pull-up
```

---

## Complete Examples

### Example 1: Blink LED

```sagelang
// Initialize LED pin
gpio_init(25, true);

// Blink 10 times
let i = 0;
while (i < 10) {
    gpio_write(25, true);
    sleep(500);  // 500ms on
    gpio_write(25, false);
    sleep(500);  // 500ms off
    i = i + 1;
}
```

### Example 2: Button Input

```sagelang
// Setup
gpio_init(15, false);   // Button input
gpio_set_pull(15, 1);   // Pull-up (button pulls LOW when pressed)
gpio_init(25, true);    // LED output

// Monitor button and control LED
while (true) {
    let pressed = gpio_read(15);
    if (!pressed) {  // Active LOW (button pressed pulls pin LOW)
        gpio_write(25, true);  // LED on
    } else {
        gpio_write(25, false); // LED off
    }
    sleep(50);  // Small delay to debounce
}
```

### Example 3: Toggle with Button

```sagelang
// Setup
gpio_init(14, false);   // Button input
gpio_set_pull(14, 1);   // Pull-up
gpio_init(25, true);    // LED output

let last_state = true;  // Track button state for edge detection

// Toggle LED on button press
while (true) {
    let current_state = gpio_read(14);
    
    // Detect falling edge (button press)
    if (last_state && !current_state) {
        gpio_toggle(25);  // Toggle LED
        sleep(200);       // Debounce delay
    }
    
    last_state = current_state;
    sleep(10);
}
```

### Example 4: Multi-LED Pattern

```sagelang
// Initialize 4 LEDs
let pins = [16, 17, 18, 19];
let i = 0;
while (i < 4) {
    gpio_init(pins[i], true);
    i = i + 1;
}

// Knight Rider pattern
while (true) {
    // Forward
    let j = 0;
    while (j < 4) {
        gpio_write(pins[j], true);
        sleep(100);
        gpio_write(pins[j], false);
        j = j + 1;
    }
    
    // Backward
    j = 3;
    while (j >= 0) {
        gpio_write(pins[j], true);
        sleep(100);
        gpio_write(pins[j], false);
        j = j - 1;
    }
}
```

---

## C API Reference

For C code integration:

### Header
```c
#include "hal/gpio.h"
```

### Functions

```c
bool gpio_hal_init(uint8_t pin, gpio_direction_t dir);
void gpio_hal_write(uint8_t pin, bool value);
bool gpio_hal_read(uint8_t pin);
void gpio_hal_toggle(uint8_t pin);
void gpio_hal_set_pull(uint8_t pin, gpio_pull_t pull);
void gpio_hal_get_pin_range(uint8_t* min_pin, uint8_t* max_pin);
```

### Example C Code

```c
#include "hal/gpio.h"
#include "pico/stdlib.h"

void blink_led(void) {
    // Initialize LED pin
    gpio_hal_init(25, GPIO_DIR_OUT);
    
    // Blink 10 times
    for (int i = 0; i < 10; i++) {
        gpio_hal_write(25, true);
        sleep_ms(500);
        gpio_hal_write(25, false);
        sleep_ms(500);
    }
}
```

---

## Pin Reference for Raspberry Pi Pico

### Special Pins
- **GPIO 25**: Built-in LED (active HIGH)
- **GPIO 0-22**: General purpose pins on edge connectors
- **GPIO 26-29**: Can be used as ADC inputs (ADC0-ADC3)

### Pin Limitations
- Total available: GPIO 0-29 (30 pins)
- Some pins may be used by peripherals (I2C, SPI, UART)
- Consult Pico pinout diagram before connecting hardware

### Voltage Levels
- Logic HIGH: 3.3V (DO NOT connect 5V signals!)
- Logic LOW: 0V (GND)
- Max current per pin: 12mA recommended, 16mA absolute max
- Total current all pins: 50mA max

---

## Testing

### Quick Test in SageLang REPL

1. Boot littleOS and enter SageLang REPL:
```bash
sage> gpio_init(25, true);
sage> gpio_write(25, true);
```

You should see the built-in LED turn on!

2. Test blinking:
```bash
sage> gpio_toggle(25);
```

### Hardware Setup for Button Testing

```
RP2040 Pin 15 ----[Button]---- GND
RP2040 Pin 25 ---- Built-in LED (no external components needed)
```

With internal pull-up enabled, the button will read HIGH when not pressed, LOW when pressed.

---

## Troubleshooting

### LED not lighting up
- Check you initialized pin as OUTPUT: `gpio_init(pin, true)`
- Verify pin number (25 for built-in LED)
- Check connections if using external LED

### Button not responding
- Enable pull-up or pull-down resistor
- Check button wiring (should connect pin to GND)
- Add debounce delay in your code

### Pin validation errors
- RP2040 only has GPIO 0-29
- Some pins may be reserved by other peripherals

---

## Next Steps

- **PWM Support**: Coming soon for LED dimming and servo control
- **Interrupts**: Hardware interrupt handling for faster response
- **I2C/SPI**: Higher-level peripheral communication
- **ADC**: Analog input reading for sensors

---

## References

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Raspberry Pi Pico Pinout](https://datasheets.raspberrypi.com/pico/Pico-R3-A4-Pinout.pdf)
- [Pico SDK GPIO Documentation](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#hardware_gpio)
