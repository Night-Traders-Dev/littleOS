// include/hal/gpio.h
// GPIO Hardware Abstraction Layer for littleOS
#ifndef LITTLEOS_HAL_GPIO_H
#define LITTLEOS_HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO pin direction
 */
typedef enum {
    GPIO_DIR_IN = 0,
    GPIO_DIR_OUT = 1
} gpio_direction_t;

/**
 * @brief GPIO pull mode
 */
typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP = 1,
    GPIO_PULL_DOWN = 2
} gpio_pull_t;

/**
 * @brief Initialize a GPIO pin
 * @param pin GPIO pin number (0-29 for RP2040)
 * @param dir Pin direction (GPIO_DIR_IN or GPIO_DIR_OUT)
 * @return true on success, false on error
 */
bool gpio_hal_init(uint8_t pin, gpio_direction_t dir);

/**
 * @brief Write value to GPIO pin
 * @param pin GPIO pin number
 * @param value Pin value (true = high, false = low)
 */
void gpio_hal_write(uint8_t pin, bool value);

/**
 * @brief Read value from GPIO pin
 * @param pin GPIO pin number
 * @return Pin value (true = high, false = low)
 */
bool gpio_hal_read(uint8_t pin);

/**
 * @brief Toggle GPIO pin state
 * @param pin GPIO pin number
 */
void gpio_hal_toggle(uint8_t pin);

/**
 * @brief Set GPIO pull resistor
 * @param pin GPIO pin number
 * @param pull Pull mode (GPIO_PULL_UP, GPIO_PULL_DOWN, or GPIO_PULL_NONE)
 */
void gpio_hal_set_pull(uint8_t pin, gpio_pull_t pull);

/**
 * @brief Get valid GPIO pin range for platform
 * @param min_pin Pointer to store minimum pin number
 * @param max_pin Pointer to store maximum pin number
 */
void gpio_hal_get_pin_range(uint8_t* min_pin, uint8_t* max_pin);

#ifdef __cplusplus
}
#endif

#endif // LITTLEOS_HAL_GPIO_H
