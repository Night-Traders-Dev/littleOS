#include "regs.h"

void littleos_uart_init() {
    // 1. Deassert reset for UART0 and IO Bank 0
    REG(RESETS_RESET) &= ~(1 << 22); // UART0
    REG(RESETS_RESET) &= ~(1 << 5);  // IO Bank 0
    while ((REG(RESETS_WDONE) & (1 << 22)) == 0);

    // 2. Set up GPIO 0 (TX) and 1 (RX) - Simplified pin muxing
    // (Note: Real implementation requires GPIO_CTRL registers setup here)

    // 3. Set Baud Rate (Assuming 125MHz clock, target 115200)
    // Integer part: 67, Fractional part: 52
    REG(UART0_IBRD) = 67;
    REG(UART0_FBRD) = 52;

    // 4. Enable FIFO and 8-bit data length
    REG(UART0_LCR_H) = (1 << 4) | (3 << 5);

    // 5. Enable UART, TX, RX
    REG(UART0_CR) = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c) {
    // Wait until TX FIFO is not full
    while (REG(UART0_FR) & (1 << 5));
    REG(UART0_DR) = c;
}

char uart_getc() {
    // Wait until RX FIFO is not empty
    while (REG(UART0_FR) & (1 << 4));
    return (char)(REG(UART0_DR) & 0xFF);
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "uart.h"

/**
 * UART0 TX Buffer Management
 *
 * Implements a circular FIFO buffer for UART0 transmit data.
 * Characters written to the UART0 data register (0x40034000) are
 * captured, buffered, and echoed to stdout in real-time.
 *
 * This allows the Bramble emulator to display serial output from
 * littleOS running on the simulated RP2040.
 */

#define UART_TX_BUFFER_SIZE 8192

typedef struct {
    char buffer[UART_TX_BUFFER_SIZE];
    int head;  /* Write pointer */
    int tail;  /* Read pointer */
    int count; /* Number of characters in buffer */
} uart_buffer_t;

/* Global UART0 TX buffer */
static uart_buffer_t uart0_buffer = {0};

/* UART output enable flag */
static int uart_output_enabled = 0;

/* Statistics (optional) */
static uint32_t uart_tx_count = 0;
static uint32_t uart_overflow_count = 0;


/**
 * uart_enable_output()
 *
 * Control whether UART0 characters are echoed to stdout.
 * When disabled, characters are still buffered but not displayed.
 * Useful for suppressing verbose output.
 *
 * @param enable: 1 = output enabled, 0 = output disabled
 */
void uart_enable_output(int enable) {
    uart_output_enabled = enable ? 1 : 0;
}

/**
 * uart_put_c()
 *
 * Write a single character to UART0.
 * Called whenever littleOS writes to the UART0 data register (0x40034000).
 *
 * Actions:
 * 1. Add character to circular buffer
 * 2. Echo to stdout immediately (if enabled)
 * 3. Flush stdout for real-time display
 *
 * @param c: Character to transmit (typically extracted from MMIO value & 0xFF)
 */
void uart_put_c(char c) {
    if (!uart_output_enabled) {
        return;
    }

    /* Add to circular buffer */
    if (uart0_buffer.count < UART_TX_BUFFER_SIZE) {
        uart0_buffer.buffer[uart0_buffer.head] = c;
        uart0_buffer.head = (uart0_buffer.head + 1) % UART_TX_BUFFER_SIZE;
        uart0_buffer.count++;
        uart_tx_count++;
    } else {
        /* Buffer overflow - count but don't lose data structure */
        uart_overflow_count++;
    }

    /* Echo to stdout immediately for real-time display */
    fputc((unsigned char)c, stdout);
    fflush(stdout);
}

/**
 * uart_put_str()
 *
 * Write a null-terminated string to UART0.
 * Internally calls uart_put_c() for each character.
 *
 * @param str: Null-terminated string to transmit
 */
void uart_put_str(const char *str) {
    if (!str) {
        return;
    }

    while (*str) {
        uart_put_c(*str++);
    }
}

/**
 * uart_has_data()
 *
 * Check if UART0 TX buffer contains pending data.
 *
 * @return: Number of characters currently in buffer (0 if empty)
 */
int uart_has_data(void) {
    return uart0_buffer.count;
}

/**
 * uart_buffer_count()
 *
 * Get the number of characters currently buffered in UART0 TX.
 *
 * @return: Number of characters in buffer (0 to UART_TX_BUFFER_SIZE)
 */
int uart_buffer_count(void) {
    return uart0_buffer.count;
}

/**
 * uart_get_c()
 *
 * Read and remove a character from the UART0 TX buffer.
 * Operates as FIFO: first character written is first character read.
 *
 * @return: Character from buffer, or '\0' if buffer is empty
 */
char uart_get_c(void) {
    if (uart0_buffer.count == 0) {
        return '\0';
    }

    char c = uart0_buffer.buffer[uart0_buffer.tail];
    uart0_buffer.tail = (uart0_buffer.tail + 1) % UART_TX_BUFFER_SIZE;
    uart0_buffer.count--;
    return c;
}

/**
 * uart_flush()
 *
 * Ensure all UART0 output has been flushed to stdout.
 * Called at program shutdown to guarantee no buffered output is lost.
 */
void uart_flush(void) {
    fflush(stdout);
}
