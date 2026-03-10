/* cmd_gpiowatch.c - GPIO state change monitor for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#endif

/* Track last known state of all GPIOs */
static uint32_t last_gpio_state = 0;
static bool gpio_watch_initialized = false;

int cmd_gpiowatch(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: gpiowatch <command> [args...]\r\n");
        printf("Commands:\r\n");
        printf("  status            - Show all GPIO states\r\n");
        printf("  monitor [PINS]    - Monitor GPIO changes (press key to stop)\r\n");
        printf("  read PIN          - Read single pin state\r\n");
        printf("  set PIN VALUE     - Set output pin (0 or 1)\r\n");
        printf("  mode PIN MODE     - Set pin mode (in/out/up/down)\r\n");
        printf("  toggle PIN        - Toggle output pin\r\n");
        printf("  pulse PIN MS      - Pulse pin high for N milliseconds\r\n");
        return 0;
    }

#ifdef PICO_BUILD
    if (strcmp(argv[1], "status") == 0) {
        printf("=== GPIO Status (GP0-GP29) ===\r\n\r\n");
        printf("Pin  Dir    State  Function\r\n");
        printf("---  -----  -----  --------\r\n");

        for (int pin = 0; pin <= 29; pin++) {
            bool is_out = gpio_get_dir(pin);
            bool state = gpio_get(pin);

            /* Read function select from IO_BANK0 */
            uint32_t ctrl = io_bank0_hw->io[pin].ctrl;
            uint32_t funcsel = ctrl & 0x1f;

            const char *func_names[] = {
                "XIP", "SPI", "UART", "I2C", "PWM",
                "SIO", "PIO0", "PIO1", "GPCK", "USB"
            };
            const char *func = (funcsel < 10) ? func_names[funcsel] : "???";

            /* Only show configured/non-default pins or all if no filter */
            printf("GP%-2d %-5s  %-5s  %s",
                   pin,
                   is_out ? "OUT" : "IN",
                   state ? "HIGH" : "LOW",
                   func);

            /* Highlight special pins */
            if (pin == 25) printf("  (LED)");
            if (pin >= 26 && pin <= 29) printf("  (ADC%d)", pin - 26);
            printf("\r\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "monitor") == 0) {
        /* Parse optional pin list */
        uint32_t mask = 0x3FFFFFFF; /* All GP0-29 */
        if (argc >= 3) {
            mask = 0;
            for (int i = 2; i < argc; i++) {
                int pin = atoi(argv[i]);
                if (pin >= 0 && pin <= 29) mask |= (1u << pin);
            }
        }

        printf("Monitoring GPIO changes (mask=0x%08lX). Press any key to stop.\r\n\r\n",
               (unsigned long)mask);
        printf("Time(ms)   Pin   Change\r\n");
        printf("--------   ---   ------\r\n");

        /* Snapshot current state */
        uint32_t prev = gpio_get_all() & mask;
        uint32_t start = to_ms_since_boot(get_absolute_time());
        int changes = 0;

        while (1) {
            uint32_t curr = gpio_get_all() & mask;
            uint32_t diff = curr ^ prev;

            if (diff) {
                uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start;
                for (int pin = 0; pin <= 29; pin++) {
                    if (diff & (1u << pin)) {
                        bool new_state = (curr >> pin) & 1;
                        printf("%-10lu GP%-2d  %s -> %s\r\n",
                               (unsigned long)elapsed, pin,
                               new_state ? "LOW" : "HIGH",
                               new_state ? "HIGH" : "LOW");
                        changes++;
                    }
                }
                prev = curr;
            }

            sleep_us(100); /* 10kHz polling */
            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT) break;
        }

        printf("\r\n%d state changes detected.\r\n", changes);
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        if (argc < 3) { printf("Usage: gpiowatch read <pin>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        if (pin < 0 || pin > 29) { printf("Invalid pin (0-29)\r\n"); return 1; }
        printf("GP%d = %s\r\n", pin, gpio_get(pin) ? "HIGH" : "LOW");
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) { printf("Usage: gpiowatch set <pin> <0|1>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        int val = atoi(argv[3]);
        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, val ? 1 : 0);
        printf("GP%d = %s\r\n", pin, val ? "HIGH" : "LOW");
        return 0;
    }

    if (strcmp(argv[1], "mode") == 0) {
        if (argc < 4) { printf("Usage: gpiowatch mode <pin> <in|out|up|down>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }

        gpio_init(pin);
        if (strcmp(argv[3], "out") == 0) {
            gpio_set_dir(pin, GPIO_OUT);
            printf("GP%d: output mode\r\n", pin);
        } else if (strcmp(argv[3], "in") == 0) {
            gpio_set_dir(pin, GPIO_IN);
            printf("GP%d: input mode\r\n", pin);
        } else if (strcmp(argv[3], "up") == 0) {
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
            printf("GP%d: input with pull-up\r\n", pin);
        } else if (strcmp(argv[3], "down") == 0) {
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_down(pin);
            printf("GP%d: input with pull-down\r\n", pin);
        } else {
            printf("Unknown mode: %s (use in/out/up/down)\r\n", argv[3]);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "toggle") == 0) {
        if (argc < 3) { printf("Usage: gpiowatch toggle <pin>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }
        bool current = gpio_get(pin);
        gpio_put(pin, !current);
        printf("GP%d: %s -> %s\r\n", pin, current ? "HIGH" : "LOW", !current ? "HIGH" : "LOW");
        return 0;
    }

    if (strcmp(argv[1], "pulse") == 0) {
        if (argc < 4) { printf("Usage: gpiowatch pulse <pin> <ms>\r\n"); return 1; }
        int pin = atoi(argv[2]);
        int ms = atoi(argv[3]);
        if (pin < 0 || pin > 29) { printf("Invalid pin\r\n"); return 1; }

        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
        sleep_ms((uint32_t)ms);
        gpio_put(pin, 0);
        printf("GP%d: pulsed HIGH for %d ms\r\n", pin, ms);
        return 0;
    }
#else
    printf("GPIO commands require hardware (PICO_BUILD)\r\n");
#endif

    return 1;
}
