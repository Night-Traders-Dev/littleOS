/* display_module.h - Display Driver Module Interface for littleOS
 *
 * Defines the standard interface for display driver modules.
 * Each display controller (SSD1306, SH1107, ST7789, etc.) implements
 * this interface and registers as a kernel module.
 *
 * The display subsystem provides shared framebuffer and drawing
 * primitives; individual drivers only need to implement hardware
 * init, flush, and control operations.
 */
#ifndef LITTLEOS_DISPLAY_MODULE_H
#define LITTLEOS_DISPLAY_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Display driver ioctl commands ---- */
#define DISPLAY_IOCTL_SET_CONTRAST   1
#define DISPLAY_IOCTL_INVERT         2
#define DISPLAY_IOCTL_SET_ROTATION   3

/* ---- Display driver operations ---- */

/* Each display driver implements these callbacks.
 * The framebuffer is owned by the display subsystem (display.c)
 * and passed to the driver during flush. */
typedef struct {
    /* Hardware initialization.
     * config points to driver-specific config struct.
     * Returns 0 on success. */
    int  (*hw_init)(const void *config);

    /* Push framebuffer to the physical display.
     * fb points to the raw page-format framebuffer.
     * w, h are the logical dimensions. */
    void (*hw_flush)(const uint8_t *fb, int w, int h);

    /* Hardware-level contrast control (0-255) */
    void (*hw_set_contrast)(uint8_t contrast);

    /* Hardware-level display invert */
    void (*hw_invert)(bool invert);

    /* Release hardware resources */
    void (*hw_deinit)(void);

    /* Check if display hardware is responding */
    bool (*hw_is_connected)(void);
} display_driver_ops_t;

/* Display driver configuration passed during init */
typedef struct {
    int     width;              /* Logical width in pixels */
    int     height;             /* Logical height in pixels */
    const display_driver_ops_t *drv_ops;
} display_driver_info_t;

/* ---- SSD1306 configuration ---- */
typedef struct {
    uint8_t i2c_addr;           /* I2C address, typically 0x3C */
} ssd1306_config_t;

/* ---- SH1107 configuration ---- */
typedef struct {
    uint8_t spi_inst;           /* SPI instance (0 or 1), 0xFF = default */
    uint8_t mosi_pin;           /* MOSI GPIO, 0xFF = default */
    uint8_t sck_pin;            /* SCK GPIO, 0xFF = default */
    uint8_t cs_pin;             /* CS GPIO, 0xFF = default */
    uint8_t dc_pin;             /* DC GPIO, 0xFF = default */
    uint8_t rst_pin;            /* RST GPIO, 0xFF = default */
} sh1107_config_t;

/* ---- Active display management ---- */

/* Set the active display driver (called by module init).
 * Only one display can be active at a time. */
void display_set_active_driver(const display_driver_ops_t *ops, int w, int h);

/* Clear the active display driver (called by module deinit). */
void display_clear_active_driver(void);

#ifdef __cplusplus
}
#endif
#endif /* LITTLEOS_DISPLAY_MODULE_H */
