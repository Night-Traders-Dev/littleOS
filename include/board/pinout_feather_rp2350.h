/* pinout_feather_rp2350.h - Adafruit Feather RP2350 pinout
 *
 * Feather form factor: 28 castellated pads + STEMMA QT connector
 * RP2350A variant (30 GPIO)
 * HSTX DVI on GPIO12-19
 */
#ifndef LITTLEOS_BOARD_PINOUT_FEATHER_RP2350_H
#define LITTLEOS_BOARD_PINOUT_FEATHER_RP2350_H

#define PINOUT_BOARD_NAME   "Adafruit Feather RP2350"
#define PINOUT_NUM_ROWS     16

/* Left side (top to bottom) */
static const pin_desc_t left_pins[PINOUT_NUM_ROWS] = {
    {  1, PIN_POWER, -1, "RST",     NULL     },
    {  2, PIN_POWER, -1, "3V3",     NULL     },
    {  3, PIN_POWER, -1, "AREF",    NULL     },
    {  4, PIN_GND,   -1, "GND",     NULL     },
    {  5, PIN_GPIO,  26, "A0",      "GP26"   },
    {  6, PIN_GPIO,  27, "A1",      "GP27"   },
    {  7, PIN_GPIO,  28, "A2",      "GP28"   },
    {  8, PIN_GPIO,  29, "A3",      "GP29"   },
    {  9, PIN_GPIO,  24, "D24",     "GP24"   },
    { 10, PIN_GPIO,  25, "D25",     "GP25"   },
    { 11, PIN_GPIO,  20, "SCK",     "GP20"   },
    { 12, PIN_GPIO,  23, "MOSI",    "GP23"   },
    { 13, PIN_GPIO,  22, "MISO",    "GP22"   },
    { 14, PIN_GPIO,   0, "RX",      "GP0"    },
    { 15, PIN_GPIO,   1, "TX",      "GP1"    },
    { 16, PIN_GND,   -1, "GND",     NULL     },
};

/* Right side (top to bottom) */
static const pin_desc_t right_pins[PINOUT_NUM_ROWS] = {
    { 28, PIN_POWER, -1, "VBUS",    NULL     },
    { 27, PIN_POWER, -1, "VBAT",    NULL     },
    { 26, PIN_POWER, -1, "EN",      NULL     },
    { 25, PIN_GND,   -1, "GND",     NULL     },
    { 24, PIN_GPIO,   4, "D4",      "GP4"    },
    { 23, PIN_GPIO,   5, "D5",      "GP5"    },
    { 22, PIN_GPIO,   6, "D6",      "GP6"    },
    { 21, PIN_GPIO,   7, "LED",     "GP7"    },
    { 20, PIN_GPIO,   8, "D8",      "GP8"    },
    { 19, PIN_GPIO,   9, "D9",      "GP9"    },
    { 18, PIN_GPIO,  10, "D10",     "GP10"   },
    { 17, PIN_GPIO,  11, "D11",     "GP11"   },
    { 16, PIN_GPIO,  12, "D12",     "GP12/HSTX" },
    { 15, PIN_GPIO,  13, "D13",     "GP13/HSTX" },
    { 14, PIN_GPIO,   2, "SDA",     "GP2"    },
    { 13, PIN_GPIO,   3, "SCL",     "GP3"    },
};

static const char *pinout_header[] = {
    "+==============================================+",
    "|          Adafruit Feather RP2350             |",
    "|               +--USB-C--+                    |",
    "+=============|           |====================+",
};
#define PINOUT_HEADER_LINES 4

#endif /* LITTLEOS_BOARD_PINOUT_FEATHER_RP2350_H */
