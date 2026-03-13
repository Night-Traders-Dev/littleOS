/* drv_sh1107.c - SH1107 64x128 SPI OLED driver module for littleOS
 *
 * Supports Waveshare Pico-OLED-1.3 and compatible SH1107 displays.
 * The physical display is 64-wide x 128-tall (portrait), but we use
 * a 128x64 landscape framebuffer (same as SSD1306) and rotate 90° CW
 * during flush for correct orientation.
 *
 * Registers as a kernel module. Can be loaded via:
 *   display init sh1107     (legacy)
 *   mod load sh1107         (module system, uses default Waveshare pins)
 */
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "drivers/display_module.h"
#include "module.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#endif

/* ================================================================
 * Default Waveshare Pico-OLED-1.3 pins
 * ================================================================ */

#define SH1107_DEF_SPI   1
#define SH1107_DEF_MOSI  11
#define SH1107_DEF_SCK   10
#define SH1107_DEF_CS    9
#define SH1107_DEF_DC    8
#define SH1107_DEF_RST   12
#define SH1107_BAUDRATE  (2000 * 1000)  /* 2 MHz */

/* ================================================================
 * Hardware state
 * ================================================================ */

static uint8_t sh_spi_inst = SH1107_DEF_SPI;
static uint8_t sh_dc_pin   = SH1107_DEF_DC;
static uint8_t sh_rst_pin  = SH1107_DEF_RST;
static uint8_t sh_cs_pin   = SH1107_DEF_CS;
static bool    sh_connected = false;

/* Forward declaration */
static const display_driver_ops_t sh1107_drv_ops;

#ifdef PICO_BUILD
/* ================================================================
 * SPI helpers
 * ================================================================ */

static spi_inst_t *sh_spi_hw(void) {
    return sh_spi_inst == 0 ? spi0 : spi1;
}

static void sh1107_reset(void) {
    gpio_put(sh_rst_pin, 0);
    sleep_ms(100);
    gpio_put(sh_rst_pin, 1);
    sleep_ms(100);
}

static void sh1107_cmd(uint8_t cmd) {
    gpio_put(sh_dc_pin, 0);       /* D/C = 0 -> command */
    gpio_put(sh_cs_pin, 0);
    spi_write_blocking(sh_spi_hw(), &cmd, 1);
    gpio_put(sh_cs_pin, 1);
}

static void sh1107_data(const uint8_t *data, size_t len) {
    gpio_put(sh_dc_pin, 1);       /* D/C = 1 -> data */
    gpio_put(sh_cs_pin, 0);
    spi_write_blocking(sh_spi_hw(), data, len);
    gpio_put(sh_cs_pin, 1);
}
#endif /* PICO_BUILD */

/* ================================================================
 * Display driver ops
 * ================================================================ */

static int sh1107_hw_init(const void *config) {
    const sh1107_config_t *cfg = (const sh1107_config_t *)config;

    if (cfg) {
        sh_spi_inst = (cfg->spi_inst == 0xFF) ? SH1107_DEF_SPI  : cfg->spi_inst;
        sh_cs_pin   = (cfg->cs_pin   == 0xFF) ? SH1107_DEF_CS   : cfg->cs_pin;
        sh_dc_pin   = (cfg->dc_pin   == 0xFF) ? SH1107_DEF_DC   : cfg->dc_pin;
        sh_rst_pin  = (cfg->rst_pin  == 0xFF) ? SH1107_DEF_RST  : cfg->rst_pin;
    }

    uint8_t pin_mosi = SH1107_DEF_MOSI;
    uint8_t pin_sck  = SH1107_DEF_SCK;
    if (cfg) {
        pin_mosi = (cfg->mosi_pin == 0xFF) ? SH1107_DEF_MOSI : cfg->mosi_pin;
        pin_sck  = (cfg->sck_pin  == 0xFF) ? SH1107_DEF_SCK  : cfg->sck_pin;
    }

#ifdef PICO_BUILD
    /* Init SPI peripheral */
    spi_init(sh_spi_hw(), SH1107_BAUDRATE);
    spi_set_format(sh_spi_hw(), 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(pin_sck,  GPIO_FUNC_SPI);
    gpio_set_function(pin_mosi, GPIO_FUNC_SPI);

    /* DC, CS, RST as GPIO outputs */
    gpio_init(sh_dc_pin);
    gpio_set_dir(sh_dc_pin, GPIO_OUT);
    gpio_init(sh_cs_pin);
    gpio_set_dir(sh_cs_pin, GPIO_OUT);
    gpio_put(sh_cs_pin, 1);
    gpio_init(sh_rst_pin);
    gpio_set_dir(sh_rst_pin, GPIO_OUT);

    /* Hardware reset */
    sh1107_reset();

    /* SH1107 init sequence — matches Waveshare Pico-OLED-1.3 demo */
    sh1107_cmd(0xAE);        /* display off */
    sh1107_cmd(0x00);        /* lower column address = 0 */
    sh1107_cmd(0x10);        /* upper column address = 0 */
    sh1107_cmd(0xB0);        /* page address = 0 */
    sh1107_cmd(0xDC);        /* display start line */
    sh1107_cmd(0x00);        /*   = 0 */
    sh1107_cmd(0x81);        /* contrast */
    sh1107_cmd(0x6F);        /*   = 0x6F (Waveshare default) */
    sh1107_cmd(0x21);        /* memory addressing: vertical */
    sh1107_cmd(0xA0);        /* segment remap: normal */
    sh1107_cmd(0xC0);        /* COM scan: normal */
    sh1107_cmd(0xA4);        /* entire display: follow RAM */
    sh1107_cmd(0xA6);        /* normal display (not inverted) */
    sh1107_cmd(0xA8);        /* multiplex ratio */
    sh1107_cmd(0x3F);        /*   = 63 (64 lines) */
    sh1107_cmd(0xD3);        /* display offset */
    sh1107_cmd(0x60);        /*   = 96 (for 64-column panel) */
    sh1107_cmd(0xD5);        /* clock divide / oscillator freq */
    sh1107_cmd(0x41);        /*   = Waveshare default */
    sh1107_cmd(0xD9);        /* pre-charge period */
    sh1107_cmd(0x22);        /*   = default */
    sh1107_cmd(0xDB);        /* VCOMH deselect level */
    sh1107_cmd(0x35);        /*   = default */
    sh1107_cmd(0xAD);        /* DC-DC control */
    sh1107_cmd(0x8A);        /*   = enable DC-DC */
    sleep_ms(200);
    sh1107_cmd(0xAF);        /* display on */

    sh_connected = true;

    /* Set active driver, then clear and flush to wipe random VRAM */
    display_set_active_driver(&sh1107_drv_ops, 128, 64);
    display_clear();
    display_flush();
#else
    (void)pin_mosi;
    (void)pin_sck;
    display_set_active_driver(&sh1107_drv_ops, 128, 64);
#endif

    return 0;
}

static void sh1107_hw_flush(const uint8_t *fb, int w, int h) {
    (void)w; (void)h;
#ifdef PICO_BUILD
    if (!sh_connected) return;

    /* Waveshare SH1107 flush: rotate 128x64 landscape framebuffer
     * onto the 64-wide x 128-tall portrait display.
     *
     * Mapping: framebuffer pixel (fx, fy) -> display column (63-fy),
     *          display row fx.  This is a 90 deg CW rotation.
     *
     * Framebuffer (SSD1306-style page format):
     *   fb[page*128 + x], bit (y%8), where page = y/8
     *
     * SH1107 column format: for column c, byte b contains
     *   rows b*8..b*8+7, bit k = row b*8+k.
     *   Auto-increments page in vertical addressing mode (0x21).
     */
    sh1107_cmd(0xB0);     /* start at page 0 */

    for (int j = 0; j < 64; j++) {
        uint8_t column = (uint8_t)(63 - j);
        sh1107_cmd((uint8_t)(0x00 + (column & 0x0F)));
        sh1107_cmd((uint8_t)(0x10 + (column >> 4)));

        int fy = j;
        int page = fy / 8;
        uint8_t mask = (uint8_t)(1 << (fy % 8));
        int base = page * 128;

        uint8_t coldata[16];
        for (int b = 0; b < 16; b++) {
            uint8_t val = 0;
            int fx_base = b * 8;
            for (int k = 0; k < 8; k++) {
                int fx = fx_base + k;
                if (fx < 128 && (fb[base + fx] & mask))
                    val |= (uint8_t)(1 << k);
            }
            coldata[b] = val;
        }
        sh1107_data(coldata, 16);
    }
#else
    (void)fb;
#endif
}

static void sh1107_hw_set_contrast(uint8_t contrast) {
#ifdef PICO_BUILD
    if (!sh_connected) return;
    sh1107_cmd(0x81);
    sh1107_cmd(contrast);
#else
    (void)contrast;
#endif
}

static void sh1107_hw_invert(bool invert) {
#ifdef PICO_BUILD
    if (!sh_connected) return;
    sh1107_cmd(invert ? (uint8_t)0xA7 : (uint8_t)0xA6);
#else
    (void)invert;
#endif
}

static void sh1107_hw_deinit(void) {
#ifdef PICO_BUILD
    if (sh_connected) {
        sh1107_cmd(0xAE);  /* display off */
        sh_connected = false;
    }
#endif
}

static bool sh1107_hw_is_connected(void) {
    return sh_connected;
}

/* Driver ops table */
static const display_driver_ops_t sh1107_drv_ops = {
    .hw_init         = sh1107_hw_init,
    .hw_flush        = sh1107_hw_flush,
    .hw_set_contrast = sh1107_hw_set_contrast,
    .hw_invert       = sh1107_hw_invert,
    .hw_deinit       = sh1107_hw_deinit,
    .hw_is_connected = sh1107_hw_is_connected,
};

/* ================================================================
 * Module interface
 * ================================================================ */

static int sh1107_mod_init(module_t *mod) {
    (void)mod;
    return sh1107_hw_init(NULL);
}

static void sh1107_mod_deinit(module_t *mod) {
    (void)mod;
    sh1107_hw_deinit();
    display_clear_active_driver();
}

static void sh1107_mod_status(module_t *mod) {
    (void)mod;
    printf("SH1107 64x128 SPI OLED (Waveshare Pico-OLED-1.3)\r\n");
    printf("  SPI:       spi%d\r\n", sh_spi_inst);
    printf("  Pins:      DC=GP%d CS=GP%d RST=GP%d\r\n",
           sh_dc_pin, sh_cs_pin, sh_rst_pin);
    printf("  Connected: %s\r\n", sh_connected ? "yes" : "no");
}

static const module_ops_t sh1107_mod_ops = {
    .init   = sh1107_mod_init,
    .deinit = sh1107_mod_deinit,
    .status = sh1107_mod_status,
};

static module_t sh1107_module = {
    .name        = "sh1107",
    .description = "SH1107 64x128 SPI OLED (Waveshare Pico-OLED-1.3)",
    .version     = "1.0.0",
    .type        = MODULE_TYPE_DRIVER,
    .ops         = &sh1107_mod_ops,
};

/* Called from module_register_builtins() */
void sh1107_module_register(void) {
    module_register(&sh1107_module);
}

/* ================================================================
 * Direct init (called by legacy display_init_sh1107 path)
 * ================================================================ */

void sh1107_hw_init_direct(uint8_t spi, uint8_t mosi, uint8_t sck,
                            uint8_t cs, uint8_t dc, uint8_t rst) {
    sh1107_config_t cfg = {
        .spi_inst = spi,
        .mosi_pin = mosi,
        .sck_pin  = sck,
        .cs_pin   = cs,
        .dc_pin   = dc,
        .rst_pin  = rst,
    };
    sh1107_hw_init(&cfg);

    /* Also mark module as loaded */
    sh1107_module.state = MODULE_STATE_LOADED;
}
