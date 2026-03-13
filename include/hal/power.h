/* power.h - Power Management HAL for littleOS */
#ifndef LITTLEOS_HAL_POWER_H
#define LITTLEOS_HAL_POWER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sleep modes */
typedef enum {
    POWER_MODE_ACTIVE = 0,    /* Normal operation */
    POWER_MODE_LIGHT_SLEEP,   /* WFI - wake on any interrupt */
    POWER_MODE_DEEP_SLEEP,    /* Dormant - clocks stopped */
    POWER_MODE_DORMANT,       /* Full dormant - XOSC/ROSC stopped */
} power_mode_t;

/* Wake sources */
typedef enum {
    WAKE_SOURCE_GPIO    = (1 << 0),
    WAKE_SOURCE_RTC     = (1 << 1),
    WAKE_SOURCE_UART    = (1 << 2),
    WAKE_SOURCE_USB     = (1 << 3),
    WAKE_SOURCE_TIMER   = (1 << 4),
} wake_source_t;

/* GPIO wake edge */
typedef enum {
    WAKE_EDGE_RISING  = 0,
    WAKE_EDGE_FALLING = 1,
    WAKE_EDGE_BOTH    = 2,
    WAKE_EDGE_HIGH    = 3,
    WAKE_EDGE_LOW     = 4,
} wake_edge_t;

/* Clock frequency presets (kHz) */
typedef enum {
#if PICO_RP2350
    POWER_CLOCK_FULL    = 150000,   /* 150 MHz (RP2350 default) */
    POWER_CLOCK_HALF    = 75000,    /* 75 MHz */
    POWER_CLOCK_QUARTER = 37500,    /* 37.5 MHz */
#else
    POWER_CLOCK_FULL    = 125000,   /* 125 MHz (RP2040 default) */
    POWER_CLOCK_HALF    = 62500,    /* 62.5 MHz */
    POWER_CLOCK_QUARTER = 31250,    /* 31.25 MHz */
#endif
    POWER_CLOCK_LOW     = 12000,    /* 12 MHz */
    POWER_CLOCK_ULTRA   = 6000,     /* 6 MHz */
} power_clock_preset_t;

/* Power status */
typedef struct {
    power_mode_t current_mode;
    uint32_t     sys_clock_khz;     /* Current system clock */
    float        core_voltage;       /* VREG voltage */
    float        vbus_voltage;       /* USB VBUS voltage (if available) */
    float        die_temp_c;         /* Die temperature */
    uint32_t     uptime_ms;
    uint32_t     total_sleep_ms;     /* Cumulative time in sleep modes */
    uint32_t     wake_count;         /* Number of wake events */
    uint32_t     last_wake_source;   /* What woke us last */
} power_status_t;

/* Init power management */
int power_init(void);

/* Enter a sleep mode */
int power_sleep(power_mode_t mode);

/* Sleep for a duration (light sleep with timer wake) */
int power_sleep_ms(uint32_t duration_ms);

/* Sleep until GPIO event */
int power_sleep_until_gpio(uint8_t gpio_pin, wake_edge_t edge);

/* Set system clock frequency */
int power_set_clock(uint32_t freq_khz);

/* Get current clock frequency */
uint32_t power_get_clock(void);

/* Set core voltage (0.80V to 1.30V in 0.05V steps) */
int power_set_voltage(float voltage);

/* Get power status */
int power_get_status(power_status_t *status);

/* Enable/disable peripherals for power saving */
int power_disable_peripheral(uint8_t peripheral_id);
int power_enable_peripheral(uint8_t peripheral_id);

/* Peripheral IDs for power_disable/enable_peripheral */
#define POWER_PERIPH_ADC        0
#define POWER_PERIPH_I2C0       1
#define POWER_PERIPH_I2C1       2
#define POWER_PERIPH_SPI0       3
#define POWER_PERIPH_SPI1       4
#define POWER_PERIPH_UART0      5
#define POWER_PERIPH_UART1      6
#define POWER_PERIPH_PWM        7
#define POWER_PERIPH_PIO0       8
#define POWER_PERIPH_PIO1       9
#define POWER_PERIPH_USB        10

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_POWER_H */
