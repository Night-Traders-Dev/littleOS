/* adc.h - ADC Hardware Abstraction Layer for littleOS */
#ifndef LITTLEOS_HAL_ADC_H
#define LITTLEOS_HAL_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADC: 12-bit SAR
 * RP2040:  5 channels (GPIO26-29 + temp sensor on ch4)
 * RP2350:  5 channels (GPIO26-29 + temp sensor on ch4) */

#define ADC_NUM_CHANNELS    NUM_ADC_CHANNELS
#define ADC_MAX_VALUE       4095    /* 12-bit */
#define ADC_VREF            3.3f   /* Reference voltage */
#define ADC_TEMP_CHANNEL    4

/* ADC channel configuration */
typedef struct {
    uint8_t     channel;        /* 0-4 */
    uint8_t     gpio_pin;       /* GPIO pin (26-29) or 0xFF for temp */
    bool        initialized;
    bool        enabled;
    uint16_t    last_raw;       /* Last raw reading */
    float       last_voltage;   /* Last converted voltage */
} adc_channel_config_t;

/* Initialize ADC subsystem */
int adc_hal_init(void);

/* Initialize specific channel */
int adc_hal_channel_init(uint8_t channel);

/* Read raw ADC value (12-bit, 0-4095) */
uint16_t adc_hal_read_raw(uint8_t channel);

/* Read voltage (0.0 - 3.3V) */
float adc_hal_read_voltage(uint8_t channel);

/* Read temperature in Celsius (channel 4 only) */
float adc_hal_read_temp_c(void);

/* Read multiple samples and average */
uint16_t adc_hal_read_averaged(uint8_t channel, uint8_t num_samples);

/* Get channel configuration */
bool adc_hal_get_config(uint8_t channel, adc_channel_config_t *config);

/* Get GPIO pin for ADC channel (returns 0xFF for temp sensor) */
uint8_t adc_hal_channel_to_gpio(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_ADC_H */
