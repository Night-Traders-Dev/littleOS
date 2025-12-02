// src/hal/gpio.c
// GPIO Hardware Abstraction Layer Implementation for RP2040
#include "hal/gpio.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

// RP2040 has GPIO 0-29 available
#define GPIO_MIN_PIN 0
#define GPIO_MAX_PIN 29

// Debug flag - set to 1 to enable verbose GPIO debug output
#define GPIO_DEBUG 1

/**
 * @brief Validate GPIO pin number
 */
static bool is_valid_pin(uint8_t pin) {
    return (pin >= GPIO_MIN_PIN && pin <= GPIO_MAX_PIN);
}

/**
 * @brief Initialize a GPIO pin
 */
bool gpio_hal_init(uint8_t pin, gpio_direction_t dir) {
    if (!is_valid_pin(pin)) {
        printf("GPIO Error: Invalid pin number %d (valid: %d-%d)\r\n", 
               pin, GPIO_MIN_PIN, GPIO_MAX_PIN);
        return false;
    }
    
#if GPIO_DEBUG
    printf("GPIO: Initializing pin %d as %s\r\n", 
           pin, (dir == GPIO_DIR_OUT) ? "OUTPUT" : "INPUT");
#endif
    
    // Initialize the pin
    gpio_init(pin);
    
    // Set direction
    if (dir == GPIO_DIR_OUT) {
        gpio_set_dir(pin, GPIO_OUT);
    } else {
        gpio_set_dir(pin, GPIO_IN);
    }
    
#if GPIO_DEBUG
    printf("GPIO: Pin %d initialized successfully\r\n", pin);
#endif
    
    return true;
}

/**
 * @brief Write value to GPIO pin
 */
void gpio_hal_write(uint8_t pin, bool value) {
    if (!is_valid_pin(pin)) {
        return;
    }
    
#if GPIO_DEBUG
    printf("GPIO: Writing %s to pin %d\r\n", value ? "HIGH" : "LOW", pin);
#endif
    
    gpio_put(pin, value);
}

/**
 * @brief Read value from GPIO pin
 */
bool gpio_hal_read(uint8_t pin) {
    if (!is_valid_pin(pin)) {
        return false;
    }
    
    bool value = gpio_get(pin);
    
#if GPIO_DEBUG
    printf("GPIO: Read %s from pin %d\r\n", value ? "HIGH" : "LOW", pin);
#endif
    
    return value;
}

/**
 * @brief Toggle GPIO pin state
 */
void gpio_hal_toggle(uint8_t pin) {
    if (!is_valid_pin(pin)) {
        return;
    }
    
#if GPIO_DEBUG
    bool current = gpio_get(pin);
    printf("GPIO: Toggling pin %d from %s to %s\r\n", 
           pin, current ? "HIGH" : "LOW", current ? "LOW" : "HIGH");
#endif
    
    gpio_xor_mask(1u << pin);
}

/**
 * @brief Set GPIO pull resistor
 */
void gpio_hal_set_pull(uint8_t pin, gpio_pull_t pull) {
    if (!is_valid_pin(pin)) {
        return;
    }
    
#if GPIO_DEBUG
    const char* pull_str = (pull == GPIO_PULL_UP) ? "PULL-UP" : 
                          (pull == GPIO_PULL_DOWN) ? "PULL-DOWN" : "NONE";
    printf("GPIO: Setting pin %d pull resistor to %s\r\n", pin, pull_str);
#endif
    
    switch (pull) {
        case GPIO_PULL_UP:
            gpio_pull_up(pin);
            break;
        case GPIO_PULL_DOWN:
            gpio_pull_down(pin);
            break;
        case GPIO_PULL_NONE:
            gpio_disable_pulls(pin);
            break;
        default:
            printf("GPIO Error: Invalid pull mode %d\r\n", pull);
            break;
    }
}

/**
 * @brief Get valid GPIO pin range for platform
 */
void gpio_hal_get_pin_range(uint8_t* min_pin, uint8_t* max_pin) {
    if (min_pin) *min_pin = GPIO_MIN_PIN;
    if (max_pin) *max_pin = GPIO_MAX_PIN;
}
