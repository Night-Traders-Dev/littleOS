/* cmd_power.c - Power management shell commands for littleOS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal/power.h"

/* Peripheral name lookup table */
static const struct {
    const char *name;
    uint8_t     id;
} periph_names[] = {
    { "adc",   POWER_PERIPH_ADC   },
    { "i2c0",  POWER_PERIPH_I2C0  },
    { "i2c1",  POWER_PERIPH_I2C1  },
    { "spi0",  POWER_PERIPH_SPI0  },
    { "spi1",  POWER_PERIPH_SPI1  },
    { "uart0", POWER_PERIPH_UART0 },
    { "uart1", POWER_PERIPH_UART1 },
    { "pwm",   POWER_PERIPH_PWM   },
    { "pio0",  POWER_PERIPH_PIO0  },
    { "pio1",  POWER_PERIPH_PIO1  },
    { "usb",   POWER_PERIPH_USB   },
};

#define NUM_PERIPHS (sizeof(periph_names) / sizeof(periph_names[0]))

static void cmd_power_usage(void) {
    printf("Power management commands:\r\n");
    printf("  power status                       - Show power status\r\n");
    printf("  power sleep light                  - Enter light sleep (wake on interrupt)\r\n");
    printf("  power sleep <ms>                   - Sleep for duration in milliseconds\r\n");
    printf("  power sleep gpio <pin> rising|falling - Sleep until GPIO event\r\n");
    printf("  power clock <freq_khz>             - Set system clock (kHz)\r\n");
    printf("  power clock full|half|quarter|low|ultra - Set clock preset\r\n");
    printf("  power voltage <volts>              - Set core voltage (0.80-1.30V)\r\n");
    printf("  power periph disable|enable <name> - Disable/enable peripheral\r\n");
    printf("\r\n");
    printf("Clock presets: full=125MHz, half=62.5MHz, quarter=31.25MHz,\r\n");
    printf("               low=12MHz, ultra=6MHz\r\n");
    printf("Peripherals:   adc, i2c0, i2c1, spi0, spi1, uart0, uart1,\r\n");
    printf("               pwm, pio0, pio1, usb\r\n");
}

static const char *mode_name(power_mode_t mode) {
    switch (mode) {
    case POWER_MODE_ACTIVE:      return "active";
    case POWER_MODE_LIGHT_SLEEP: return "light_sleep";
    case POWER_MODE_DEEP_SLEEP:  return "deep_sleep";
    case POWER_MODE_DORMANT:     return "dormant";
    default:                     return "unknown";
    }
}

static const char *wake_source_name(uint32_t source) {
    if (source == 0)                    return "none";
    if (source & WAKE_SOURCE_GPIO)      return "gpio";
    if (source & WAKE_SOURCE_RTC)       return "rtc";
    if (source & WAKE_SOURCE_UART)      return "uart";
    if (source & WAKE_SOURCE_USB)       return "usb";
    if (source & WAKE_SOURCE_TIMER)     return "timer";
    return "unknown";
}

static int cmd_power_status(void) {
    power_status_t st;
    int r = power_get_status(&st);
    if (r != 0) {
        printf("Error: power subsystem not initialized. Run power_init() first.\r\n");
        return -1;
    }

    uint32_t uptime_s  = st.uptime_ms / 1000;
    uint32_t hours     = uptime_s / 3600;
    uint32_t minutes   = (uptime_s % 3600) / 60;
    uint32_t seconds   = uptime_s % 60;

    printf("=== Power Status ===\r\n");
    printf("  Mode:         %s\r\n", mode_name(st.current_mode));
    printf("  System clock: %lu kHz (%.1f MHz)\r\n",
           (unsigned long)st.sys_clock_khz,
           (double)st.sys_clock_khz / 1000.0);
    printf("  Core voltage: %.2f V\r\n", (double)st.core_voltage);
    printf("  VBUS voltage: %.1f V%s\r\n",
           (double)st.vbus_voltage,
           st.vbus_voltage > 0.0f ? " (USB powered)" : " (no USB)");
    printf("  Die temp:     %.1f C\r\n", (double)st.die_temp_c);
    printf("  Uptime:       %lu:%02lu:%02lu (%lu ms)\r\n",
           (unsigned long)hours, (unsigned long)minutes,
           (unsigned long)seconds, (unsigned long)st.uptime_ms);
    printf("  Total sleep:  %lu ms\r\n", (unsigned long)st.total_sleep_ms);
    printf("  Wake count:   %lu\r\n", (unsigned long)st.wake_count);
    printf("  Last wake:    %s\r\n", wake_source_name(st.last_wake_source));

    return 0;
}

static int cmd_power_sleep(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: power sleep light|<ms>|gpio <pin> rising|falling\r\n");
        return -1;
    }

    if (strcmp(argv[2], "light") == 0) {
        printf("Entering light sleep... (any interrupt will wake)\r\n");
        int r = power_sleep(POWER_MODE_LIGHT_SLEEP);
        if (r == 0) {
            printf("Woke from light sleep.\r\n");
        } else {
            printf("Error entering sleep: %d\r\n", r);
        }
        return r;
    }

    if (strcmp(argv[2], "gpio") == 0) {
        if (argc < 5) {
            printf("Usage: power sleep gpio <pin> rising|falling|both|high|low\r\n");
            return -1;
        }
        uint8_t pin = (uint8_t)atoi(argv[3]);
        if (pin > 29) {
            printf("Error: GPIO pin must be 0-29\r\n");
            return -1;
        }

        wake_edge_t edge;
        if (strcmp(argv[4], "rising") == 0) {
            edge = WAKE_EDGE_RISING;
        } else if (strcmp(argv[4], "falling") == 0) {
            edge = WAKE_EDGE_FALLING;
        } else if (strcmp(argv[4], "both") == 0) {
            edge = WAKE_EDGE_BOTH;
        } else if (strcmp(argv[4], "high") == 0) {
            edge = WAKE_EDGE_HIGH;
        } else if (strcmp(argv[4], "low") == 0) {
            edge = WAKE_EDGE_LOW;
        } else {
            printf("Error: unknown edge type '%s' (use rising|falling|both|high|low)\r\n",
                   argv[4]);
            return -1;
        }

        printf("Entering sleep, waiting for GPIO%u %s...\r\n", pin, argv[4]);
        int r = power_sleep_until_gpio(pin, edge);
        if (r == 0) {
            printf("Woke from GPIO sleep.\r\n");
        } else {
            printf("Error: %d\r\n", r);
        }
        return r;
    }

    /* Timed sleep: argument is milliseconds */
    uint32_t ms = (uint32_t)strtoul(argv[2], NULL, 10);
    if (ms == 0) {
        printf("Error: invalid sleep duration '%s'\r\n", argv[2]);
        return -1;
    }

    printf("Sleeping for %lu ms...\r\n", (unsigned long)ms);
    int r = power_sleep_ms(ms);
    if (r == 0) {
        printf("Woke after timed sleep.\r\n");
    } else {
        printf("Error: %d\r\n", r);
    }
    return r;
}

static int cmd_power_clock(int argc, char *argv[]) {
    if (argc < 3) {
        uint32_t clk = power_get_clock();
        printf("Current system clock: %lu kHz (%.1f MHz)\r\n",
               (unsigned long)clk, (double)clk / 1000.0);
        return 0;
    }

    uint32_t freq_khz = 0;

    if (strcmp(argv[2], "full") == 0) {
        freq_khz = POWER_CLOCK_FULL;
    } else if (strcmp(argv[2], "half") == 0) {
        freq_khz = POWER_CLOCK_HALF;
    } else if (strcmp(argv[2], "quarter") == 0) {
        freq_khz = POWER_CLOCK_QUARTER;
    } else if (strcmp(argv[2], "low") == 0) {
        freq_khz = POWER_CLOCK_LOW;
    } else if (strcmp(argv[2], "ultra") == 0) {
        freq_khz = POWER_CLOCK_ULTRA;
    } else {
        freq_khz = (uint32_t)strtoul(argv[2], NULL, 10);
        if (freq_khz == 0) {
            printf("Error: invalid frequency '%s'\r\n", argv[2]);
            return -1;
        }
    }

    printf("WARNING: Changing system clock may affect UART, timers, and peripherals.\r\n");
    printf("Setting clock to %lu kHz (%.1f MHz)...\r\n",
           (unsigned long)freq_khz, (double)freq_khz / 1000.0);

    int r = power_set_clock(freq_khz);
    if (r == 0) {
        printf("Clock set successfully.\r\n");
        uint32_t actual = power_get_clock();
        printf("Actual clock: %lu kHz (%.1f MHz)\r\n",
               (unsigned long)actual, (double)actual / 1000.0);
    } else {
        printf("Error setting clock: %d\r\n", r);
    }
    return r;
}

static int cmd_power_voltage(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: power voltage <volts> (0.80 to 1.30)\r\n");
        return -1;
    }

    float voltage = (float)atof(argv[2]);
    if (voltage < 0.80f || voltage > 1.30f) {
        printf("Error: voltage must be between 0.80V and 1.30V\r\n");
        return -1;
    }

    printf("Setting core voltage to %.2fV...\r\n", (double)voltage);
    int r = power_set_voltage(voltage);
    if (r == 0) {
        printf("Voltage set successfully.\r\n");
    } else {
        printf("Error setting voltage: %d\r\n", r);
    }
    return r;
}

static int lookup_peripheral(const char *name) {
    for (unsigned i = 0; i < NUM_PERIPHS; i++) {
        if (strcmp(periph_names[i].name, name) == 0) {
            return (int)periph_names[i].id;
        }
    }
    return -1;
}

static int cmd_power_periph(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: power periph disable|enable <name>\r\n");
        printf("Peripherals: ");
        for (unsigned i = 0; i < NUM_PERIPHS; i++) {
            printf("%s%s", periph_names[i].name,
                   (i < NUM_PERIPHS - 1) ? ", " : "\r\n");
        }
        return -1;
    }

    int pid = lookup_peripheral(argv[3]);
    if (pid < 0) {
        printf("Error: unknown peripheral '%s'\r\n", argv[3]);
        printf("Valid: ");
        for (unsigned i = 0; i < NUM_PERIPHS; i++) {
            printf("%s%s", periph_names[i].name,
                   (i < NUM_PERIPHS - 1) ? ", " : "\r\n");
        }
        return -1;
    }

    int r;
    if (strcmp(argv[2], "disable") == 0) {
        printf("Disabling %s...\r\n", argv[3]);
        r = power_disable_peripheral((uint8_t)pid);
        if (r == 0) {
            printf("%s disabled (held in reset).\r\n", argv[3]);
        } else {
            printf("Error: %d\r\n", r);
        }
    } else if (strcmp(argv[2], "enable") == 0) {
        printf("Enabling %s...\r\n", argv[3]);
        r = power_enable_peripheral((uint8_t)pid);
        if (r == 0) {
            printf("%s enabled.\r\n", argv[3]);
        } else {
            printf("Error: %d\r\n", r);
        }
    } else {
        printf("Error: use 'disable' or 'enable', not '%s'\r\n", argv[2]);
        return -1;
    }

    return r;
}

int cmd_power(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_power_usage();
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) {
        return cmd_power_status();
    }

    if (strcmp(argv[1], "sleep") == 0) {
        return cmd_power_sleep(argc, argv);
    }

    if (strcmp(argv[1], "clock") == 0) {
        return cmd_power_clock(argc, argv);
    }

    if (strcmp(argv[1], "voltage") == 0) {
        return cmd_power_voltage(argc, argv);
    }

    if (strcmp(argv[1], "periph") == 0) {
        return cmd_power_periph(argc, argv);
    }

    printf("Unknown power subcommand: %s\r\n", argv[1]);
    cmd_power_usage();
    return -1;
}
