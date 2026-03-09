/* adc.c - ADC Hardware Abstraction Layer Implementation for RP2040 */
#include "hal/adc.h"
#include "dmesg.h"
#include <string.h>

#ifdef PICO_BUILD
#include "hardware/adc.h"
#include "hardware/gpio.h"
#endif

/* Temperature sensor constants (from RP2040 datasheet) */
#define ADC_TEMP_REF_VOLTAGE    0.706f
#define ADC_TEMP_SLOPE          0.001721f
#define ADC_TEMP_REF_CELSIUS    27.0f

/* GPIO base for ADC channels 0-3 */
#define ADC_GPIO_BASE           26

/* Static configuration for all ADC channels */
static adc_channel_config_t adc_channels[ADC_NUM_CHANNELS];

/* Track whether the ADC hardware has been initialized */
static bool adc_hw_initialized = false;

int adc_hal_init(void) {
    if (adc_hw_initialized) {
        dmesg_debug("adc: already initialized");
        return 0;
    }

#ifdef PICO_BUILD
    adc_init();
    adc_set_temp_sensor_enabled(true);
#endif

    /* Clear all channel configs */
    memset(adc_channels, 0, sizeof(adc_channels));

    /* Pre-populate channel metadata */
    for (uint8_t ch = 0; ch < ADC_NUM_CHANNELS; ch++) {
        adc_channels[ch].channel = ch;
        adc_channels[ch].gpio_pin = (ch < 4) ? (ADC_GPIO_BASE + ch) : 0xFF;
    }

    adc_hw_initialized = true;
    dmesg_info("adc: initialized, temp sensor enabled");

    return 0;
}

int adc_hal_channel_init(uint8_t channel) {
    if (channel >= ADC_NUM_CHANNELS) {
        dmesg_err("adc: invalid channel %u", channel);
        return -1;
    }

    if (!adc_hw_initialized) {
        dmesg_err("adc: hardware not initialized, call adc_hal_init() first");
        return -1;
    }

    adc_channel_config_t *ch = &adc_channels[channel];

#ifdef PICO_BUILD
    if (channel < 4) {
        /* Channels 0-3 map to GPIO26-29 */
        adc_gpio_init(ADC_GPIO_BASE + channel);
    }
    /* Channel 4 (temp sensor) is already enabled in adc_hal_init */
#endif

    ch->initialized = true;
    ch->enabled = true;

    if (channel == ADC_TEMP_CHANNEL) {
        dmesg_info("adc: channel %u (temp sensor) initialized", channel);
    } else {
        dmesg_info("adc: channel %u (GPIO%u) initialized", channel, ch->gpio_pin);
    }

    return 0;
}

uint16_t adc_hal_read_raw(uint8_t channel) {
    if (channel >= ADC_NUM_CHANNELS) {
        dmesg_err("adc: read_raw invalid channel %u", channel);
        return 0;
    }

    if (!adc_channels[channel].initialized) {
        dmesg_err("adc: read_raw on uninitialized channel %u", channel);
        return 0;
    }

    uint16_t raw = 0;

#ifdef PICO_BUILD
    adc_select_input(channel);
    raw = adc_read();
#endif

    adc_channels[channel].last_raw = raw;
    return raw;
}

float adc_hal_read_voltage(uint8_t channel) {
    uint16_t raw = adc_hal_read_raw(channel);
    float voltage = (float)raw * ADC_VREF / (float)(ADC_MAX_VALUE + 1);

    adc_channels[channel].last_voltage = voltage;
    return voltage;
}

float adc_hal_read_temp_c(void) {
    if (!adc_channels[ADC_TEMP_CHANNEL].initialized) {
        dmesg_err("adc: temp sensor not initialized");
        return 0.0f;
    }

    float voltage = adc_hal_read_voltage(ADC_TEMP_CHANNEL);

    /* RP2040 temperature formula:
     * temp_C = 27.0 - (voltage - 0.706) / 0.001721 */
    float temp_c = ADC_TEMP_REF_CELSIUS -
                   (voltage - ADC_TEMP_REF_VOLTAGE) / ADC_TEMP_SLOPE;

    return temp_c;
}

uint16_t adc_hal_read_averaged(uint8_t channel, uint8_t num_samples) {
    if (channel >= ADC_NUM_CHANNELS || !adc_channels[channel].initialized) {
        return 0;
    }

    if (num_samples == 0) {
        num_samples = 1;
    }

    uint32_t sum = 0;

    for (uint8_t i = 0; i < num_samples; i++) {
        sum += adc_hal_read_raw(channel);
    }

    uint16_t avg = (uint16_t)(sum / num_samples);
    adc_channels[channel].last_raw = avg;

    return avg;
}

bool adc_hal_get_config(uint8_t channel, adc_channel_config_t *config) {
    if (channel >= ADC_NUM_CHANNELS || !config) {
        return false;
    }

    if (!adc_channels[channel].initialized) {
        return false;
    }

    *config = adc_channels[channel];
    return true;
}

uint8_t adc_hal_channel_to_gpio(uint8_t channel) {
    if (channel < 4) {
        return ADC_GPIO_BASE + channel;
    }
    return 0xFF;
}
