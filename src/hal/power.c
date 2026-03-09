/* power.c - Power Management HAL Implementation for RP2040 */
#include "hal/power.h"
#include "dmesg.h"
#include <string.h>
#include <stdio.h>

#ifdef PICO_BUILD
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/pll.h"
#include "hardware/xosc.h"
#if __has_include("hardware/rosc.h")
#include "hardware/rosc.h"
#define HAS_ROSC 1
#else
#define HAS_ROSC 0
#endif
#include "hardware/resets.h"
#include "hardware/structs/scb.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

/* Try to include pico/sleep.h for dormant support; fall back to registers */
#if __has_include("pico/sleep.h")
#include "pico/sleep.h"
#define HAS_PICO_SLEEP 1
#else
#define HAS_PICO_SLEEP 0
#endif

/* RP2040 register addresses for direct dormant access */
#define XOSC_BASE       0x40024000
#define XOSC_DORMANT    (*(volatile uint32_t *)(XOSC_BASE + 0x08))
#define XOSC_STATUS     (*(volatile uint32_t *)(XOSC_BASE + 0x04))
#define XOSC_DORMANT_VALUE  0x636f6d61  /* "coma" in ASCII */
#define XOSC_WAKE_VALUE     0x77616b65  /* "wake" in ASCII */

#define ROSC_BASE       0x40060000
#define ROSC_DORMANT    (*(volatile uint32_t *)(ROSC_BASE + 0x0c))
#define ROSC_DORMANT_VALUE  0x636f6d61  /* "coma" */

/* VREG voltage enum mapping (from SDK) */
#define VREG_VOLTAGE_0_85  0x05
#define VREG_VOLTAGE_0_90  0x06
#define VREG_VOLTAGE_0_95  0x07
#define VREG_VOLTAGE_1_00  0x08
#define VREG_VOLTAGE_1_05  0x09
#define VREG_VOLTAGE_1_10  0x0a
#define VREG_VOLTAGE_1_15  0x0b
#define VREG_VOLTAGE_1_20  0x0c
#define VREG_VOLTAGE_1_25  0x0d
#define VREG_VOLTAGE_1_30  0x0e

/* Peripheral reset bit mapping (from RP2040 datasheet Table 2) */
static const uint32_t periph_reset_bits[] = {
    [POWER_PERIPH_ADC]   = (1 << 0),   /* RESET_ADC */
    [POWER_PERIPH_I2C0]  = (1 << 3),   /* RESET_I2C0 */
    [POWER_PERIPH_I2C1]  = (1 << 4),   /* RESET_I2C1 */
    [POWER_PERIPH_SPI0]  = (1 << 16),  /* RESET_SPI0 */
    [POWER_PERIPH_SPI1]  = (1 << 17),  /* RESET_SPI1 */
    [POWER_PERIPH_UART0] = (1 << 22),  /* RESET_UART0 */
    [POWER_PERIPH_UART1] = (1 << 23),  /* RESET_UART1 */
    [POWER_PERIPH_PWM]   = (1 << 14),  /* RESET_PWM */
    [POWER_PERIPH_PIO0]  = (1 << 10),  /* RESET_PIO0 */
    [POWER_PERIPH_PIO1]  = (1 << 11),  /* RESET_PIO1 */
    [POWER_PERIPH_USB]   = (1 << 24),  /* RESET_USBCTRL */
};

#endif /* PICO_BUILD */

/* Maximum number of peripherals */
#define POWER_PERIPH_MAX 11

/* Static state */
static power_mode_t s_current_mode    = POWER_MODE_ACTIVE;
static uint32_t     s_current_clock_khz = 125000;
static float        s_current_voltage = 1.10f;
static uint32_t     s_total_sleep_ms  = 0;
static uint32_t     s_wake_count      = 0;
static uint32_t     s_last_wake_source = 0;
static bool         s_initialized     = false;

/* Track which peripherals are disabled */
static bool s_periph_disabled[POWER_PERIPH_MAX];

#ifdef PICO_BUILD
/* Timer alarm callback - does nothing, just wakes from WFI */
static void power_timer_alarm_cb(uint alarm_num) {
    (void)alarm_num;
    /* Intentionally empty - the interrupt itself wakes us from WFI */
}

/* GPIO wake interrupt callback */
static volatile bool s_gpio_woke = false;
static void power_gpio_wake_cb(uint gpio, uint32_t events) {
    (void)gpio;
    (void)events;
    s_gpio_woke = true;
}

/* Get current time in ms */
static uint32_t power_get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

/* Map float voltage to VREG enum */
static uint8_t voltage_to_vreg_enum(float v) {
    if (v <= 0.85f) return VREG_VOLTAGE_0_85;
    if (v <= 0.90f) return VREG_VOLTAGE_0_90;
    if (v <= 0.95f) return VREG_VOLTAGE_0_95;
    if (v <= 1.00f) return VREG_VOLTAGE_1_00;
    if (v <= 1.05f) return VREG_VOLTAGE_1_05;
    if (v <= 1.10f) return VREG_VOLTAGE_1_10;
    if (v <= 1.15f) return VREG_VOLTAGE_1_15;
    if (v <= 1.20f) return VREG_VOLTAGE_1_20;
    if (v <= 1.25f) return VREG_VOLTAGE_1_25;
    return VREG_VOLTAGE_1_30;
}

/* Read die temperature from ADC channel 4 */
static float power_read_temp(void) {
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / (1 << 12);
    float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temp;
}
#endif /* PICO_BUILD */

int power_init(void) {
    if (s_initialized) {
        dmesg_debug("power: already initialized");
        return 0;
    }

    memset(s_periph_disabled, 0, sizeof(s_periph_disabled));
    s_total_sleep_ms   = 0;
    s_wake_count       = 0;
    s_last_wake_source = 0;
    s_current_mode     = POWER_MODE_ACTIVE;

#ifdef PICO_BUILD
    s_current_clock_khz = clock_get_hz(clk_sys) / 1000;
    s_current_voltage   = 1.10f; /* Default RP2040 VREG voltage */
#else
    s_current_clock_khz = 125000;
    s_current_voltage   = 1.10f;
#endif

    s_initialized = true;
    dmesg_info("power: initialized, sys_clk=%lu kHz", (unsigned long)s_current_clock_khz);

    return 0;
}

int power_sleep(power_mode_t mode) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

    if (mode == POWER_MODE_ACTIVE) {
        return 0;
    }

#ifdef PICO_BUILD
    uint32_t sleep_start = power_get_time_ms();
    s_current_mode = mode;

    switch (mode) {
    case POWER_MODE_LIGHT_SLEEP:
        dmesg_info("power: entering light sleep (WFI)");
        __wfi();
        break;

    case POWER_MODE_DEEP_SLEEP: {
        dmesg_info("power: entering deep sleep");
        /* Set SLEEPDEEP bit in SCB for deeper sleep on WFI */
        scb_hw->scr |= M0PLUS_SCR_SLEEPDEEP_BITS;
        __wfi();
        /* Clear SLEEPDEEP on wake */
        scb_hw->scr &= ~M0PLUS_SCR_SLEEPDEEP_BITS;
        break;
    }

    case POWER_MODE_DORMANT: {
        dmesg_info("power: entering dormant mode (XOSC stop)");
        /* Save current clock configuration */
        uint32_t saved_clock = s_current_clock_khz;

#if HAS_PICO_SLEEP
        /* Use SDK sleep functions if available */
        sleep_run_from_rosc();
        xosc_dormant();
        /* On wake: restart XOSC and reconfigure clocks */
        xosc_init();
        set_sys_clock_khz(saved_clock, true);
#else
        /* Direct register access fallback */
        /* Switch system clock to ROSC before stopping XOSC */
        clock_configure(clk_sys,
                        CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                        CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_ROSC_CLKSRC,
                        6500 * 1000, 6500 * 1000);

        /* Put XOSC into dormant mode */
        XOSC_DORMANT = XOSC_DORMANT_VALUE;

        /* Execution resumes here after wake event */
        /* Restart XOSC */
        xosc_init();
        /* Restore system clock */
        set_sys_clock_khz(saved_clock, true);
#endif
        dmesg_info("power: woke from dormant, restoring clocks to %lu kHz",
                   (unsigned long)saved_clock);
        break;
    }

    default:
        dmesg_err("power: invalid sleep mode %d", mode);
        s_current_mode = POWER_MODE_ACTIVE;
        return -1;
    }

    /* Calculate time spent sleeping */
    uint32_t sleep_end = power_get_time_ms();
    uint32_t elapsed = sleep_end - sleep_start;
    s_total_sleep_ms += elapsed;
    s_wake_count++;
    s_current_mode = POWER_MODE_ACTIVE;
    dmesg_debug("power: woke after %lu ms (total sleep: %lu ms, wakes: %lu)",
                (unsigned long)elapsed, (unsigned long)s_total_sleep_ms,
                (unsigned long)s_wake_count);

#else
    /* Non-PICO_BUILD stub */
    (void)mode;
    dmesg_info("power: sleep mode %d (stub, no-op)", mode);
    s_wake_count++;
    s_current_mode = POWER_MODE_ACTIVE;
#endif

    return 0;
}

int power_sleep_ms(uint32_t duration_ms) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

    if (duration_ms == 0) {
        return 0;
    }

#ifdef PICO_BUILD
    dmesg_info("power: timed sleep for %lu ms", (unsigned long)duration_ms);

    uint32_t sleep_start = power_get_time_ms();
    s_current_mode = POWER_MODE_LIGHT_SLEEP;
    s_last_wake_source = WAKE_SOURCE_TIMER;

    /* Set up a hardware alarm to wake us */
    int alarm_id = hardware_alarm_claim_unused(false);
    if (alarm_id >= 0) {
        hardware_alarm_set_callback(alarm_id, power_timer_alarm_cb);
        absolute_time_t target = make_timeout_time_ms(duration_ms);
        hardware_alarm_set_target(alarm_id, target);

        /* Enter WFI - will wake on the alarm interrupt (or any other interrupt) */
        __wfi();

        /* Clean up the alarm */
        hardware_alarm_cancel(alarm_id);
        hardware_alarm_set_callback(alarm_id, NULL);
        hardware_alarm_unclaim(alarm_id);
    } else {
        /* Fallback: busy-wait sleep if no alarm available */
        dmesg_warn("power: no hardware alarm available, using busy wait");
        sleep_ms(duration_ms);
    }

    uint32_t sleep_end = power_get_time_ms();
    uint32_t elapsed = sleep_end - sleep_start;
    s_total_sleep_ms += elapsed;
    s_wake_count++;
    s_current_mode = POWER_MODE_ACTIVE;

    dmesg_debug("power: timed sleep done, slept %lu ms", (unsigned long)elapsed);
#else
    (void)duration_ms;
    dmesg_info("power: timed sleep %lu ms (stub)", (unsigned long)duration_ms);
    s_wake_count++;
    s_last_wake_source = WAKE_SOURCE_TIMER;
#endif

    return 0;
}

int power_sleep_until_gpio(uint8_t gpio_pin, wake_edge_t edge) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

#ifdef PICO_BUILD
    if (gpio_pin > 29) {
        dmesg_err("power: invalid GPIO pin %u", gpio_pin);
        return -1;
    }

    dmesg_info("power: sleeping until GPIO%u event (edge=%d)", gpio_pin, edge);

    /* Map wake_edge_t to SDK GPIO event mask */
    uint32_t event_mask = 0;
    switch (edge) {
    case WAKE_EDGE_RISING:  event_mask = GPIO_IRQ_EDGE_RISE;  break;
    case WAKE_EDGE_FALLING: event_mask = GPIO_IRQ_EDGE_FALL;  break;
    case WAKE_EDGE_BOTH:    event_mask = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL; break;
    case WAKE_EDGE_HIGH:    event_mask = GPIO_IRQ_LEVEL_HIGH;  break;
    case WAKE_EDGE_LOW:     event_mask = GPIO_IRQ_LEVEL_LOW;   break;
    default:
        dmesg_err("power: invalid edge type %d", edge);
        return -1;
    }

    uint32_t sleep_start = power_get_time_ms();
    s_current_mode = POWER_MODE_LIGHT_SLEEP;
    s_gpio_woke = false;

    /* Initialize pin as input with pull-up if waiting for falling edge */
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_IN);
    if (edge == WAKE_EDGE_FALLING || edge == WAKE_EDGE_LOW) {
        gpio_pull_up(gpio_pin);
    } else if (edge == WAKE_EDGE_RISING || edge == WAKE_EDGE_HIGH) {
        gpio_pull_down(gpio_pin);
    }

    /* Enable GPIO interrupt */
    gpio_set_irq_enabled_with_callback(gpio_pin, event_mask, true, power_gpio_wake_cb);

    /* Enter WFI - will wake when GPIO interrupt fires */
    while (!s_gpio_woke) {
        __wfi();
    }

    /* Disable the GPIO interrupt */
    gpio_set_irq_enabled(gpio_pin, event_mask, false);

    uint32_t sleep_end = power_get_time_ms();
    uint32_t elapsed = sleep_end - sleep_start;
    s_total_sleep_ms += elapsed;
    s_wake_count++;
    s_last_wake_source = WAKE_SOURCE_GPIO;
    s_current_mode = POWER_MODE_ACTIVE;

    dmesg_info("power: GPIO%u wake after %lu ms", gpio_pin, (unsigned long)elapsed);
#else
    (void)gpio_pin;
    (void)edge;
    dmesg_info("power: gpio sleep (stub)");
    s_wake_count++;
    s_last_wake_source = WAKE_SOURCE_GPIO;
#endif

    return 0;
}

int power_set_clock(uint32_t freq_khz) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

    if (freq_khz == 0 || freq_khz > 133000) {
        dmesg_err("power: invalid clock frequency %lu kHz (range: 1-133000)",
                  (unsigned long)freq_khz);
        return -1;
    }

#ifdef PICO_BUILD
    dmesg_info("power: setting system clock to %lu kHz", (unsigned long)freq_khz);

    /* Adjust voltage before changing clock if needed */
    if (freq_khz > 125000 && s_current_voltage < 1.15f) {
        /* High frequencies need higher voltage */
        vreg_set_voltage(voltage_to_vreg_enum(1.15f));
        sleep_ms(10); /* Let voltage stabilize */
        s_current_voltage = 1.15f;
        dmesg_info("power: raised VREG voltage to 1.15V for high clock");
    }

    bool ok = set_sys_clock_khz(freq_khz, false);
    if (!ok) {
        dmesg_err("power: failed to set clock to %lu kHz (PLL cannot lock)",
                  (unsigned long)freq_khz);
        return -1;
    }

    /* Update UART baud rate after clock change for stdio */
    stdio_init_all();

    s_current_clock_khz = clock_get_hz(clk_sys) / 1000;

    /* If clock is low enough, we can reduce voltage to save power */
    if (freq_khz <= 48000 && s_current_voltage > 0.95f) {
        vreg_set_voltage(voltage_to_vreg_enum(0.95f));
        sleep_ms(10);
        s_current_voltage = 0.95f;
        dmesg_info("power: lowered VREG voltage to 0.95V for low clock");
    }

    dmesg_info("power: clock set to %lu kHz", (unsigned long)s_current_clock_khz);
#else
    s_current_clock_khz = freq_khz;
    dmesg_info("power: clock set to %lu kHz (stub)", (unsigned long)freq_khz);
#endif

    return 0;
}

uint32_t power_get_clock(void) {
#ifdef PICO_BUILD
    if (s_initialized) {
        s_current_clock_khz = clock_get_hz(clk_sys) / 1000;
    }
#endif
    return s_current_clock_khz;
}

int power_set_voltage(float voltage) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

    if (voltage < 0.80f || voltage > 1.30f) {
        dmesg_err("power: voltage %.2fV out of range (0.80-1.30V)", (double)voltage);
        return -1;
    }

#ifdef PICO_BUILD
    dmesg_info("power: setting VREG voltage to %.2fV", (double)voltage);

    /* Safety check: don't allow voltage too low for current clock speed */
    if (s_current_clock_khz > 48000 && voltage < 0.95f) {
        dmesg_warn("power: voltage %.2fV too low for %lu kHz, minimum 0.95V",
                   (double)voltage, (unsigned long)s_current_clock_khz);
        return -1;
    }
    if (s_current_clock_khz > 125000 && voltage < 1.10f) {
        dmesg_warn("power: voltage %.2fV too low for %lu kHz, minimum 1.10V",
                   (double)voltage, (unsigned long)s_current_clock_khz);
        return -1;
    }

    uint8_t vreg_val = voltage_to_vreg_enum(voltage);
    vreg_set_voltage(vreg_val);
    sleep_ms(10); /* Allow voltage to stabilize */
    s_current_voltage = voltage;

    dmesg_info("power: VREG voltage set to %.2fV (reg=0x%02x)",
               (double)voltage, vreg_val);
#else
    s_current_voltage = voltage;
    dmesg_info("power: voltage set to %.2fV (stub)", (double)voltage);
#endif

    return 0;
}

int power_get_status(power_status_t *status) {
    if (!status) {
        return -1;
    }

    if (!s_initialized) {
        memset(status, 0, sizeof(*status));
        return -1;
    }

    status->current_mode    = s_current_mode;
    status->total_sleep_ms  = s_total_sleep_ms;
    status->wake_count      = s_wake_count;
    status->last_wake_source = s_last_wake_source;

#ifdef PICO_BUILD
    status->sys_clock_khz = clock_get_hz(clk_sys) / 1000;
    status->core_voltage  = s_current_voltage;
    status->uptime_ms     = power_get_time_ms();

    /* Read die temperature from ADC channel 4 */
    status->die_temp_c = power_read_temp();

    /* Check VBUS (USB power) via GPIO 24 if available */
    gpio_init(24);
    gpio_set_dir(24, GPIO_IN);
    bool vbus_present = gpio_get(24);
    status->vbus_voltage = vbus_present ? 5.0f : 0.0f;
#else
    status->sys_clock_khz = s_current_clock_khz;
    status->core_voltage  = s_current_voltage;
    status->die_temp_c    = 25.0f;
    status->vbus_voltage  = 5.0f;
    status->uptime_ms     = 0;
#endif

    return 0;
}

int power_disable_peripheral(uint8_t peripheral_id) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

    if (peripheral_id >= POWER_PERIPH_MAX) {
        dmesg_err("power: invalid peripheral id %u", peripheral_id);
        return -1;
    }

    if (s_periph_disabled[peripheral_id]) {
        dmesg_debug("power: peripheral %u already disabled", peripheral_id);
        return 0;
    }

#ifdef PICO_BUILD
    uint32_t reset_bit = periph_reset_bits[peripheral_id];
    /* Assert reset to disable the peripheral */
    resets_hw->reset |= reset_bit;
    dmesg_info("power: disabled peripheral %u (reset_bit=0x%08lx)",
               peripheral_id, (unsigned long)reset_bit);
#else
    dmesg_info("power: disabled peripheral %u (stub)", peripheral_id);
#endif

    s_periph_disabled[peripheral_id] = true;
    return 0;
}

int power_enable_peripheral(uint8_t peripheral_id) {
    if (!s_initialized) {
        dmesg_err("power: not initialized");
        return -1;
    }

    if (peripheral_id >= POWER_PERIPH_MAX) {
        dmesg_err("power: invalid peripheral id %u", peripheral_id);
        return -1;
    }

    if (!s_periph_disabled[peripheral_id]) {
        dmesg_debug("power: peripheral %u already enabled", peripheral_id);
        return 0;
    }

#ifdef PICO_BUILD
    uint32_t reset_bit = periph_reset_bits[peripheral_id];
    /* Deassert reset to enable the peripheral */
    resets_hw->reset &= ~reset_bit;
    /* Wait for peripheral to come out of reset */
    while (!(resets_hw->reset_done & reset_bit)) {
        tight_loop_contents();
    }
    dmesg_info("power: enabled peripheral %u (reset_bit=0x%08lx)",
               peripheral_id, (unsigned long)reset_bit);
#else
    dmesg_info("power: enabled peripheral %u (stub)", peripheral_id);
#endif

    s_periph_disabled[peripheral_id] = false;
    return 0;
}
