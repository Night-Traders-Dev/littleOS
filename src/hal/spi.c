/* spi.c - SPI Hardware Abstraction Layer Implementation for RP2040 */
#include "hal/spi.h"
#include "dmesg.h"
#include <string.h>

#ifdef PICO_BUILD
#include "hardware/spi.h"
#include "hardware/gpio.h"
#endif

/* RP2040 GPIO range */
#define GPIO_MIN_PIN    0
#define GPIO_MAX_PIN    29

/* Default data bits if not otherwise specified */
#define SPI_DEFAULT_DATA_BITS   8

/* Static configuration for both SPI instances */
static spi_config_t spi_configs[SPI_NUM_INSTANCES];

#ifdef PICO_BUILD
/**
 * @brief Get the Pico SDK spi instance for a given instance number
 */
static spi_inst_t *get_spi_inst(uint8_t instance) {
    return (instance == 0) ? spi0 : spi1;
}
#endif

/**
 * @brief Validate pin number is within RP2040 range
 */
static bool is_valid_pin(uint8_t pin) {
    return (pin >= GPIO_MIN_PIN && pin <= GPIO_MAX_PIN);
}

/**
 * @brief Extract CPOL and CPHA from SPI mode enum
 */
static void mode_to_cpol_cpha(spi_mode_t mode, uint8_t *cpol, uint8_t *cpha) {
    *cpol = (mode >> 1) & 1;
    *cpha = mode & 1;
}

int spi_hal_init(uint8_t instance, uint8_t mosi, uint8_t miso,
                 uint8_t sck, uint8_t cs, uint32_t baudrate, spi_mode_t mode) {
    if (instance >= SPI_NUM_INSTANCES) {
        dmesg_err("spi: invalid instance %u", instance);
        return -1;
    }

    if (!is_valid_pin(mosi) || !is_valid_pin(miso) ||
        !is_valid_pin(sck) || !is_valid_pin(cs)) {
        dmesg_err("spi%u: invalid pins MOSI=%u MISO=%u SCK=%u CS=%u",
                  instance, mosi, miso, sck, cs);
        return -1;
    }

    spi_config_t *cfg = &spi_configs[instance];

#ifdef PICO_BUILD
    spi_inst_t *inst = get_spi_inst(instance);

    /* Initialize SPI peripheral */
    uint32_t actual_baud = spi_init(inst, baudrate);

    /* Set SPI format: 8 data bits, CPOL/CPHA from mode, MSB first */
    uint8_t cpol, cpha;
    mode_to_cpol_cpha(mode, &cpol, &cpha);
    spi_set_format(inst, SPI_DEFAULT_DATA_BITS, cpol, cpha, SPI_MSB_FIRST);

    /* Configure GPIO pins for SPI function */
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    gpio_set_function(sck, GPIO_FUNC_SPI);

    /* CS is managed manually as a GPIO output, default HIGH (deselected) */
    gpio_init(cs);
    gpio_set_dir(cs, GPIO_OUT);
    gpio_put(cs, 1);

    cfg->baudrate = actual_baud;
    dmesg_info("spi%u: init MOSI=%u MISO=%u SCK=%u CS=%u baud=%lu mode=%u",
               instance, mosi, miso, sck, cs, actual_baud, mode);
#else
    cfg->baudrate = baudrate;
    dmesg_info("spi%u: stub init MOSI=%u MISO=%u SCK=%u CS=%u",
               instance, mosi, miso, sck, cs);
#endif

    cfg->instance = instance;
    cfg->mosi_pin = mosi;
    cfg->miso_pin = miso;
    cfg->sck_pin = sck;
    cfg->cs_pin = cs;
    cfg->mode = mode;
    cfg->data_bits = SPI_DEFAULT_DATA_BITS;
    cfg->initialized = true;

    return 0;
}

int spi_hal_deinit(uint8_t instance) {
    if (instance >= SPI_NUM_INSTANCES) {
        return -1;
    }

    spi_config_t *cfg = &spi_configs[instance];
    if (!cfg->initialized) {
        return -1;
    }

#ifdef PICO_BUILD
    spi_deinit(get_spi_inst(instance));
#endif

    dmesg_info("spi%u: deinitialized", instance);
    memset(cfg, 0, sizeof(*cfg));
    return 0;
}

void spi_hal_cs_select(uint8_t instance) {
    if (instance >= SPI_NUM_INSTANCES || !spi_configs[instance].initialized) {
        return;
    }

#ifdef PICO_BUILD
    gpio_put(spi_configs[instance].cs_pin, 0);
#endif
}

void spi_hal_cs_deselect(uint8_t instance) {
    if (instance >= SPI_NUM_INSTANCES || !spi_configs[instance].initialized) {
        return;
    }

#ifdef PICO_BUILD
    gpio_put(spi_configs[instance].cs_pin, 1);
#endif
}

int spi_hal_write(uint8_t instance, const uint8_t *data, size_t len) {
    if (instance >= SPI_NUM_INSTANCES || !spi_configs[instance].initialized) {
        dmesg_err("spi: write on uninitialized instance %u", instance);
        return -1;
    }

    if (!data || len == 0) {
        return -1;
    }

#ifdef PICO_BUILD
    spi_inst_t *inst = get_spi_inst(instance);

    spi_hal_cs_select(instance);
    int ret = spi_write_blocking(inst, data, len);
    spi_hal_cs_deselect(instance);

    return ret;
#else
    dmesg_debug("spi%u: stub write len=%u", instance, (unsigned)len);
    return (int)len;
#endif
}

int spi_hal_read(uint8_t instance, uint8_t *data, size_t len) {
    if (instance >= SPI_NUM_INSTANCES || !spi_configs[instance].initialized) {
        dmesg_err("spi: read on uninitialized instance %u", instance);
        return -1;
    }

    if (!data || len == 0) {
        return -1;
    }

#ifdef PICO_BUILD
    spi_inst_t *inst = get_spi_inst(instance);

    spi_hal_cs_select(instance);
    int ret = spi_read_blocking(inst, 0x00, data, len);
    spi_hal_cs_deselect(instance);

    return ret;
#else
    dmesg_debug("spi%u: stub read len=%u", instance, (unsigned)len);
    memset(data, 0, len);
    return (int)len;
#endif
}

int spi_hal_transfer(uint8_t instance, const uint8_t *tx, uint8_t *rx, size_t len) {
    if (instance >= SPI_NUM_INSTANCES || !spi_configs[instance].initialized) {
        dmesg_err("spi: transfer on uninitialized instance %u", instance);
        return -1;
    }

    if (!tx || !rx || len == 0) {
        return -1;
    }

#ifdef PICO_BUILD
    spi_inst_t *inst = get_spi_inst(instance);

    spi_hal_cs_select(instance);
    int ret = spi_write_read_blocking(inst, tx, rx, len);
    spi_hal_cs_deselect(instance);

    return ret;
#else
    dmesg_debug("spi%u: stub transfer len=%u", instance, (unsigned)len);
    memset(rx, 0, len);
    return (int)len;
#endif
}

int spi_hal_set_baudrate(uint8_t instance, uint32_t baudrate) {
    if (instance >= SPI_NUM_INSTANCES || !spi_configs[instance].initialized) {
        return -1;
    }

#ifdef PICO_BUILD
    spi_inst_t *inst = get_spi_inst(instance);
    uint32_t actual = spi_set_baudrate(inst, baudrate);
    spi_configs[instance].baudrate = actual;
    dmesg_info("spi%u: baudrate set to %lu (req %lu)", instance, actual, baudrate);
#else
    spi_configs[instance].baudrate = baudrate;
    dmesg_info("spi%u: stub baudrate set to %lu", instance, baudrate);
#endif

    return 0;
}

bool spi_hal_get_config(uint8_t instance, spi_config_t *config) {
    if (instance >= SPI_NUM_INSTANCES || !config) {
        return false;
    }

    if (!spi_configs[instance].initialized) {
        return false;
    }

    *config = spi_configs[instance];
    return true;
}
