/* spi.h - SPI Hardware Abstraction Layer for littleOS */
#ifndef LITTLEOS_HAL_SPI_H
#define LITTLEOS_HAL_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RP2040 has 2 SPI peripherals (spi0, spi1) */
#define SPI_NUM_INSTANCES   2

typedef enum {
    SPI_MODE_0 = 0,     /* CPOL=0, CPHA=0 */
    SPI_MODE_1 = 1,     /* CPOL=0, CPHA=1 */
    SPI_MODE_2 = 2,     /* CPOL=1, CPHA=0 */
    SPI_MODE_3 = 3,     /* CPOL=1, CPHA=1 */
} spi_mode_t;

typedef struct {
    uint8_t     instance;       /* 0 or 1 */
    uint8_t     mosi_pin;       /* TX/MOSI GPIO */
    uint8_t     miso_pin;       /* RX/MISO GPIO */
    uint8_t     sck_pin;        /* Clock GPIO */
    uint8_t     cs_pin;         /* Chip select GPIO (managed manually) */
    uint32_t    baudrate;       /* Actual baudrate */
    spi_mode_t  mode;
    uint8_t     data_bits;      /* 4-16 */
    bool        initialized;
} spi_config_t;

/* Initialize SPI instance */
int spi_hal_init(uint8_t instance, uint8_t mosi, uint8_t miso,
                 uint8_t sck, uint8_t cs, uint32_t baudrate, spi_mode_t mode);

/* Deinitialize SPI instance */
int spi_hal_deinit(uint8_t instance);

/* Write data (full-duplex, discard read) */
int spi_hal_write(uint8_t instance, const uint8_t *data, size_t len);

/* Read data (full-duplex, send zeros) */
int spi_hal_read(uint8_t instance, uint8_t *data, size_t len);

/* Full-duplex transfer */
int spi_hal_transfer(uint8_t instance, const uint8_t *tx, uint8_t *rx, size_t len);

/* Chip select control */
void spi_hal_cs_select(uint8_t instance);
void spi_hal_cs_deselect(uint8_t instance);

/* Set baudrate */
int spi_hal_set_baudrate(uint8_t instance, uint32_t baudrate);

/* Get configuration */
bool spi_hal_get_config(uint8_t instance, spi_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_SPI_H */
