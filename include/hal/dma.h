/* dma.h - DMA Engine HAL for littleOS */
#ifndef LITTLEOS_HAL_DMA_H
#define LITTLEOS_HAL_DMA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RP2040 has 12 DMA channels */
#define DMA_NUM_CHANNELS    12

/* Transfer data sizes */
typedef enum {
    DMA_XFER_SIZE_8  = 0,    /* Byte transfer */
    DMA_XFER_SIZE_16 = 1,    /* Halfword transfer */
    DMA_XFER_SIZE_32 = 2,    /* Word transfer */
} dma_transfer_size_t;

/* DMA DREQ (Data Request) sources */
typedef enum {
    DMA_DREQ_PIO0_TX0 = 0,
    DMA_DREQ_PIO0_TX1 = 1,
    DMA_DREQ_PIO0_TX2 = 2,
    DMA_DREQ_PIO0_TX3 = 3,
    DMA_DREQ_PIO0_RX0 = 4,
    DMA_DREQ_PIO0_RX1 = 5,
    DMA_DREQ_PIO0_RX2 = 6,
    DMA_DREQ_PIO0_RX3 = 7,
    DMA_DREQ_PIO1_TX0 = 8,
    DMA_DREQ_PIO1_TX1 = 9,
    DMA_DREQ_PIO1_TX2 = 10,
    DMA_DREQ_PIO1_TX3 = 11,
    DMA_DREQ_PIO1_RX0 = 12,
    DMA_DREQ_PIO1_RX1 = 13,
    DMA_DREQ_PIO1_RX2 = 14,
    DMA_DREQ_PIO1_RX3 = 15,
    DMA_DREQ_SPI0_TX  = 16,
    DMA_DREQ_SPI0_RX  = 17,
    DMA_DREQ_SPI1_TX  = 18,
    DMA_DREQ_SPI1_RX  = 19,
    DMA_DREQ_UART0_TX = 20,
    DMA_DREQ_UART0_RX = 21,
    DMA_DREQ_UART1_TX = 22,
    DMA_DREQ_UART1_RX = 23,
    DMA_DREQ_ADC      = 36,
    DMA_DREQ_FORCE    = 0x3F,  /* Unpaced / immediate */
} dma_dreq_t;

/* DMA channel status */
typedef struct {
    bool     claimed;        /* Channel claimed by us */
    bool     busy;           /* Transfer in progress */
    uint32_t transfer_count; /* Remaining transfer count */
    uint32_t total_transfers;/* Total completed transfers */
    uint32_t total_bytes;    /* Total bytes transferred */
} dma_channel_status_t;

/* DMA transfer descriptor */
typedef struct {
    volatile void *read_addr;   /* Source address */
    volatile void *write_addr;  /* Destination address */
    uint32_t transfer_count;    /* Number of transfers */
    dma_transfer_size_t size;   /* Transfer size */
    dma_dreq_t dreq;            /* Data request source */
    bool read_increment;        /* Increment read address */
    bool write_increment;       /* Increment write address */
    bool ring_read;             /* Ring buffer on read side */
    bool ring_write;            /* Ring buffer on write side */
    uint8_t ring_size_bits;     /* Ring buffer size (power of 2) */
    int chain_to;               /* Chain to channel (-1 = none) */
    bool irq_quiet;             /* Don't generate IRQ */
} dma_transfer_t;

/* Completion callback */
typedef void (*dma_callback_t)(int channel, void *user_data);

/* Init DMA subsystem */
int dma_hal_init(void);

/* Claim/release a DMA channel (-1 = auto-assign) */
int dma_hal_claim(int channel);
int dma_hal_release(int channel);

/* Configure and start a transfer */
int dma_hal_start(int channel, const dma_transfer_t *transfer);

/* Memory-to-memory copy (convenience) */
int dma_hal_memcpy(int channel, void *dst, const void *src, size_t len);

/* Wait for channel to complete */
int dma_hal_wait(int channel, uint32_t timeout_ms);

/* Check if channel is busy */
bool dma_hal_busy(int channel);

/* Abort a transfer */
int dma_hal_abort(int channel);

/* Get channel status */
int dma_hal_status(int channel, dma_channel_status_t *status);

/* Set completion callback */
int dma_hal_set_callback(int channel, dma_callback_t cb, void *user_data);

/* Sniff/checksum: configure DMA sniffer (CRC32, sum, etc.) */
typedef enum {
    DMA_SNIFF_CRC32  = 0,
    DMA_SNIFF_CRC32R = 1,  /* Bit-reversed CRC32 */
    DMA_SNIFF_CRC16  = 2,
    DMA_SNIFF_SUM    = 0xF,/* Byte sum */
} dma_sniff_mode_t;

int dma_hal_sniff_enable(int channel, dma_sniff_mode_t mode);
uint32_t dma_hal_sniff_result(void);
void dma_hal_sniff_reset(uint32_t seed);

/* Timer-paced DMA: set fractional divider for timer-paced transfers */
int dma_hal_set_timer(uint8_t timer_num, uint16_t numerator, uint16_t denominator);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_DMA_H */
