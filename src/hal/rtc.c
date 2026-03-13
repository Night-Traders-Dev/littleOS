/* rtc.c - External RTC driver for DS3231 and PCF8563
 *
 * Communicates with external I2C real-time clock chips using the
 * littleOS I2C HAL.  All BCD conversion is handled internally.
 */
#include "hal/rtc.h"
#include "hal/i2c.h"
#include <string.h>

/* ================================================================
 * Internal state
 * ================================================================ */

static struct {
    uint8_t    i2c_instance;
    uint8_t    addr;
    rtc_chip_t chip;
    bool       ready;
} rtc_state;

/* ================================================================
 * BCD helpers
 * ================================================================ */

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
}

static uint8_t bin_to_bcd(uint8_t bin) {
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

/* ================================================================
 * DS3231 register map
 * ================================================================ */

#define DS3231_REG_SEC      0x00
#define DS3231_REG_MIN      0x01
#define DS3231_REG_HOUR     0x02
#define DS3231_REG_DAY      0x03   /* day of week 1-7 */
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05   /* bit 7 = century */
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_A1_SEC   0x07
#define DS3231_REG_A1_MIN   0x08
#define DS3231_REG_A1_HOUR  0x09
#define DS3231_REG_A1_DAY   0x0A
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_TEMP_MSB 0x11
#define DS3231_REG_TEMP_LSB 0x12

#define DS3231_STATUS_OSF   0x80   /* oscillator stop flag */
#define DS3231_CTRL_A1IE    0x01   /* alarm 1 interrupt enable */
#define DS3231_CTRL_INTCN   0x04   /* interrupt control */
#define DS3231_STATUS_A1F   0x01   /* alarm 1 flag */

/* ================================================================
 * PCF8563 register map
 * ================================================================ */

#define PCF8563_REG_CTRL1      0x00
#define PCF8563_REG_CTRL2      0x01
#define PCF8563_REG_SEC        0x02
#define PCF8563_REG_MIN        0x03
#define PCF8563_REG_HOUR       0x04
#define PCF8563_REG_DAY        0x05
#define PCF8563_REG_WEEKDAY    0x06
#define PCF8563_REG_MONTH      0x07   /* bit 7 = century */
#define PCF8563_REG_YEAR       0x08
#define PCF8563_REG_ALARM_MIN  0x09
#define PCF8563_REG_ALARM_HOUR 0x0A
#define PCF8563_REG_ALARM_DAY  0x0B
#define PCF8563_REG_ALARM_WDAY 0x0C

#define PCF8563_SEC_VL         0x80   /* voltage-low (clock integrity) */
#define PCF8563_CTRL2_AIE      0x02   /* alarm interrupt enable */
#define PCF8563_CTRL2_AF       0x08   /* alarm flag */

/* ================================================================
 * I2C read/write wrappers
 * ================================================================ */

static int rtc_read_reg(uint8_t reg, uint8_t *val) {
    return i2c_hal_read_reg(rtc_state.i2c_instance, rtc_state.addr, reg, val);
}

static int rtc_write_reg(uint8_t reg, uint8_t val) {
    return i2c_hal_write_reg(rtc_state.i2c_instance, rtc_state.addr, reg, val);
}

static int rtc_read_burst(uint8_t start_reg, uint8_t *buf, size_t len) {
    return i2c_hal_write_read(rtc_state.i2c_instance, rtc_state.addr,
                              &start_reg, 1, buf, len);
}

/* ================================================================
 * DS3231 implementation
 * ================================================================ */

static int ds3231_get_datetime(rtc_datetime_t *dt) {
    uint8_t buf[7];
    int r = rtc_read_burst(DS3231_REG_SEC, buf, 7);
    if (r != 0) return -1;

    dt->second  = bcd_to_bin(buf[0] & 0x7F);
    dt->minute  = bcd_to_bin(buf[1] & 0x7F);
    dt->hour    = bcd_to_bin(buf[2] & 0x3F); /* 24h mode */
    dt->weekday = (buf[3] & 0x07) - 1;       /* DS3231: 1-7 → 0-6 */
    dt->day     = bcd_to_bin(buf[4] & 0x3F);
    dt->month   = bcd_to_bin(buf[5] & 0x1F);
    dt->year    = 2000 + bcd_to_bin(buf[6]);
    if (buf[5] & 0x80) dt->year += 100;      /* century bit */

    return 0;
}

static int ds3231_set_datetime(const rtc_datetime_t *dt) {
    uint8_t buf[8];
    uint16_t y = dt->year;
    uint8_t century = 0;
    if (y >= 2100) { y -= 100; century = 0x80; }

    buf[0] = DS3231_REG_SEC;             /* register address */
    buf[1] = bin_to_bcd(dt->second);
    buf[2] = bin_to_bcd(dt->minute);
    buf[3] = bin_to_bcd(dt->hour);       /* 24h mode */
    buf[4] = (dt->weekday + 1) & 0x07;   /* 0-6 → 1-7 */
    buf[5] = bin_to_bcd(dt->day);
    buf[6] = bin_to_bcd(dt->month) | century;
    buf[7] = bin_to_bcd((uint8_t)(y - 2000));

    return i2c_hal_write(rtc_state.i2c_instance, rtc_state.addr, buf, 8);
}

/* ================================================================
 * PCF8563 implementation
 * ================================================================ */

static int pcf8563_get_datetime(rtc_datetime_t *dt) {
    uint8_t buf[7];
    int r = rtc_read_burst(PCF8563_REG_SEC, buf, 7);
    if (r != 0) return -1;

    dt->second  = bcd_to_bin(buf[0] & 0x7F);
    dt->minute  = bcd_to_bin(buf[1] & 0x7F);
    dt->hour    = bcd_to_bin(buf[2] & 0x3F);
    dt->day     = bcd_to_bin(buf[3] & 0x3F);
    dt->weekday = buf[4] & 0x07;
    dt->month   = bcd_to_bin(buf[5] & 0x1F);
    dt->year    = 2000 + bcd_to_bin(buf[6]);
    if (buf[5] & 0x80) dt->year += 100;

    return 0;
}

static int pcf8563_set_datetime(const rtc_datetime_t *dt) {
    uint8_t buf[8];
    uint16_t y = dt->year;
    uint8_t century = 0;
    if (y >= 2100) { y -= 100; century = 0x80; }

    buf[0] = PCF8563_REG_SEC;             /* register address */
    buf[1] = bin_to_bcd(dt->second) & 0x7F;
    buf[2] = bin_to_bcd(dt->minute) & 0x7F;
    buf[3] = bin_to_bcd(dt->hour) & 0x3F;
    buf[4] = bin_to_bcd(dt->day) & 0x3F;
    buf[5] = dt->weekday & 0x07;
    buf[6] = bin_to_bcd(dt->month) | century;
    buf[7] = bin_to_bcd((uint8_t)(y - 2000));

    return i2c_hal_write(rtc_state.i2c_instance, rtc_state.addr, buf, 8);
}

/* ================================================================
 * Public API
 * ================================================================ */

rtc_chip_t rtc_probe(uint8_t i2c_instance, uint8_t sda_pin, uint8_t scl_pin) {
    /* Ensure the I2C bus is up */
    i2c_hal_init(i2c_instance, sda_pin, scl_pin, I2C_SPEED_STANDARD);

    uint8_t val;

    /* Try DS3231 — read seconds register */
    if (i2c_hal_read_reg(i2c_instance, RTC_DS3231_ADDR, 0x00, &val) == 0)
        return RTC_CHIP_DS3231;

    /* Try PCF8563 — read control register 1 */
    if (i2c_hal_read_reg(i2c_instance, RTC_PCF8563_ADDR, 0x00, &val) == 0)
        return RTC_CHIP_PCF8563;

    return RTC_CHIP_NONE;
}

int rtc_init(uint8_t i2c_instance, uint8_t sda_pin, uint8_t scl_pin,
             rtc_chip_t chip) {
    if (chip == RTC_CHIP_NONE) return -1;

    int r = i2c_hal_init(i2c_instance, sda_pin, scl_pin, I2C_SPEED_STANDARD);
    if (r != 0) return r;

    rtc_state.i2c_instance = i2c_instance;
    rtc_state.chip = chip;
    rtc_state.ready = true;

    if (chip == RTC_CHIP_DS3231) {
        rtc_state.addr = RTC_DS3231_ADDR;
        /* Ensure 24-hour mode, enable square-wave output off */
        rtc_write_reg(DS3231_REG_CONTROL, DS3231_CTRL_INTCN);
    } else {
        rtc_state.addr = RTC_PCF8563_ADDR;
        /* Clear stop bit, normal mode */
        rtc_write_reg(PCF8563_REG_CTRL1, 0x00);
        rtc_write_reg(PCF8563_REG_CTRL2, 0x00);
    }

    return 0;
}

bool rtc_is_ready(void) {
    return rtc_state.ready;
}

rtc_chip_t rtc_get_chip(void) {
    return rtc_state.chip;
}

int rtc_get_datetime(rtc_datetime_t *dt) {
    if (!rtc_state.ready || !dt) return -1;
    memset(dt, 0, sizeof(*dt));

    if (rtc_state.chip == RTC_CHIP_DS3231)
        return ds3231_get_datetime(dt);
    else
        return pcf8563_get_datetime(dt);
}

int rtc_set_datetime(const rtc_datetime_t *dt) {
    if (!rtc_state.ready || !dt) return -1;

    if (dt->month < 1 || dt->month > 12) return -1;
    if (dt->day < 1 || dt->day > 31)     return -1;
    if (dt->hour > 23 || dt->minute > 59 || dt->second > 59) return -1;

    if (rtc_state.chip == RTC_CHIP_DS3231)
        return ds3231_set_datetime(dt);
    else
        return pcf8563_set_datetime(dt);
}

/* ================================================================
 * Epoch conversion  (epoch = seconds since 2000-01-01 00:00:00)
 * ================================================================ */

static const uint16_t days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static bool is_leap(uint16_t y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

uint32_t rtc_datetime_to_epoch(const rtc_datetime_t *dt) {
    uint32_t days = 0;
    for (uint16_t y = 2000; y < dt->year; y++)
        days += is_leap(y) ? 366 : 365;

    days += days_before_month[dt->month - 1];
    if (dt->month > 2 && is_leap(dt->year)) days++;
    days += dt->day - 1;

    return days * 86400u + dt->hour * 3600u + dt->minute * 60u + dt->second;
}

void rtc_epoch_to_datetime(uint32_t epoch, rtc_datetime_t *dt) {
    uint32_t rem = epoch;
    uint16_t y = 2000;

    for (;;) {
        uint32_t ydays = is_leap(y) ? 366 : 365;
        if (rem < ydays * 86400u) break;
        rem -= ydays * 86400u;
        y++;
    }
    dt->year = y;

    uint32_t day_of_year = rem / 86400u;
    rem %= 86400u;
    dt->hour   = (uint8_t)(rem / 3600u);
    rem %= 3600u;
    dt->minute = (uint8_t)(rem / 60u);
    dt->second = (uint8_t)(rem % 60u);

    /* month/day from day_of_year */
    uint8_t m;
    for (m = 11; m > 0; m--) {
        uint16_t threshold = days_before_month[m];
        if (m >= 2 && is_leap(y)) threshold++;
        if (day_of_year >= threshold) {
            day_of_year -= threshold;
            break;
        }
    }
    dt->month = m + 1;
    dt->day   = (uint8_t)(day_of_year + 1);

    /* weekday: Zeller-like from epoch days */
    uint32_t total_days = epoch / 86400u;
    dt->weekday = (uint8_t)((total_days + 6) % 7); /* 2000-01-01 = Saturday */
}

/* ================================================================
 * DS3231 extras
 * ================================================================ */

int rtc_ds3231_get_temperature(rtc_temperature_t *temp) {
    if (!rtc_state.ready || rtc_state.chip != RTC_CHIP_DS3231 || !temp)
        return -1;

    uint8_t msb, lsb;
    int r = rtc_read_reg(DS3231_REG_TEMP_MSB, &msb);
    if (r != 0) return r;
    r = rtc_read_reg(DS3231_REG_TEMP_LSB, &lsb);
    if (r != 0) return r;

    temp->integer = (int8_t)msb;
    temp->frac_25 = (uint8_t)((lsb >> 6) * 25);
    return 0;
}

bool rtc_ds3231_oscillator_stopped(void) {
    if (!rtc_state.ready || rtc_state.chip != RTC_CHIP_DS3231)
        return true;

    uint8_t status;
    if (rtc_read_reg(DS3231_REG_STATUS, &status) != 0) return true;
    return (status & DS3231_STATUS_OSF) != 0;
}

int rtc_ds3231_clear_osc_flag(void) {
    if (!rtc_state.ready || rtc_state.chip != RTC_CHIP_DS3231)
        return -1;

    uint8_t status;
    int r = rtc_read_reg(DS3231_REG_STATUS, &status);
    if (r != 0) return r;
    return rtc_write_reg(DS3231_REG_STATUS, status & ~DS3231_STATUS_OSF);
}

/* ================================================================
 * Alarm
 * ================================================================ */

int rtc_set_alarm(const rtc_datetime_t *when, uint8_t match_flags) {
    if (!rtc_state.ready || !when) return -1;

    if (rtc_state.chip == RTC_CHIP_DS3231) {
        /* DS3231 Alarm 1 (seconds-level resolution)
         * A1M1-A1M4 bits control which fields must match.
         * bit 7 of each register: 1 = don't care */
        uint8_t a1m1 = (match_flags & RTC_ALARM_MATCH_SEC)  ? 0x00 : 0x80;
        uint8_t a1m2 = (match_flags & RTC_ALARM_MATCH_MIN)  ? 0x00 : 0x80;
        uint8_t a1m3 = (match_flags & RTC_ALARM_MATCH_HOUR) ? 0x00 : 0x80;
        uint8_t a1m4 = (match_flags & RTC_ALARM_MATCH_DAY)  ? 0x00 : 0x80;

        rtc_write_reg(DS3231_REG_A1_SEC,  bin_to_bcd(when->second) | a1m1);
        rtc_write_reg(DS3231_REG_A1_MIN,  bin_to_bcd(when->minute) | a1m2);
        rtc_write_reg(DS3231_REG_A1_HOUR, bin_to_bcd(when->hour)   | a1m3);
        rtc_write_reg(DS3231_REG_A1_DAY,  bin_to_bcd(when->day)    | a1m4);

        /* Enable alarm 1 interrupt */
        uint8_t ctrl;
        rtc_read_reg(DS3231_REG_CONTROL, &ctrl);
        ctrl |= DS3231_CTRL_A1IE | DS3231_CTRL_INTCN;
        rtc_write_reg(DS3231_REG_CONTROL, ctrl);

        /* Clear alarm flag */
        uint8_t status;
        rtc_read_reg(DS3231_REG_STATUS, &status);
        rtc_write_reg(DS3231_REG_STATUS, status & ~DS3231_STATUS_A1F);

        return 0;
    }

    if (rtc_state.chip == RTC_CHIP_PCF8563) {
        uint8_t dis_min  = (match_flags & RTC_ALARM_MATCH_MIN)  ? 0x00 : 0x80;
        uint8_t dis_hour = (match_flags & RTC_ALARM_MATCH_HOUR) ? 0x00 : 0x80;
        uint8_t dis_day  = (match_flags & RTC_ALARM_MATCH_DAY)  ? 0x00 : 0x80;

        rtc_write_reg(PCF8563_REG_ALARM_MIN,  bin_to_bcd(when->minute) | dis_min);
        rtc_write_reg(PCF8563_REG_ALARM_HOUR, bin_to_bcd(when->hour)   | dis_hour);
        rtc_write_reg(PCF8563_REG_ALARM_DAY,  bin_to_bcd(when->day)    | dis_day);
        rtc_write_reg(PCF8563_REG_ALARM_WDAY, 0x80); /* weekday: don't care */

        /* Enable alarm interrupt, clear flag */
        uint8_t ctrl2;
        rtc_read_reg(PCF8563_REG_CTRL2, &ctrl2);
        ctrl2 |= PCF8563_CTRL2_AIE;
        ctrl2 &= ~PCF8563_CTRL2_AF;
        rtc_write_reg(PCF8563_REG_CTRL2, ctrl2);

        return 0;
    }

    return -1;
}

int rtc_clear_alarm(void) {
    if (!rtc_state.ready) return -1;

    if (rtc_state.chip == RTC_CHIP_DS3231) {
        uint8_t ctrl;
        rtc_read_reg(DS3231_REG_CONTROL, &ctrl);
        ctrl &= ~DS3231_CTRL_A1IE;
        rtc_write_reg(DS3231_REG_CONTROL, ctrl);

        uint8_t status;
        rtc_read_reg(DS3231_REG_STATUS, &status);
        rtc_write_reg(DS3231_REG_STATUS, status & ~DS3231_STATUS_A1F);
        return 0;
    }

    if (rtc_state.chip == RTC_CHIP_PCF8563) {
        uint8_t ctrl2;
        rtc_read_reg(PCF8563_REG_CTRL2, &ctrl2);
        ctrl2 &= ~(PCF8563_CTRL2_AIE | PCF8563_CTRL2_AF);
        rtc_write_reg(PCF8563_REG_CTRL2, ctrl2);
        return 0;
    }

    return -1;
}

bool rtc_alarm_fired(void) {
    if (!rtc_state.ready) return false;

    uint8_t val;

    if (rtc_state.chip == RTC_CHIP_DS3231) {
        if (rtc_read_reg(DS3231_REG_STATUS, &val) != 0) return false;
        return (val & DS3231_STATUS_A1F) != 0;
    }

    if (rtc_state.chip == RTC_CHIP_PCF8563) {
        if (rtc_read_reg(PCF8563_REG_CTRL2, &val) != 0) return false;
        return (val & PCF8563_CTRL2_AF) != 0;
    }

    return false;
}
