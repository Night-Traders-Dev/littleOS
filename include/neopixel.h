#ifndef LITTLEOS_NEOPIXEL_H
#define LITTLEOS_NEOPIXEL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define NEOPIXEL_MAX_LEDS  64

typedef struct {
    uint8_t r, g, b;
} neopixel_rgb_t;

void neopixel_init(unsigned int pin, int num_leds);
void neopixel_set(int index, uint8_t r, uint8_t g, uint8_t b);
void neopixel_set_all(uint8_t r, uint8_t g, uint8_t b);
void neopixel_clear(void);
void neopixel_show(void);
void neopixel_set_brightness(uint8_t brightness);  /* 0-255 */
int  neopixel_get_count(void);

/* Effects */
void neopixel_rainbow(int offset);
void neopixel_chase(uint8_t r, uint8_t g, uint8_t b, int pos);

#ifdef __cplusplus
}
#endif
#endif
