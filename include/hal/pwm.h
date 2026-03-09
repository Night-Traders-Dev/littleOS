/* pwm.h - PWM Hardware Abstraction Layer for littleOS */
#ifndef LITTLEOS_HAL_PWM_H
#define LITTLEOS_HAL_PWM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RP2040 has 8 PWM slices, each with 2 channels (A and B)
 * GPIO pin → slice mapping: slice = (pin >> 1) & 7
 * Channel: A = even pin, B = odd pin */

#define PWM_NUM_SLICES      8
#define PWM_CHANNELS_PER    2

typedef struct {
    uint8_t     gpio_pin;       /* GPIO pin */
    uint8_t     slice;          /* PWM slice (auto-determined from pin) */
    uint8_t     channel;        /* Channel A(0) or B(1) */
    uint32_t    frequency_hz;   /* Desired frequency */
    uint16_t    duty_cycle;     /* Duty cycle 0-10000 (0.00% - 100.00%) */
    uint16_t    wrap;           /* Counter wrap value */
    float       clkdiv;         /* Clock divider */
    bool        initialized;
    bool        enabled;
} pwm_config_t;

/* Initialize PWM on a GPIO pin */
int pwm_hal_init(uint8_t gpio_pin, uint32_t frequency_hz, uint16_t duty_percent_x100);

/* Deinitialize PWM on a pin */
int pwm_hal_deinit(uint8_t gpio_pin);

/* Set duty cycle (0-10000 = 0.00%-100.00%) */
int pwm_hal_set_duty(uint8_t gpio_pin, uint16_t duty_percent_x100);

/* Set frequency (reconfigures slice) */
int pwm_hal_set_frequency(uint8_t gpio_pin, uint32_t frequency_hz);

/* Enable/disable PWM output */
int pwm_hal_enable(uint8_t gpio_pin, bool enable);

/* Get current configuration */
bool pwm_hal_get_config(uint8_t gpio_pin, pwm_config_t *config);

/* Get PWM slice and channel for a pin */
void pwm_hal_get_slice_channel(uint8_t gpio_pin, uint8_t *slice, uint8_t *channel);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_PWM_H */
