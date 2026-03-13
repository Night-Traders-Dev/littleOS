/* pio.h - PIO (Programmable I/O) HAL for littleOS */
#ifndef LITTLEOS_HAL_PIO_H
#define LITTLEOS_HAL_PIO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hardware/platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PIO block/SM counts from SDK (RP2040: 2 blocks, RP2350: 3 blocks) */
#define PIO_NUM_BLOCKS      NUM_PIOS
#define PIO_NUM_SM           NUM_PIO_STATE_MACHINES
#define PIO_MAX_PROGRAM_LEN  32  /* Max instructions per program */

/* PIO program descriptor */
typedef struct {
    uint16_t instructions[PIO_MAX_PROGRAM_LEN];
    uint8_t  length;         /* Number of instructions */
    uint8_t  origin;         /* Load offset (-1 = auto) */
    uint8_t  wrap_target;    /* Wrap target instruction */
    uint8_t  wrap;           /* Wrap instruction */
    bool     loaded;
} pio_hal_program_t;

/* State machine configuration */
typedef struct {
    uint8_t  pio_block;      /* 0 or 1 */
    uint8_t  sm;             /* State machine 0-3 */
    float    clkdiv;         /* Clock divider */
    uint8_t  pin_base;       /* Base pin */
    uint8_t  pin_count;      /* Number of pins */
    uint8_t  sideset_pins;   /* Number of sideset pins */
    bool     autopull;       /* Enable autopull */
    bool     autopush;       /* Enable autopush */
    uint8_t  pull_threshold; /* Pull threshold bits */
    uint8_t  push_threshold; /* Push threshold bits */
    bool     shift_right;    /* Shift direction for OSR */
    bool     in_shift_right; /* Shift direction for ISR */
} pio_hal_sm_config_t;

/* State machine status */
typedef struct {
    bool     active;
    bool     program_loaded;
    uint8_t  program_offset;
    uint8_t  program_length;
    float    clkdiv;
    uint8_t  pin_base;
    uint8_t  pin_count;
    uint32_t tx_stalls;      /* FIFO stall count */
    uint32_t rx_stalls;
} pio_sm_status_t;

/* Init PIO subsystem */
int pio_hal_init(void);

/* Load a PIO program from instruction array */
int pio_hal_load_program(uint8_t pio_block, const uint16_t *instructions,
                         uint8_t length, uint8_t origin);

/* Unload program from a PIO block */
int pio_hal_unload_program(uint8_t pio_block);

/* Configure a state machine */
int pio_hal_sm_config(uint8_t pio_block, uint8_t sm, const pio_hal_sm_config_t *cfg);

/* Start/stop a state machine */
int pio_hal_sm_start(uint8_t pio_block, uint8_t sm);
int pio_hal_sm_stop(uint8_t pio_block, uint8_t sm);

/* Restart a state machine (reset PC to wrap_target) */
int pio_hal_sm_restart(uint8_t pio_block, uint8_t sm);

/* FIFO operations */
int pio_hal_sm_put(uint8_t pio_block, uint8_t sm, uint32_t data);
int pio_hal_sm_get(uint8_t pio_block, uint8_t sm, uint32_t *data);
bool pio_hal_sm_tx_empty(uint8_t pio_block, uint8_t sm);
bool pio_hal_sm_rx_empty(uint8_t pio_block, uint8_t sm);

/* Execute a single instruction on a state machine */
int pio_hal_sm_exec(uint8_t pio_block, uint8_t sm, uint16_t instruction);

/* Get state machine status */
int pio_hal_sm_status(uint8_t pio_block, uint8_t sm, pio_sm_status_t *status);

/* Set pin direction for PIO pins */
int pio_hal_set_pin_dir(uint8_t pio_block, uint8_t sm, uint8_t pin, bool output);

/* Built-in program: blink (toggle pin at frequency) */
int pio_hal_blink_program(uint8_t pio_block, uint8_t sm, uint8_t pin, float freq_hz);

/* Built-in program: WS2812 (NeoPixel) driver */
int pio_hal_ws2812_init(uint8_t pio_block, uint8_t sm, uint8_t pin);
int pio_hal_ws2812_put(uint8_t pio_block, uint8_t sm, uint32_t grb);

/* Built-in program: UART TX at arbitrary baud */
int pio_hal_uart_tx_init(uint8_t pio_block, uint8_t sm, uint8_t pin, uint32_t baud);
int pio_hal_uart_tx_put(uint8_t pio_block, uint8_t sm, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_HAL_PIO_H */
