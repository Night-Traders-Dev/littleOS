// kernel.c
#include <stdint.h>
#include <stdio.h>

void little_uart_init(void);
void shell_run(void);
void char_puts(const char *s);

// Core kernel entry point used by the rest of your code
void kernel_main(void) {
    little_uart_init();

    char_puts("\r\nRP2040 tiny OS kernel\r\n> ");

    shell_run();
}


