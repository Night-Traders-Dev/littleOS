/* i2c.c - I2C Hardware Abstraction Layer Implementation for RP2040 */
#include "hal/i2c.h"
#include "dmesg.h"
#include <string.h>

#ifdef PICO_BUILD
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#endif

/* RP2040 GPIO range */
#define GPIO_MIN_PIN    0
#define GPIO_MAX_PIN    29

/* I2C scan address range */
#define I2C_SCAN_ADDR_MIN   0x08
#define I2C_SCAN_ADDR_MAX   0x77

/* Static configuration for both I2C instances */
static i2c_config_t i2c_configs[I2C_NUM_INSTANCES];

#ifdef PICO_BUILD
/**
 * @brief Get the Pico SDK i2c instance for a given instance number
 */
static i2c_inst_t *get_i2c_inst(uint8_t instance) {
    return (instance == 0) ? i2c0 : i2c1;
}
#endif

/**
 * @brief Validate pin number is within RP2040 range
 */
static bool is_valid_pin(uint8_t pin) {
    return (pin >= GPIO_MIN_PIN && pin <= GPIO_MAX_PIN);
}

int i2c_hal_init(uint8_t instance, uint8_t sda_pin, uint8_t scl_pin, i2c_speed_t speed) {
    if (instance >= I2C_NUM_INSTANCES) {
        dmesg_err("i2c: invalid instance %u", instance);
        return -1;
    }

    if (!is_valid_pin(sda_pin) || !is_valid_pin(scl_pin)) {
        dmesg_err("i2c%u: invalid pins SDA=%u SCL=%u", instance, sda_pin, scl_pin);
        return -1;
    }

    i2c_config_t *cfg = &i2c_configs[instance];

#ifdef PICO_BUILD
    i2c_inst_t *inst = get_i2c_inst(instance);

    /* Initialize I2C peripheral at requested speed */
    uint32_t actual_baud = i2c_init(inst, (uint32_t)speed);

    /* Configure GPIO pins for I2C function */
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);

    /* Enable internal pull-ups (I2C requires pull-ups) */
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    cfg->baudrate = actual_baud;
    dmesg_info("i2c%u: init SDA=%u SCL=%u baud=%lu (req %lu)",
               instance, sda_pin, scl_pin, actual_baud, (uint32_t)speed);
#else
    cfg->baudrate = (uint32_t)speed;
    dmesg_info("i2c%u: stub init SDA=%u SCL=%u", instance, sda_pin, scl_pin);
#endif

    cfg->instance = instance;
    cfg->sda_pin = sda_pin;
    cfg->scl_pin = scl_pin;
    cfg->initialized = true;

    return 0;
}

int i2c_hal_deinit(uint8_t instance) {
    if (instance >= I2C_NUM_INSTANCES) {
        return -1;
    }

    i2c_config_t *cfg = &i2c_configs[instance];
    if (!cfg->initialized) {
        return -1;
    }

#ifdef PICO_BUILD
    i2c_deinit(get_i2c_inst(instance));
#endif

    dmesg_info("i2c%u: deinitialized", instance);
    memset(cfg, 0, sizeof(*cfg));
    return 0;
}

int i2c_hal_write(uint8_t instance, uint8_t addr, const uint8_t *data, size_t len) {
    if (instance >= I2C_NUM_INSTANCES || !i2c_configs[instance].initialized) {
        dmesg_err("i2c: write on uninitialized instance %u", instance);
        return -1;
    }

    if (!data || len == 0 || len > I2C_MAX_TRANSFER) {
        return -1;
    }

#ifdef PICO_BUILD
    i2c_inst_t *inst = get_i2c_inst(instance);
    int ret = i2c_write_blocking(inst, addr, data, len, false);
    if (ret == PICO_ERROR_GENERIC) {
        dmesg_debug("i2c%u: write NACK addr=0x%02X", instance, addr);
        return -1;
    }
    return ret;
#else
    dmesg_debug("i2c%u: stub write addr=0x%02X len=%u", instance, addr, (unsigned)len);
    return (int)len;
#endif
}

int i2c_hal_read(uint8_t instance, uint8_t addr, uint8_t *data, size_t len) {
    if (instance >= I2C_NUM_INSTANCES || !i2c_configs[instance].initialized) {
        dmesg_err("i2c: read on uninitialized instance %u", instance);
        return -1;
    }

    if (!data || len == 0 || len > I2C_MAX_TRANSFER) {
        return -1;
    }

#ifdef PICO_BUILD
    i2c_inst_t *inst = get_i2c_inst(instance);
    int ret = i2c_read_blocking(inst, addr, data, len, false);
    if (ret == PICO_ERROR_GENERIC) {
        dmesg_debug("i2c%u: read NACK addr=0x%02X", instance, addr);
        return -1;
    }
    return ret;
#else
    dmesg_debug("i2c%u: stub read addr=0x%02X len=%u", instance, addr, (unsigned)len);
    memset(data, 0, len);
    return (int)len;
#endif
}

int i2c_hal_write_read(uint8_t instance, uint8_t addr,
                       const uint8_t *write_data, size_t write_len,
                       uint8_t *read_data, size_t read_len) {
    if (instance >= I2C_NUM_INSTANCES || !i2c_configs[instance].initialized) {
        return -1;
    }

    if (!write_data || write_len == 0 || !read_data || read_len == 0) {
        return -1;
    }

#ifdef PICO_BUILD
    i2c_inst_t *inst = get_i2c_inst(instance);

    /* Write phase: nostop=true to hold the bus */
    int ret = i2c_write_blocking(inst, addr, write_data, write_len, true);
    if (ret == PICO_ERROR_GENERIC) {
        dmesg_debug("i2c%u: write_read write phase NACK addr=0x%02X", instance, addr);
        return -1;
    }

    /* Read phase: nostop=false to release the bus */
    ret = i2c_read_blocking(inst, addr, read_data, read_len, false);
    if (ret == PICO_ERROR_GENERIC) {
        dmesg_debug("i2c%u: write_read read phase NACK addr=0x%02X", instance, addr);
        return -1;
    }

    return ret;
#else
    dmesg_debug("i2c%u: stub write_read addr=0x%02X wlen=%u rlen=%u",
                instance, addr, (unsigned)write_len, (unsigned)read_len);
    memset(read_data, 0, read_len);
    return (int)read_len;
#endif
}

int i2c_hal_write_reg(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    return i2c_hal_write(instance, addr, buf, 2);
}

int i2c_hal_read_reg(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t *value) {
    if (!value) {
        return -1;
    }
    return i2c_hal_write_read(instance, addr, &reg, 1, value, 1);
}

int i2c_hal_scan(uint8_t instance, uint8_t *found_addrs, uint8_t max_addrs) {
    if (instance >= I2C_NUM_INSTANCES || !i2c_configs[instance].initialized) {
        dmesg_err("i2c: scan on uninitialized instance %u", instance);
        return -1;
    }

    if (!found_addrs || max_addrs == 0) {
        return -1;
    }

    uint8_t count = 0;

#ifdef PICO_BUILD
    i2c_inst_t *inst = get_i2c_inst(instance);
    uint8_t dummy;

    dmesg_info("i2c%u: scanning bus...", instance);

    for (uint8_t addr = I2C_SCAN_ADDR_MIN; addr <= I2C_SCAN_ADDR_MAX; addr++) {
        /* Try a zero-byte write to probe for ACK */
        int ret = i2c_write_blocking(inst, addr, &dummy, 0, false);
        if (ret != PICO_ERROR_GENERIC) {
            if (count < max_addrs) {
                found_addrs[count] = addr;
            }
            count++;
            dmesg_debug("i2c%u: found device at 0x%02X", instance, addr);
        }
    }

    dmesg_info("i2c%u: scan complete, %u devices found", instance, count);
#else
    dmesg_info("i2c%u: stub scan, no devices", instance);
#endif

    return count;
}

bool i2c_hal_get_config(uint8_t instance, i2c_config_t *config) {
    if (instance >= I2C_NUM_INSTANCES || !config) {
        return false;
    }

    if (!i2c_configs[instance].initialized) {
        return false;
    }

    *config = i2c_configs[instance];
    return true;
}
