/* neopixel.c - WS2812/NeoPixel PIO driver for littleOS */
#include <stdio.h>
#include <string.h>
#include "neopixel.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#endif

static neopixel_rgb_t pixels[NEOPIXEL_MAX_LEDS];
static int num_pixels = 0;
static unsigned int neopixel_pin = 0;
static uint8_t global_brightness = 255;
static bool initialized = false;

#ifdef PICO_BUILD
static PIO pio_inst = pio0;
static unsigned int pio_sm = 0;
static unsigned int pio_offset = 0;

/* WS2812 PIO program - bit-bangs the NeoPixel protocol
 * T0H = 350ns, T1H = 700ns, T0L = 800ns, T1L = 600ns
 * Using 800kHz base frequency (1.25us per bit) */
static const uint16_t ws2812_program[] = {
    /* .wrap_target */
    0x6221, /* out x, 1       side 0 [2] ; shift 1 bit, set pin low */
    0x1123, /* jmp !x, 3      side 1 [1] ; if 0, jump to short pulse */
    0x1400, /* jmp 0           side 1 [4] ; if 1, long high pulse, then back */
    0xa442, /* nop             side 0 [4] ; short high done, extend low */
    /* .wrap */
};

static void ws2812_pio_init(unsigned int pin) {
    /* Claim a state machine */
    pio_sm = pio_claim_unused_sm(pio_inst, true);

    /* Load the program */
    pio_offset = pio_add_program(pio_inst, &(pio_program_t){
        .instructions = ws2812_program,
        .length = 4,
        .origin = -1,
    });

    /* Configure the state machine */
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, pio_offset, pio_offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_out_shift(&c, false, true, 24); /* shift left, autopull, 24 bits */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    /* Set pin as sideset output */
    pio_gpio_init(pio_inst, pin);
    pio_sm_set_consecutive_pindirs(pio_inst, pio_sm, pin, 1, true);
    sm_config_set_sideset_pins(&c, pin);

    /* Clock divider for 800kHz * 10 cycles = 8MHz */
    float div = (float)clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio_inst, pio_sm, pio_offset, &c);
    pio_sm_set_enabled(pio_inst, pio_sm, true);
}

static void ws2812_put_pixel(uint32_t grb) {
    pio_sm_put_blocking(pio_inst, pio_sm, grb << 8);
}
#endif

void neopixel_init(unsigned int pin, int num_leds) {
    if (num_leds > NEOPIXEL_MAX_LEDS) num_leds = NEOPIXEL_MAX_LEDS;
    if (num_leds < 1) num_leds = 1;

    num_pixels = num_leds;
    neopixel_pin = pin;
    memset(pixels, 0, sizeof(pixels));
    global_brightness = 255;

#ifdef PICO_BUILD
    ws2812_pio_init(pin);
#endif
    initialized = true;
}

void neopixel_set(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= num_pixels) return;
    pixels[index].r = r;
    pixels[index].g = g;
    pixels[index].b = b;
}

void neopixel_set_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < num_pixels; i++) {
        pixels[i].r = r;
        pixels[i].g = g;
        pixels[i].b = b;
    }
}

void neopixel_clear(void) {
    memset(pixels, 0, sizeof(neopixel_rgb_t) * num_pixels);
}

void neopixel_show(void) {
    if (!initialized) return;

#ifdef PICO_BUILD
    for (int i = 0; i < num_pixels; i++) {
        uint8_t r = (uint8_t)((uint16_t)pixels[i].r * global_brightness / 255);
        uint8_t g = (uint8_t)((uint16_t)pixels[i].g * global_brightness / 255);
        uint8_t b = (uint8_t)((uint16_t)pixels[i].b * global_brightness / 255);
        /* WS2812 expects GRB order */
        uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
        ws2812_put_pixel(grb);
    }
    sleep_us(60); /* Reset pulse */
#endif
}

void neopixel_set_brightness(uint8_t brightness) {
    global_brightness = brightness;
}

int neopixel_get_count(void) {
    return num_pixels;
}

void neopixel_rainbow(int offset) {
    for (int i = 0; i < num_pixels; i++) {
        int hue = (i * 256 / num_pixels + offset) % 256;
        /* Simple HSV-to-RGB with S=255, V=255 */
        int region = hue / 43;
        int remainder = (hue - region * 43) * 6;
        uint8_t p = 0, q = (uint8_t)(255 - remainder), t = (uint8_t)remainder;
        switch (region) {
            case 0:  pixels[i] = (neopixel_rgb_t){255, t, p}; break;
            case 1:  pixels[i] = (neopixel_rgb_t){q, 255, p}; break;
            case 2:  pixels[i] = (neopixel_rgb_t){p, 255, t}; break;
            case 3:  pixels[i] = (neopixel_rgb_t){p, q, 255}; break;
            case 4:  pixels[i] = (neopixel_rgb_t){t, p, 255}; break;
            default: pixels[i] = (neopixel_rgb_t){255, p, q}; break;
        }
    }
}

void neopixel_chase(uint8_t r, uint8_t g, uint8_t b, int pos) {
    neopixel_clear();
    if (pos >= 0 && pos < num_pixels) {
        pixels[pos].r = r;
        pixels[pos].g = g;
        pixels[pos].b = b;
    }
}
