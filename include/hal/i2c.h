/* i2c.h - I2C Hardware Abstraction Layer for littleOS */
#ifndef LITTLEOS_HAL_I2C_H
#define LITTLEOS_HAL_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RP2040 has 2 I2C peripherals (i2c0, i2c1) */
#define I2C_NUM_INSTANCES   2
#define I2C_MAX_TRANSFER    256

typedef enum {
    I2C_SPEED_STANDARD  = 100000,   /* 100 kHz */
    I2C_SPEED_FAST      = 400000,   /* 400 kHz */
    I2C_SPEED_FASTPLUS  = 1000000,  /* 1 MHz */
} i2c_speed_t;

typedef struct {
    uint8_t     instance;       /* 0 or 1 */
    uint8_t     sda_pin;        /* GPIO pin for SDA */
    uint8_t     scl_pin;        /* GPIO pin for SCL */
    uint32_t    baudrate;       /* Actual baudrate achieved */
    bool        initialized;
} i2c_config_t;

/* Initialize I2C instance */
int i2c_hal_init(uint8_t instance, uint8_t sda_pin, uint8_t scl_pin, i2c_speed_t speed);

/* Deinitialize I2C instance */
int i2c_hal_deinit(uint8_t instance);

/* Write data to I2C device */
int i2c_hal_write(uint8_t instance, uint8_t addr, const uint8_t *data, size_t len);

/* Read data from I2C device */
int i2c_hal_read(uint8_t instance, uint8_t addr, uint8_t *data, size_t len);

/* Write then read (register read pattern) */
int i2c_hal_write_read(uint8_t instance, uint8_t addr,
                       const uint8_t *write_data, size_t write_len,
                       uint8_t *read_data, size_t read_len);

/* Write a single register */
int i2c_hal_write_reg(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t value);

/* Read a single register */
int i2c_hal_read_reg(uint8_t instance, uint8_t addr, uint8_t reg, uint8_t *value);

/* Scan I2C bus for devices */
int i2c_hal_scan(uint8_t instance, uint8_t *found_addrs, uint8_t max_addrs);

/* Get configuration */
bool i2c_hal_get_config(uint8_t instance, i2c_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_I2C_H */
