/* pwm.c - PWM Hardware Abstraction Layer Implementation for RP2040 */
#include "hal/pwm.h"
#include "dmesg.h"
#include <string.h>

#ifdef PICO_BUILD
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#endif

/* RP2040 GPIO range */
#define GPIO_MIN_PIN    0
#define GPIO_MAX_PIN    29
#define NUM_GPIO_PINS   30

/* Default wrap value for 0.01% duty cycle resolution */
#define PWM_DEFAULT_WRAP    10000

/* Maximum duty cycle value (100.00%) */
#define PWM_DUTY_MAX        10000

/* Static configuration indexed by GPIO pin */
static pwm_config_t pwm_configs[NUM_GPIO_PINS];

/**
 * @brief Validate GPIO pin number
 */
static bool is_valid_pin(uint8_t pin) {
    return (pin >= GPIO_MIN_PIN && pin <= GPIO_MAX_PIN);
}

/**
 * @brief Calculate clock divider for desired frequency
 */
static float calculate_clkdiv(uint32_t frequency_hz, uint16_t wrap) {
#ifdef PICO_BUILD
    uint32_t sys_clk = clock_get_hz(clk_sys);
    float divider = (float)sys_clk / ((float)frequency_hz * (float)(wrap + 1));

    /* Clamp to valid range: 1.0 to 255.9375 (8-bit integer + 4-bit fraction) */
    if (divider < 1.0f) {
        divider = 1.0f;
    } else if (divider > 255.0f) {
        divider = 255.0f;
    }

    return divider;
#else
    (void)frequency_hz;
    (void)wrap;
    return 1.0f;
#endif
}

int pwm_hal_init(uint8_t gpio_pin, uint32_t frequency_hz, uint16_t duty_percent_x100) {
    if (!is_valid_pin(gpio_pin)) {
        dmesg_err("pwm: invalid pin %u", gpio_pin);
        return -1;
    }

    if (frequency_hz == 0) {
        dmesg_err("pwm: frequency must be non-zero");
        return -1;
    }

    if (duty_percent_x100 > PWM_DUTY_MAX) {
        duty_percent_x100 = PWM_DUTY_MAX;
    }

    pwm_config_t *cfg = &pwm_configs[gpio_pin];
    cfg->gpio_pin = gpio_pin;
    cfg->wrap = PWM_DEFAULT_WRAP;
    cfg->frequency_hz = frequency_hz;
    cfg->duty_cycle = duty_percent_x100;
    cfg->clkdiv = calculate_clkdiv(frequency_hz, cfg->wrap);

#ifdef PICO_BUILD
    /* Set GPIO to PWM function */
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);

    /* Determine slice and channel from pin */
    uint slice = pwm_gpio_to_slice_num(gpio_pin);
    uint channel = pwm_gpio_to_channel(gpio_pin);

    cfg->slice = (uint8_t)slice;
    cfg->channel = (uint8_t)channel;

    /* Configure PWM slice */
    pwm_config pwm_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&pwm_cfg, cfg->clkdiv);
    pwm_config_set_wrap(&pwm_cfg, cfg->wrap);
    pwm_init(slice, &pwm_cfg, true);

    /* Set initial duty cycle */
    uint16_t level = (uint16_t)(((uint32_t)duty_percent_x100 * (uint32_t)cfg->wrap) / PWM_DUTY_MAX);
    pwm_set_chan_level(slice, channel, level);

    dmesg_info("pwm: pin %u slice %u ch %u freq=%lu div=%.2f duty=%u.%02u%%",
               gpio_pin, slice, channel, frequency_hz,
               (double)cfg->clkdiv,
               duty_percent_x100 / 100, duty_percent_x100 % 100);
#else
    cfg->slice = (gpio_pin >> 1) & 7;
    cfg->channel = gpio_pin & 1;
    dmesg_info("pwm: stub init pin %u freq=%lu duty=%u.%02u%%",
               gpio_pin, frequency_hz,
               duty_percent_x100 / 100, duty_percent_x100 % 100);
#endif

    cfg->initialized = true;
    cfg->enabled = true;

    return 0;
}

int pwm_hal_deinit(uint8_t gpio_pin) {
    if (!is_valid_pin(gpio_pin)) {
        return -1;
    }

    pwm_config_t *cfg = &pwm_configs[gpio_pin];
    if (!cfg->initialized) {
        return -1;
    }

#ifdef PICO_BUILD
    pwm_set_enabled(cfg->slice, false);
#endif

    dmesg_info("pwm: pin %u deinitialized", gpio_pin);
    memset(cfg, 0, sizeof(*cfg));
    return 0;
}

int pwm_hal_set_duty(uint8_t gpio_pin, uint16_t duty_percent_x100) {
    if (!is_valid_pin(gpio_pin)) {
        return -1;
    }

    pwm_config_t *cfg = &pwm_configs[gpio_pin];
    if (!cfg->initialized) {
        dmesg_err("pwm: set_duty on uninitialized pin %u", gpio_pin);
        return -1;
    }

    if (duty_percent_x100 > PWM_DUTY_MAX) {
        duty_percent_x100 = PWM_DUTY_MAX;
    }

    cfg->duty_cycle = duty_percent_x100;

#ifdef PICO_BUILD
    uint16_t level = (uint16_t)(((uint32_t)duty_percent_x100 * (uint32_t)cfg->wrap) / PWM_DUTY_MAX);
    pwm_set_chan_level(cfg->slice, cfg->channel, level);
#endif

    dmesg_debug("pwm: pin %u duty=%u.%02u%%", gpio_pin,
                duty_percent_x100 / 100, duty_percent_x100 % 100);

    return 0;
}

int pwm_hal_set_frequency(uint8_t gpio_pin, uint32_t frequency_hz) {
    if (!is_valid_pin(gpio_pin)) {
        return -1;
    }

    pwm_config_t *cfg = &pwm_configs[gpio_pin];
    if (!cfg->initialized) {
        dmesg_err("pwm: set_frequency on uninitialized pin %u", gpio_pin);
        return -1;
    }

    if (frequency_hz == 0) {
        return -1;
    }

    cfg->frequency_hz = frequency_hz;
    cfg->clkdiv = calculate_clkdiv(frequency_hz, cfg->wrap);

#ifdef PICO_BUILD
    /* Reconfigure the PWM slice with new divider */
    pwm_config pwm_cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&pwm_cfg, cfg->clkdiv);
    pwm_config_set_wrap(&pwm_cfg, cfg->wrap);
    pwm_init(cfg->slice, &pwm_cfg, cfg->enabled);

    /* Re-apply current duty cycle */
    uint16_t level = (uint16_t)(((uint32_t)cfg->duty_cycle * (uint32_t)cfg->wrap) / PWM_DUTY_MAX);
    pwm_set_chan_level(cfg->slice, cfg->channel, level);
#endif

    dmesg_info("pwm: pin %u freq changed to %lu div=%.2f",
               gpio_pin, frequency_hz, (double)cfg->clkdiv);

    return 0;
}

int pwm_hal_enable(uint8_t gpio_pin, bool enable) {
    if (!is_valid_pin(gpio_pin)) {
        return -1;
    }

    pwm_config_t *cfg = &pwm_configs[gpio_pin];
    if (!cfg->initialized) {
        return -1;
    }

#ifdef PICO_BUILD
    pwm_set_enabled(cfg->slice, enable);
#endif

    cfg->enabled = enable;
    dmesg_debug("pwm: pin %u %s", gpio_pin, enable ? "enabled" : "disabled");

    return 0;
}

bool pwm_hal_get_config(uint8_t gpio_pin, pwm_config_t *config) {
    if (!is_valid_pin(gpio_pin) || !config) {
        return false;
    }

    if (!pwm_configs[gpio_pin].initialized) {
        return false;
    }

    *config = pwm_configs[gpio_pin];
    return true;
}

void pwm_hal_get_slice_channel(uint8_t gpio_pin, uint8_t *slice, uint8_t *channel) {
    if (slice) {
        *slice = (gpio_pin >> 1) & 7;
    }
    if (channel) {
        *channel = gpio_pin & 1;
    }
}
