/* drv_ssd1306.c - SSD1306 128x64 I2C OLED driver module for littleOS
 *
 * Registers as a kernel module. Can be loaded via:
 *   display init [addr]    (legacy)
 *   mod load ssd1306       (module system, uses default 0x3C)
 */
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "drivers/display_module.h"
#include "module.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#endif

/* ================================================================
 * Hardware state
 * ================================================================ */

static uint8_t     ssd_addr      = 0x3C;
static bool        ssd_connected = false;

/* Forward declaration */
static const display_driver_ops_t ssd1306_drv_ops;

#ifdef PICO_BUILD
static i2c_inst_t *ssd_i2c       = NULL;

/* ================================================================
 * I2C helpers
 * ================================================================ */

static void ssd1306_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(ssd_i2c, ssd_addr, buf, 2, false);
}

static void ssd1306_cmd2(uint8_t cmd, uint8_t arg) {
    uint8_t buf[3] = {0x00, cmd, arg};
    i2c_write_blocking(ssd_i2c, ssd_addr, buf, 3, false);
}
#endif /* PICO_BUILD */

/* ================================================================
 * Display driver ops
 * ================================================================ */

static int ssd1306_hw_init(const void *config) {
    const ssd1306_config_t *cfg = (const ssd1306_config_t *)config;
    if (cfg)
        ssd_addr = cfg->i2c_addr;
    else
        ssd_addr = 0x3C;

#ifdef PICO_BUILD
    ssd_i2c = i2c0;
    ssd1306_cmd(0xAE);        /* display off */
    ssd1306_cmd2(0xD5, 0x80); /* clock divide ratio */
    ssd1306_cmd2(0xA8, 0x3F); /* multiplex ratio (64-1) */
    ssd1306_cmd2(0xD3, 0x00); /* display offset = 0 */
    ssd1306_cmd(0x40);        /* start line = 0 */
    ssd1306_cmd2(0x8D, 0x14); /* enable charge pump */
    ssd1306_cmd2(0x20, 0x00); /* horizontal addressing mode */
    ssd1306_cmd(0xA1);        /* segment remap */
    ssd1306_cmd(0xC8);        /* COM scan direction remapped */
    ssd1306_cmd2(0xDA, 0x12); /* COM pins config */
    ssd1306_cmd2(0x81, 0xCF); /* contrast */
    ssd1306_cmd2(0xD9, 0xF1); /* pre-charge period */
    ssd1306_cmd2(0xDB, 0x40); /* VCOMH deselect level */
    ssd1306_cmd(0xA4);        /* display from RAM */
    ssd1306_cmd(0xA6);        /* normal display */
    ssd1306_cmd(0xAF);        /* display on */
    ssd_connected = true;
#endif

    display_set_active_driver(&ssd1306_drv_ops, 128, 64);

#ifdef PICO_BUILD
    /* Clear and flush to start with a blank screen */
    display_clear();
    display_flush();
#endif
    return 0;
}

static void ssd1306_hw_flush(const uint8_t *fb, int w, int h) {
    (void)w; (void)h;
#ifdef PICO_BUILD
    if (!ssd_connected) return;

    /* Set column range 0..127, page range 0..7 */
    ssd1306_cmd(0x21); ssd1306_cmd(0x00); ssd1306_cmd(0x7F);
    ssd1306_cmd(0x22); ssd1306_cmd(0x00); ssd1306_cmd(0x07);

    for (int i = 0; i < DISPLAY_MAX_BUF_SIZE; i += 16) {
        uint8_t buf[17];
        buf[0] = 0x40;
        int chunk = DISPLAY_MAX_BUF_SIZE - i;
        if (chunk > 16) chunk = 16;
        memcpy(&buf[1], &fb[i], (size_t)chunk);
        i2c_write_blocking(ssd_i2c, ssd_addr, buf, (size_t)(chunk + 1), false);
    }
#else
    (void)fb;
#endif
}

static void ssd1306_hw_set_contrast(uint8_t contrast) {
#ifdef PICO_BUILD
    if (!ssd_connected) return;
    ssd1306_cmd2(0x81, contrast);
#else
    (void)contrast;
#endif
}

static void ssd1306_hw_invert(bool invert) {
#ifdef PICO_BUILD
    if (!ssd_connected) return;
    ssd1306_cmd(invert ? 0xA7 : 0xA6);
#else
    (void)invert;
#endif
}

static void ssd1306_hw_deinit(void) {
#ifdef PICO_BUILD
    if (ssd_connected) {
        ssd1306_cmd(0xAE);  /* display off */
        ssd_connected = false;
    }
#endif
}

static bool ssd1306_hw_is_connected(void) {
    return ssd_connected;
}

/* Driver ops table */
static const display_driver_ops_t ssd1306_drv_ops = {
    .hw_init         = ssd1306_hw_init,
    .hw_flush        = ssd1306_hw_flush,
    .hw_set_contrast = ssd1306_hw_set_contrast,
    .hw_invert       = ssd1306_hw_invert,
    .hw_deinit       = ssd1306_hw_deinit,
    .hw_is_connected = ssd1306_hw_is_connected,
};

/* ================================================================
 * Module interface
 * ================================================================ */

static int ssd1306_mod_init(module_t *mod) {
    (void)mod;
    return ssd1306_hw_init(NULL);
}

static void ssd1306_mod_deinit(module_t *mod) {
    (void)mod;
    ssd1306_hw_deinit();
    display_clear_active_driver();
}

static void ssd1306_mod_status(module_t *mod) {
    (void)mod;
    printf("SSD1306 128x64 I2C OLED\r\n");
    printf("  Address:   0x%02X\r\n", ssd_addr);
    printf("  Connected: %s\r\n", ssd_connected ? "yes" : "no");
}

static const module_ops_t ssd1306_mod_ops = {
    .init   = ssd1306_mod_init,
    .deinit = ssd1306_mod_deinit,
    .status = ssd1306_mod_status,
};

static module_t ssd1306_module = {
    .name        = "ssd1306",
    .description = "SSD1306 128x64 I2C OLED display",
    .version     = "1.0.0",
    .type        = MODULE_TYPE_DRIVER,
    .ops         = &ssd1306_mod_ops,
};

/* Called from module_register_builtins() */
void ssd1306_module_register(void) {
    module_register(&ssd1306_module);
}

/* ================================================================
 * Direct init (called by legacy display_init path)
 * ================================================================ */

void ssd1306_hw_init_direct(uint8_t addr) {
    ssd1306_config_t cfg = { .i2c_addr = addr };
    ssd1306_hw_init(&cfg);

    /* Also mark module as loaded */
    ssd1306_module.state = MODULE_STATE_LOADED;
}
