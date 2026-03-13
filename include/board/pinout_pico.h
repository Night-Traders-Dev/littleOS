/* pinout_pico.h - Raspberry Pi Pico 40-pin DIP pinout */
#ifndef LITTLEOS_BOARD_PINOUT_PICO_H
#define LITTLEOS_BOARD_PINOUT_PICO_H

#define PINOUT_BOARD_NAME   "Raspberry Pi Pico"
#define PINOUT_NUM_ROWS     20

/* Left side: pins 1-20 (top to bottom) */
static const pin_desc_t left_pins[PINOUT_NUM_ROWS] = {
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
static const pin_desc_t right_pins[PINOUT_NUM_ROWS] = {
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

static const char *pinout_header[] = {
    "+=============================================================+",
    "|                   Raspberry Pi Pico                         |",
    "|                      +--USB--+                              |",
    "+=====================|       |==============================+",
};
#define PINOUT_HEADER_LINES 4

#endif /* LITTLEOS_BOARD_PINOUT_PICO_H */
