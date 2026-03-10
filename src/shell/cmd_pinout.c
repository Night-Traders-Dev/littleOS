/* cmd_pinout.c - GPIO Pin Visualizer for Raspberry Pi Pico board */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#endif

#include "watchdog.h"
#include "supervisor.h"

/* ANSI color codes */
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"

/* RP2040 has GPIO 0-28 (29 GPIO pins) */
#define NUM_GPIO 29

/* Pin type on the physical Pico board */
typedef enum {
    PIN_GPIO,
    PIN_GND,
    PIN_POWER,   /* VBUS, VSYS, 3V3, 3V3_EN, ADC_VREF */
    PIN_RUN,
} pin_type_t;

/* Physical pin descriptor */
typedef struct {
    int         pin_number;   /* physical pin 1-40 */
    pin_type_t  type;
    int         gpio;         /* GPIO number, or -1 if not GPIO */
    const char *label;        /* display label */
    const char *alt_label;    /* alternate label (e.g., ADC0) */
} pin_desc_t;

/* Left side: pins 1-20 (top to bottom) */
static const pin_desc_t left_pins[20] = {
    {  1, PIN_GPIO,  0, "GP0",  NULL     },
    {  2, PIN_GPIO,  1, "GP1",  NULL     },
    {  3, PIN_GND,  -1, "GND",  NULL     },
    {  4, PIN_GPIO,  2, "GP2",  NULL     },
    {  5, PIN_GPIO,  3, "GP3",  NULL     },
    {  6, PIN_GPIO,  4, "GP4",  NULL     },
    {  7, PIN_GPIO,  5, "GP5",  NULL     },
    {  8, PIN_GND,  -1, "GND",  NULL     },
    {  9, PIN_GPIO,  6, "GP6",  NULL     },
    { 10, PIN_GPIO,  7, "GP7",  NULL     },
    { 11, PIN_GPIO,  8, "GP8",  NULL     },
    { 12, PIN_GPIO,  9, "GP9",  NULL     },
    { 13, PIN_GND,  -1, "GND",  NULL     },
    { 14, PIN_GPIO, 10, "GP10", NULL     },
    { 15, PIN_GPIO, 11, "GP11", NULL     },
    { 16, PIN_GPIO, 12, "GP12", NULL     },
    { 17, PIN_GPIO, 13, "GP13", NULL     },
    { 18, PIN_GND,  -1, "GND",  NULL     },
    { 19, PIN_GPIO, 14, "GP14", NULL     },
    { 20, PIN_GPIO, 15, "GP15", NULL     },
};

/* Right side: pins 40-21 (top to bottom, so pin 40 is row 0) */
static const pin_desc_t right_pins[20] = {
    { 40, PIN_POWER, -1, "VBUS",    NULL     },
    { 39, PIN_POWER, -1, "VSYS",    NULL     },
    { 38, PIN_GND,   -1, "GND",     NULL     },
    { 37, PIN_POWER, -1, "3V3_EN",  NULL     },
    { 36, PIN_POWER, -1, "3V3",     NULL     },
    { 35, PIN_POWER, -1, "ADC_VREF",NULL     },
    { 34, PIN_GPIO,  28, "GP28",    "ADC2"   },
    { 33, PIN_GND,   -1, "GND",     "AGND"   },
    { 32, PIN_GPIO,  27, "GP27",    "ADC1"   },
    { 31, PIN_GPIO,  26, "GP26",    "ADC0"   },
    { 30, PIN_RUN,   -1, "RUN",     NULL     },
    { 29, PIN_GPIO,  22, "GP22",    NULL     },
    { 28, PIN_GND,   -1, "GND",     NULL     },
    { 27, PIN_GPIO,  21, "GP21",    NULL     },
    { 26, PIN_GPIO,  20, "GP20",    NULL     },
    { 25, PIN_GPIO,  19, "GP19",    NULL     },
    { 24, PIN_GPIO,  18, "GP18",    NULL     },
    { 23, PIN_GND,   -1, "GND",     NULL     },
    { 22, PIN_GPIO,  17, "GP17",    NULL     },
    { 21, PIN_GPIO,  16, "GP16",    NULL     },
};

/* GPIO function names (matches RP2040 SDK gpio_function enum) */
static const char *gpio_func_name(int func) {
    switch (func) {
        case 0:  return "XIP";
        case 1:  return "SPI";
        case 2:  return "UART";
        case 3:  return "I2C";
        case 4:  return "PWM";
        case 5:  return "SIO";  /* normal GPIO */
        case 6:  return "PIO0";
        case 7:  return "PIO1";
        case 8:  return "CLK";
        case 9:  return "USB";
        case 0x1f: return "NULL";
        default: return "???";
    }
}

/*
 * Get GPIO state. Returns direction, value, and function.
 * On non-Pico builds, returns demo/placeholder values.
 */
static void get_gpio_state(int gpio, int *dir, int *value, int *func) {
#ifdef PICO_BUILD
    *func  = (int)gpio_get_function((uint)gpio);
    *dir   = (int)gpio_get_dir((uint)gpio);
    *value = (int)gpio_get((uint)gpio);
#else
    /* Demo mode: all pins show as inactive */
    *dir   = 0;
    *value = 0;
    *func  = 0x1f; /* GPIO_FUNC_NULL */
    (void)gpio;
#endif
}

/*
 * Format a GPIO pin state string with color.
 * buf must be at least 32 bytes.
 * Returns the visible width (excluding ANSI codes).
 */
static int format_gpio_state(int gpio, char *buf, int buflen) {
    int dir, value, func;
    get_gpio_state(gpio, &dir, &value, &func);

#ifndef PICO_BUILD
    /* Non-Pico: show dashes */
    snprintf(buf, (size_t)buflen, "%s[  ---  ]%s", ANSI_DIM, ANSI_RESET);
    return 9; /* "[  ---  ]" */
#else
    const char *color;
    const char *dir_str;
    const char *val_str;

    if (func == 5) {
        /* SIO = normal GPIO */
        if (dir == 1) {
            /* Output */
            dir_str = "OUT";
            val_str = value ? "HIGH" : "LOW ";
            color   = value ? ANSI_GREEN : ANSI_RED;
        } else {
            /* Input */
            dir_str = "IN ";
            val_str = value ? "HIGH" : "LOW ";
            color   = ANSI_YELLOW;
        }
    } else if (func == 0x1f) {
        /* NULL function - unconfigured */
        snprintf(buf, (size_t)buflen, "%s[  ---  ]%s", ANSI_DIM, ANSI_RESET);
        return 9;
    } else {
        /* Alternate function */
        const char *fname = gpio_func_name(func);
        snprintf(buf, (size_t)buflen, "%s[%-6s ]%s", ANSI_CYAN, fname, ANSI_RESET);
        return 9;
    }

    snprintf(buf, (size_t)buflen, "%s[%s %s]%s", color, dir_str, val_str, ANSI_RESET);
    return 9; /* "[OUT HIGH]" = 9 visible chars */
#endif
}

/* Format a non-GPIO pin (GND, power, RUN) */
static void format_special_pin(const pin_desc_t *pin, char *buf, int buflen) {
    switch (pin->type) {
        case PIN_GND:
            snprintf(buf, (size_t)buflen, "%s", ANSI_DIM);
            break;
        case PIN_POWER:
            snprintf(buf, (size_t)buflen, "%s", ANSI_BOLD);
            break;
        case PIN_RUN:
            snprintf(buf, (size_t)buflen, "%s", ANSI_DIM);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void print_board(void) {
    /* Board header */
    printf("\033[2J\033[H"); /* clear screen, cursor home */
    printf("%s", ANSI_BOLD);
    printf("+=============================================================+\r\n");
    printf("|                   Raspberry Pi Pico                         |\r\n");
    printf("|                      +--USB--+                              |\r\n");
    printf("+=====================|       |==============================+\r\n");
    printf("%s", ANSI_RESET);

    /* Print each row of pins */
    for (int row = 0; row < 20; row++) {
        const pin_desc_t *lp = &left_pins[row];
        const pin_desc_t *rp = &right_pins[row];

        /* Left side */
        printf("| ");

        if (lp->type == PIN_GPIO) {
            char state[64];
            format_gpio_state(lp->gpio, state, (int)sizeof(state));
            /* label + state: "GP0  [OUT HIGH]" */
            printf("%-5s %s", lp->label, state);
        } else {
            char prefix[16];
            format_special_pin(lp, prefix, (int)sizeof(prefix));
            printf("%s     %-9s%s", prefix, lp->label, ANSI_RESET);
        }

        /* Pin numbers in the middle */
        printf("  (%2d)  (%2d)  ", lp->pin_number, rp->pin_number);

        /* Right side */
        if (rp->type == PIN_GPIO) {
            char state[64];
            format_gpio_state(rp->gpio, state, (int)sizeof(state));
            /* Build label with optional alternate */
            char label[16];
            if (rp->alt_label) {
                snprintf(label, sizeof(label), "%s/%s", rp->label, rp->alt_label);
            } else {
                snprintf(label, sizeof(label), "%s", rp->label);
            }
            printf("%s %-10s", state, label);
        } else {
            char prefix[16];
            format_special_pin(rp, prefix, (int)sizeof(prefix));
            char label[16];
            if (rp->alt_label) {
                snprintf(label, sizeof(label), "%s/%s", rp->label, rp->alt_label);
            } else {
                snprintf(label, sizeof(label), "%s", rp->label);
            }
            printf("%-9s %s%s", " ", prefix, ANSI_RESET);
            printf("%-10s", label);
        }

        printf(" |\r\n");
    }

    printf("%s", ANSI_BOLD);
    printf("+=============================================================+\r\n");
    printf("%s", ANSI_RESET);
}

static void print_legend(void) {
    printf("Pin state legend:\r\n");
    printf("  %s[OUT HIGH]%s  - GPIO output, driven HIGH\r\n", ANSI_GREEN, ANSI_RESET);
    printf("  %s[OUT LOW ]%s  - GPIO output, driven LOW\r\n", ANSI_RED, ANSI_RESET);
    printf("  %s[IN  HIGH]%s  - GPIO input, reading HIGH\r\n", ANSI_YELLOW, ANSI_RESET);
    printf("  %s[IN  LOW ]%s  - GPIO input, reading LOW\r\n", ANSI_YELLOW, ANSI_RESET);
    printf("  %s[UART   ]%s  - Alternate function (UART, SPI, I2C, etc.)\r\n", ANSI_CYAN, ANSI_RESET);
    printf("  %s[  ---  ]%s  - Unconfigured / not available\r\n", ANSI_DIM, ANSI_RESET);
    printf("\r\n");
    printf("  %sGND%s          - Ground pin\r\n", ANSI_DIM, ANSI_RESET);
    printf("  %sVBUS/VSYS%s   - Power pins\r\n", ANSI_BOLD, ANSI_RESET);
    printf("\r\n");
}

static void print_gpio_detail(int gpio) {
    if (gpio < 0 || gpio >= NUM_GPIO) {
        printf("Invalid GPIO number: %d (valid range: 0-%d)\r\n", gpio, NUM_GPIO - 1);
        return;
    }

    int dir, value, func;
    get_gpio_state(gpio, &dir, &value, &func);

    printf("GPIO %d details:\r\n", gpio);
    printf("  Function:  %s (%d)\r\n", gpio_func_name(func), func);

    if (func == 0x1f) {
        printf("  State:     Unconfigured (NULL function)\r\n");
        return;
    }

    if (func == 5) {
        /* SIO - normal GPIO */
        printf("  Direction: %s\r\n", dir ? "OUTPUT" : "INPUT");
        printf("  Value:     %s (%d)\r\n", value ? "HIGH" : "LOW", value);

#ifdef PICO_BUILD
        /* Pull-up/pull-down info */
        bool pull_up   = gpio_is_pulled_up((uint)gpio);
        bool pull_down = gpio_is_pulled_down((uint)gpio);
        if (pull_up)
            printf("  Pull:      UP\r\n");
        else if (pull_down)
            printf("  Pull:      DOWN\r\n");
        else
            printf("  Pull:      NONE\r\n");

        /* Drive strength */
        uint drive = gpio_get_drive_strength((uint)gpio);
        const char *drive_str;
        switch (drive) {
            case 0: drive_str = "2mA";  break;
            case 1: drive_str = "4mA";  break;
            case 2: drive_str = "8mA";  break;
            case 3: drive_str = "12mA"; break;
            default: drive_str = "???"; break;
        }
        printf("  Drive:     %s\r\n", drive_str);

        /* Slew rate */
        bool slew_fast = gpio_get_slew_rate((uint)gpio);
        printf("  Slew rate: %s\r\n", slew_fast ? "FAST" : "SLOW");
#endif
    } else {
        printf("  Mode:      Alternate function (%s)\r\n", gpio_func_name(func));
        printf("  Value:     %s (%d)\r\n", value ? "HIGH" : "LOW", value);
    }

    /* Find physical pin number */
    for (int i = 0; i < 20; i++) {
        if (left_pins[i].gpio == gpio) {
            printf("  Phys pin:  %d (left side)\r\n", left_pins[i].pin_number);
            return;
        }
        if (right_pins[i].gpio == gpio) {
            const pin_desc_t *rp = &right_pins[i];
            printf("  Phys pin:  %d (right side)", rp->pin_number);
            if (rp->alt_label) {
                printf(" [also: %s]", rp->alt_label);
            }
            printf("\r\n");
            return;
        }
    }
}

static int cmd_pinout_watch(void) {
    printf("Pinout watch mode - press 'q' to quit\r\n");
    printf("\r\n");

    while (1) {
        wdt_feed();
        supervisor_heartbeat();

        print_board();
        printf("\r\nPress 'q' to quit...\r\n");

#ifdef PICO_BUILD
        /* Wait ~500ms, checking for keypress */
        for (int i = 0; i < 50; i++) {
            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT && (c == 'q' || c == 'Q')) {
                printf("Watch mode stopped.\r\n");
                return 0;
            }
            sleep_ms(10);
            wdt_feed();
        }
#else
        /* Non-Pico: just show once and exit */
        printf("(watch mode requires Pico hardware)\r\n");
        return 0;
#endif
    }
}

static void cmd_pinout_usage(void) {
    printf("GPIO Pin Visualizer - Raspberry Pi Pico pinout\r\n");
    printf("Usage:\r\n");
    printf("  pinout              - Show board with current GPIO states\r\n");
    printf("  pinout watch        - Live updating display (q to quit)\r\n");
    printf("  pinout gpio <N>     - Show detailed info for GPIO N\r\n");
    printf("  pinout legend       - Show color legend\r\n");
}

int cmd_pinout(int argc, char *argv[]) {
    if (argc < 2) {
        /* Default: show board */
        print_board();
        print_legend();
        return 0;
    }

    if (strcmp(argv[1], "watch") == 0) {
        return cmd_pinout_watch();
    }

    if (strcmp(argv[1], "gpio") == 0) {
        if (argc < 3) {
            printf("Usage: pinout gpio <N>\r\n");
            printf("  N = GPIO number (0-%d)\r\n", NUM_GPIO - 1);
            return -1;
        }
        int gpio = atoi(argv[2]);
        print_gpio_detail(gpio);
        return 0;
    }

    if (strcmp(argv[1], "legend") == 0) {
        print_legend();
        return 0;
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        cmd_pinout_usage();
        return 0;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    cmd_pinout_usage();
    return -1;
}
