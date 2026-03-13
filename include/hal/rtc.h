/* rtc.h - External RTC Hardware Abstraction Layer for littleOS
 *
 * RP2040/RP2350 have no onboard RTC.  This driver talks to common
 * I2C real-time clock chips (DS3231, PCF8563) via the existing I2C HAL.
 */
#ifndef LITTLEOS_HAL_RTC_H
#define LITTLEOS_HAL_RTC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Supported RTC chips */
typedef enum {
    RTC_CHIP_NONE = 0,
    RTC_CHIP_DS3231,
    RTC_CHIP_PCF8563,
} rtc_chip_t;

/* I2C addresses */
#define RTC_DS3231_ADDR     0x68
#define RTC_PCF8563_ADDR    0x51

/* Date/time structure (BCD-free, all binary) */
typedef struct {
    uint16_t year;      /* 2000-2099 */
    uint8_t  month;     /* 1-12 */
    uint8_t  day;       /* 1-31 */
    uint8_t  hour;      /* 0-23 */
    uint8_t  minute;    /* 0-59 */
    uint8_t  second;    /* 0-59 */
    uint8_t  weekday;   /* 0=Sunday .. 6=Saturday */
} rtc_datetime_t;

/* Alarm match flags (bitwise OR) */
#define RTC_ALARM_MATCH_SEC   (1 << 0)
#define RTC_ALARM_MATCH_MIN   (1 << 1)
#define RTC_ALARM_MATCH_HOUR  (1 << 2)
#define RTC_ALARM_MATCH_DAY   (1 << 3)

/* DS3231 temperature (0.25 C resolution) */
typedef struct {
    int8_t   integer;   /* whole degrees C */
    uint8_t  frac_25;   /* 0, 25, 50, or 75 (hundredths) */
} rtc_temperature_t;

/* ---- Lifecycle ---- */

/* Probe the I2C bus for a known RTC chip.
 * i2c_instance: 0 or 1          sda/scl: GPIO pins
 * Returns detected chip type, or RTC_CHIP_NONE. */
rtc_chip_t rtc_probe(uint8_t i2c_instance, uint8_t sda_pin, uint8_t scl_pin);

/* Initialise the RTC driver with a specific chip.
 * Call rtc_probe() first, or pass the chip type directly if known. */
int rtc_init(uint8_t i2c_instance, uint8_t sda_pin, uint8_t scl_pin,
             rtc_chip_t chip);

/* Check if the RTC driver is initialised and a chip is present. */
bool rtc_is_ready(void);

/* Return which chip is in use. */
rtc_chip_t rtc_get_chip(void);

/* ---- Time ---- */

/* Read current date/time from the RTC. */
int rtc_get_datetime(rtc_datetime_t *dt);

/* Set the RTC date/time. */
int rtc_set_datetime(const rtc_datetime_t *dt);

/* Convenience: seconds since 2000-01-01 00:00:00 (epoch for embedded). */
uint32_t rtc_datetime_to_epoch(const rtc_datetime_t *dt);
void     rtc_epoch_to_datetime(uint32_t epoch, rtc_datetime_t *dt);

/* ---- DS3231 extras ---- */

/* Read the DS3231 on-die temperature sensor. */
int rtc_ds3231_get_temperature(rtc_temperature_t *temp);

/* Check if the DS3231 oscillator-stop flag is set (power was lost). */
bool rtc_ds3231_oscillator_stopped(void);

/* Clear the oscillator-stop flag after setting the time. */
int rtc_ds3231_clear_osc_flag(void);

/* ---- Alarm (DS3231 Alarm 1 / PCF8563 alarm) ---- */

int  rtc_set_alarm(const rtc_datetime_t *when, uint8_t match_flags);
int  rtc_clear_alarm(void);
bool rtc_alarm_fired(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_RTC_H */
