/* board_config.h - Board-specific configuration for littleOS
 *
 * Auto-selects board metadata using SDK-defined macros.
 * The Pico SDK board headers define detection macros like
 * ADAFRUIT_FEATHER_RP2350, and platform macros like PICO_RP2350.
 */
#ifndef LITTLEOS_BOARD_CONFIG_H
#define LITTLEOS_BOARD_CONFIG_H

#include "hardware/platform_defs.h"

/* =========================================================================
 * Chip Detection
 * ========================================================================= */

#if PICO_RP2350
    #define CHIP_MODEL_STR      "RP2350"
    #define CHIP_RAM_SIZE       (520 * 1024)    /* 520KB SRAM */
    #define CHIP_CORE_COUNT     2
    #if PICO_RISCV
        #define CHIP_CORE_STR   "Hazard3 RISC-V (Dual Core)"
    #else
        #define CHIP_CORE_STR   "ARM Cortex-M33 (Dual Core)"
    #endif
#else
    #define CHIP_MODEL_STR      "RP2040"
    #define CHIP_RAM_SIZE       (264 * 1024)    /* 264KB SRAM */
    #define CHIP_CORE_COUNT     2
    #define CHIP_CORE_STR       "ARM Cortex-M0+ (Dual Core)"
#endif

/* Flash size from SDK board header, with fallback */
#ifndef PICO_FLASH_SIZE_BYTES
    #define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif
#define CHIP_FLASH_SIZE     PICO_FLASH_SIZE_BYTES

/* =========================================================================
 * Board Detection
 * ========================================================================= */

#if defined(ADAFRUIT_FEATHER_RP2350)
    #define BOARD_NAME          "Adafruit Feather RP2350"
    #define BOARD_HAS_HSTX      1
    #define BOARD_HAS_WIFI      0
#elif defined(RASPBERRYPI_PICO2_W)
    #define BOARD_NAME          "Raspberry Pi Pico 2 W"
    #define BOARD_HAS_HSTX      1
    #define BOARD_HAS_WIFI      1
#elif defined(RASPBERRYPI_PICO2)
    #define BOARD_NAME          "Raspberry Pi Pico 2"
    #define BOARD_HAS_HSTX      1
    #define BOARD_HAS_WIFI      0
#elif defined(PICO_W)
    #define BOARD_NAME          "Raspberry Pi Pico W"
    #define BOARD_HAS_HSTX      0
    #define BOARD_HAS_WIFI      1
#else
    #define BOARD_NAME          "Raspberry Pi Pico"
    #define BOARD_HAS_HSTX      0
    #define BOARD_HAS_WIFI      0
#endif

/* LED pin from SDK board header */
#ifdef PICO_DEFAULT_LED_PIN
    #define BOARD_LED_PIN       PICO_DEFAULT_LED_PIN
#endif

/* WS2812 NeoPixel pin from SDK board header */
#ifdef PICO_DEFAULT_WS2812_PIN
    #define BOARD_WS2812_PIN    PICO_DEFAULT_WS2812_PIN
#endif

/* =========================================================================
 * GPIO Limits (from SDK platform_defs.h)
 * ========================================================================= */

#define BOARD_GPIO_MIN      0
#define BOARD_GPIO_MAX      (NUM_BANK0_GPIOS - 1)
#define BOARD_GPIO_COUNT    NUM_BANK0_GPIOS

#endif /* LITTLEOS_BOARD_CONFIG_H */
