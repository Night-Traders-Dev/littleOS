#include "regs.h"

void uart_init() {
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
