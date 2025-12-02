#include "reg.h"

// Defined in uart.c
void uart_puts(const char *s);
char uart_getc();
void uart_putc(char c);

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void shell_run() {
    char buffer[64];
    int idx = 0;

    uart_puts("\r\nWelcome to littleOS Shell!\r\n> ");

    while (1) {
        char c = uart_getc();

        // Echo character
        uart_putc(c);

        if (c == '\r') { // Enter key
            uart_putc('\n');
            buffer[idx] = '\0';

            if (strcmp(buffer, "help") == 0) {
                uart_puts("Available commands: help, version, reboot\r\n");
            } else if (strcmp(buffer, "version") == 0) {
                uart_puts("littleOS v0.1.0 - RP2040\r\n");
            } else if (strcmp(buffer, "reboot") == 0) {
                uart_puts("Rebooting...\r\n");
                // Trigger watchdog reset or similar (omitted)
            } else if (idx > 0) {
                uart_puts("Unknown command.\r\n");
            }

            idx = 0;
            uart_puts("> ");
        } else if (c == '\b' || c == 0x7F) { // Backspace
            if (idx > 0) idx--;
        } else {
            if (idx < 63) buffer[idx++] = c;
        }
    }
}
