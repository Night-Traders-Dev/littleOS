// src/sage_gpio.c
// SageLang Native Function Bindings for GPIO
#include "sage_embed.h"
#include "hal/gpio.h"
#include <stdio.h>
#include <stdlib.h>

// Include SageLang headers
#include "value.h"
#include "env.h"

/**
 * @brief SageLang native function: gpio_init(pin, is_output)
 * Initialize a GPIO pin as input or output
 * 
 * Usage in SageLang:
 *   gpio_init(25, true);   // Pin 25 as output (LED on Pico)
 *   gpio_init(15, false);  // Pin 15 as input
 */
static Value* sage_gpio_init(int argc, Value** args) {
    if (argc != 2) {
        fprintf(stderr, "gpio_init() requires 2 arguments: pin, is_output\r\n");
        return value_create_number(0);
    }
    
    if (args[0]->type != VALUE_NUMBER || args[1]->type != VALUE_BOOLEAN) {
        fprintf(stderr, "gpio_init() argument types: (number, boolean)\r\n");
        return value_create_number(0);
    }
    
    int pin = (int)args[0]->as.number;
    bool is_output = args[1]->as.boolean;
    
    gpio_direction_t dir = is_output ? GPIO_DIR_OUT : GPIO_DIR_IN;
    bool success = gpio_hal_init(pin, dir);
    
    return value_create_boolean(success);
}

/**
 * @brief SageLang native function: gpio_write(pin, value)
 * Set GPIO pin output state
 * 
 * Usage in SageLang:
 *   gpio_write(25, true);   // Set pin 25 HIGH
 *   gpio_write(25, false);  // Set pin 25 LOW
 */
static Value* sage_gpio_write(int argc, Value** args) {
    if (argc != 2) {
        fprintf(stderr, "gpio_write() requires 2 arguments: pin, value\r\n");
        return value_create_nil();
    }
    
    if (args[0]->type != VALUE_NUMBER || args[1]->type != VALUE_BOOLEAN) {
        fprintf(stderr, "gpio_write() argument types: (number, boolean)\r\n");
        return value_create_nil();
    }
    
    int pin = (int)args[0]->as.number;
    bool value = args[1]->as.boolean;
    
    gpio_hal_write(pin, value);
    
    return value_create_nil();
}

/**
 * @brief SageLang native function: gpio_read(pin)
 * Read GPIO pin input state
 * 
 * Usage in SageLang:
 *   let state = gpio_read(15);  // Read pin 15
 */
static Value* sage_gpio_read(int argc, Value** args) {
    if (argc != 1) {
        fprintf(stderr, "gpio_read() requires 1 argument: pin\r\n");
        return value_create_boolean(false);
    }
    
    if (args[0]->type != VALUE_NUMBER) {
        fprintf(stderr, "gpio_read() argument type: (number)\r\n");
        return value_create_boolean(false);
    }
    
    int pin = (int)args[0]->as.number;
    bool state = gpio_hal_read(pin);
    
    return value_create_boolean(state);
}

/**
 * @brief SageLang native function: gpio_toggle(pin)
 * Toggle GPIO pin output state
 * 
 * Usage in SageLang:
 *   gpio_toggle(25);  // Toggle pin 25
 */
static Value* sage_gpio_toggle(int argc, Value** args) {
    if (argc != 1) {
        fprintf(stderr, "gpio_toggle() requires 1 argument: pin\r\n");
        return value_create_nil();
    }
    
    if (args[0]->type != VALUE_NUMBER) {
        fprintf(stderr, "gpio_toggle() argument type: (number)\r\n");
        return value_create_nil();
    }
    
    int pin = (int)args[0]->as.number;
    gpio_hal_toggle(pin);
    
    return value_create_nil();
}

/**
 * @brief SageLang native function: gpio_set_pull(pin, mode)
 * Set GPIO pull resistor mode
 * 
 * Usage in SageLang:
 *   gpio_set_pull(15, 1);  // Pull-up
 *   gpio_set_pull(15, 2);  // Pull-down
 *   gpio_set_pull(15, 0);  // No pull
 */
static Value* sage_gpio_set_pull(int argc, Value** args) {
    if (argc != 2) {
        fprintf(stderr, "gpio_set_pull() requires 2 arguments: pin, mode\r\n");
        return value_create_nil();
    }
    
    if (args[0]->type != VALUE_NUMBER || args[1]->type != VALUE_NUMBER) {
        fprintf(stderr, "gpio_set_pull() argument types: (number, number)\r\n");
        return value_create_nil();
    }
    
    int pin = (int)args[0]->as.number;
    int mode = (int)args[1]->as.number;
    
    gpio_pull_t pull;
    switch (mode) {
        case 0: pull = GPIO_PULL_NONE; break;
        case 1: pull = GPIO_PULL_UP; break;
        case 2: pull = GPIO_PULL_DOWN; break;
        default:
            fprintf(stderr, "gpio_set_pull() invalid mode %d (use 0=none, 1=up, 2=down)\r\n", mode);
            return value_create_nil();
    }
    
    gpio_hal_set_pull(pin, pull);
    
    return value_create_nil();
}

/**
 * @brief Register all GPIO native functions with SageLang environment
 * Call this during SageLang initialization
 */
void sage_register_gpio_functions(Env* env) {
    if (!env) return;
    
    // Register GPIO functions
    env_define_native(env, "gpio_init", sage_gpio_init);
    env_define_native(env, "gpio_write", sage_gpio_write);
    env_define_native(env, "gpio_read", sage_gpio_read);
    env_define_native(env, "gpio_toggle", sage_gpio_toggle);
    env_define_native(env, "gpio_set_pull", sage_gpio_set_pull);
    
    printf("GPIO: Registered 5 native functions\r\n");
}
