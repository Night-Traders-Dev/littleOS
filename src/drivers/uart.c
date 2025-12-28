#include "regs.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void littleos_uart_init() {
    REG(RESETS_RESET) &= ~(1 << 22);
    REG(RESETS_RESET) &= ~(1 << 5);
    while ((REG(RESETS_WDONE) & (1 << 22)) == 0);

    REG(UART0_IBRD) = 67;
    REG(UART0_FBRD) = 52;

    REG(UART0_LCR_H) = (1 << 4) | (3 << 5);

    REG(UART0_CR) = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c) {
    while (REG(UART0_FR) & (1 << 5));
    REG(UART0_DR) = c;
}

char uart_getc() {
    while (REG(UART0_FR) & (1 << 4));
    return (char)(REG(UART0_DR) & 0xFF);
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

#define UART_TX_BUFFER_SIZE 8192

typedef struct {
    char buffer[UART_TX_BUFFER_SIZE];
    int head;
    int tail;
    int count;
} uart_buffer_t;

static uart_buffer_t uart0_buffer = {0};
static int uart_output_enabled = 0;
static uint32_t uart_tx_count = 0;
static uint32_t uart_overflow_count = 0;

void uart_enable_output(int enable) {
    uart_output_enabled = enable ? 1 : 0;
}

void uart_put_c(char c) {
    if (!uart_output_enabled) {
        return;
    }

    if (uart0_buffer.count < UART_TX_BUFFER_SIZE) {
        uart0_buffer.buffer[uart0_buffer.head] = c;
        uart0_buffer.head = (uart0_buffer.head + 1) % UART_TX_BUFFER_SIZE;
        uart0_buffer.count++;
        uart_tx_count++;
    } else {
        uart_overflow_count++;
    }

    fputc((unsigned char)c, stdout);
    fflush(stdout);
}

void uart_put_str(const char *str) {
    if (!str) {
        return;
    }

    while (*str) {
        uart_put_c(*str++);
    }
}

int uart_has_data(void) {
    return uart0_buffer.count;
}

int uart_buffer_count(void) {
    return uart0_buffer.count;
}

char uart_get_c(void) {
    if (uart0_buffer.count == 0) {
        return '\0';
    }

    char c = uart0_buffer.buffer[uart0_buffer.tail];
    uart0_buffer.tail = (uart0_buffer.tail + 1) % UART_TX_BUFFER_SIZE;
    uart0_buffer.count--;
    return c;
}

void uart_flush(void) {
    fflush(stdout);
}

void uart_get_stats(uint32_t *tx_count, uint32_t *overflow_count) {
    if (tx_count) {
        *tx_count = uart_tx_count;
    }
    if (overflow_count) {
        *overflow_count = uart_overflow_count;
    }
}

void uart_clear_buffer(void) {
    uart0_buffer.head = 0;
    uart0_buffer.tail = 0;
    uart0_buffer.count = 0;
}
