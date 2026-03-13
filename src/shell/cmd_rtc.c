/* cmd_rtc.c - Shell command for external RTC (DS3231 / PCF8563) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal/rtc.h"

static const char *chip_name(rtc_chip_t c) {
    switch (c) {
        case RTC_CHIP_DS3231:  return "DS3231";
        case RTC_CHIP_PCF8563: return "PCF8563";
        default:               return "none";
    }
}

static const char *weekday_name[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static void print_datetime(const rtc_datetime_t *dt) {
    printf("%s %04u-%02u-%02u %02u:%02u:%02u\r\n",
           weekday_name[dt->weekday % 7],
           dt->year, dt->month, dt->day,
           dt->hour, dt->minute, dt->second);
}

static int cmd_rtc_status(void) {
    if (!rtc_is_ready()) {
        printf("RTC: not initialised\r\n");
        printf("Use 'rtc probe' to scan for an RTC chip\r\n");
        return 1;
    }

    printf("RTC chip: %s\r\n", chip_name(rtc_get_chip()));

    rtc_datetime_t dt;
    if (rtc_get_datetime(&dt) == 0) {
        printf("Time:     ");
        print_datetime(&dt);
        printf("Epoch:    %lu (since 2000-01-01)\r\n",
               (unsigned long)rtc_datetime_to_epoch(&dt));
    } else {
        printf("Failed to read time\r\n");
    }

    if (rtc_get_chip() == RTC_CHIP_DS3231) {
        rtc_temperature_t temp;
        if (rtc_ds3231_get_temperature(&temp) == 0) {
            printf("Temp:     %d.%02u C\r\n", temp.integer, temp.frac_25);
        }
        if (rtc_ds3231_oscillator_stopped()) {
            printf("WARNING:  Oscillator was stopped (power loss?)\r\n");
            printf("          Set the time and run 'rtc clearosc'\r\n");
        }
    }

    return 0;
}

static int cmd_rtc_probe(int argc, char *argv[]) {
    /* Default: I2C0 on GP4 (SDA) / GP5 (SCL) — standard Pico pinout */
    uint8_t inst = 0, sda = 4, scl = 5;

    if (argc >= 4) {
        inst = (uint8_t)atoi(argv[1]);
        sda  = (uint8_t)atoi(argv[2]);
        scl  = (uint8_t)atoi(argv[3]);
    }

    printf("Probing I2C%u (SDA=GP%u, SCL=GP%u)...\r\n", inst, sda, scl);
    rtc_chip_t chip = rtc_probe(inst, sda, scl);

    if (chip == RTC_CHIP_NONE) {
        printf("No RTC found\r\n");
        return 1;
    }

    printf("Found %s\r\n", chip_name(chip));
    int r = rtc_init(inst, sda, scl, chip);
    if (r != 0) {
        printf("Init failed (%d)\r\n", r);
        return 1;
    }

    printf("RTC initialised\r\n");
    return cmd_rtc_status();
}

/* Parse "YYYY-MM-DD HH:MM:SS" */
static int parse_datetime(const char *date_str, const char *time_str,
                          rtc_datetime_t *dt) {
    if (sscanf(date_str, "%hu-%hhu-%hhu",
               &dt->year, &dt->month, &dt->day) != 3)
        return -1;
    if (time_str) {
        if (sscanf(time_str, "%hhu:%hhu:%hhu",
                   &dt->hour, &dt->minute, &dt->second) < 2)
            return -1;
    } else {
        dt->hour = dt->minute = dt->second = 0;
    }
    /* Compute weekday from epoch */
    uint32_t epoch = rtc_datetime_to_epoch(dt);
    dt->weekday = (uint8_t)((epoch / 86400u + 6) % 7);
    return 0;
}

static int cmd_rtc_set(int argc, char *argv[]) {
    if (!rtc_is_ready()) {
        printf("RTC not initialised. Run 'rtc probe' first.\r\n");
        return 1;
    }
    if (argc < 2) {
        printf("Usage: rtc set <YYYY-MM-DD> [HH:MM:SS]\r\n");
        return 1;
    }

    rtc_datetime_t dt;
    memset(&dt, 0, sizeof(dt));
    if (parse_datetime(argv[1], argc >= 3 ? argv[2] : NULL, &dt) != 0) {
        printf("Invalid format. Use YYYY-MM-DD HH:MM:SS\r\n");
        return 1;
    }

    int r = rtc_set_datetime(&dt);
    if (r != 0) {
        printf("Failed to set time (%d)\r\n", r);
        return 1;
    }

    if (rtc_get_chip() == RTC_CHIP_DS3231)
        rtc_ds3231_clear_osc_flag();

    printf("Time set to ");
    print_datetime(&dt);
    return 0;
}

int cmd_rtc(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: rtc <subcommand>\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  status                        - Show RTC status\r\n");
        printf("  probe [i2c sda scl]           - Scan for RTC chip\r\n");
        printf("  get                           - Read date/time\r\n");
        printf("  set <YYYY-MM-DD> [HH:MM:SS]   - Set date/time\r\n");
        printf("  epoch                         - Show seconds since 2000\r\n");
        printf("  clearosc                      - Clear DS3231 osc-stop flag\r\n");
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        return cmd_rtc_status();
    }

    if (strcmp(argv[1], "probe") == 0) {
        return cmd_rtc_probe(argc - 1, argv + 1);
    }

    if (strcmp(argv[1], "get") == 0) {
        if (!rtc_is_ready()) {
            printf("RTC not initialised\r\n");
            return 1;
        }
        rtc_datetime_t dt;
        if (rtc_get_datetime(&dt) != 0) {
            printf("Read failed\r\n");
            return 1;
        }
        print_datetime(&dt);
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        return cmd_rtc_set(argc - 1, argv + 1);
    }

    if (strcmp(argv[1], "epoch") == 0) {
        if (!rtc_is_ready()) {
            printf("RTC not initialised\r\n");
            return 1;
        }
        rtc_datetime_t dt;
        if (rtc_get_datetime(&dt) != 0) {
            printf("Read failed\r\n");
            return 1;
        }
        printf("%lu\r\n", (unsigned long)rtc_datetime_to_epoch(&dt));
        return 0;
    }

    if (strcmp(argv[1], "clearosc") == 0) {
        if (rtc_get_chip() != RTC_CHIP_DS3231) {
            printf("Only available on DS3231\r\n");
            return 1;
        }
        rtc_ds3231_clear_osc_flag();
        printf("Oscillator-stop flag cleared\r\n");
        return 0;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
