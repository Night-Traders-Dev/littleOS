#ifndef UART_H
#define UART_H

#include <stdint.h>

/**
 * Initialize UART0 subsystem
 * Sets up internal buffers and enables output by default
 */
void littleos_uart_init(void); 

/**
 * Enable or disable UART0 output capture
 * @param enable: 1 to enable output, 0 to disable
 */
void uart_enable_output(int enable);

/**
 * Write a single character to UART0
 * Character is buffered and echoed to stdout in real-time
 * @param c: Character to transmit
 */
void uart_put_c(char c);

/**
 * Write a null-terminated string to UART0
 * Internally calls uart_put_char() for each character
 * @param str: String to transmit
 */
void uart_put_str(const char *str);

/**
 * Check if UART0 TX buffer has pending data
 * @return: Number of characters in buffer (0 if empty)
 */
int uart_has_data(void);

/**
 * Read a character from UART0 TX buffer
 * Removes character from buffer (FIFO pop)
 * @return: Character from buffer, or '\0' if empty
 */
char uart_get_c(void);

/**
 * Get current UART0 TX buffer fill level
 * @return: Number of characters currently buffered
 */
int uart_buffer_count(void);

/**
 * Flush UART0 output to stdout
 * Ensures all buffered data is displayed
 */
void uart_flush(void);

#endif // UART_H
