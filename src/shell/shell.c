#include <stdint.h>


#define UART0_BASE 0x40034000u

#define UARTDR   (*(volatile uint32_t*)(UART0_BASE + 0x00))
#define UARTFR   (*(volatile uint32_t*)(UART0_BASE + 0x18))
#define UARTIBRD (*(volatile uint32_t*)(UART0_BASE + 0x24))
#define UARTFBRD (*(volatile uint32_t*)(UART0_BASE + 0x28))
#define UARTLCRH (*(volatile uint32_t*)(UART0_BASE + 0x2C))
#define UARTCR   (*(volatile uint32_t*)(UART0_BASE + 0x30))
#define UARTICR  (*(volatile uint32_t*)(UART0_BASE + 0x44))

#define IO_BANK0_BASE  0x40014000u
#define GPIO0_CTRL    (*(volatile uint32_t*)(IO_BANK0_BASE + 0x004))
#define GPIO1_CTRL    (*(volatile uint32_t*)(IO_BANK0_BASE + 0x00C))
#define SIO_BASE       0xd0000000u
#define GPIO_OE_SET   (*(volatile uint32_t*)(SIO_BASE + 0x024))
#define GPIO_OUT_SET  (*(volatile uint32_t*)(SIO_BASE + 0x014))
#define GPIO_OUT_CLR  (*(volatile uint32_t*)(SIO_BASE + 0x018))

static void gpio_init_uart0_pins(void) {
    GPIO0_CTRL = 2; // UART0_TX
    GPIO1_CTRL = 2; // UART0_RX
}

void little_uart_init(void) {
    gpio_init_uart0_pins();

    UARTCR = 0;
    UARTICR = 0x7FF;

    UARTIBRD = 67; // 115200 @ 125 MHz approx.
    UARTFBRD = 52;

    UARTLCRH = (3 << 5) | (1 << 4);
    UARTCR = (1 << 0) | (1 << 8) | (1 << 9);
}

void char_putc(char c) {
    while (UARTFR & (1 << 5)) {
    }
    UARTDR = (uint32_t)c;
}

char char_getc(void) {
    while (UARTFR & (1 << 4)) {
    }
    return (char)(UARTDR & 0xFF);
}

void char_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            char_putc('\r');
        char_putc(*s++);
    }
}

#define CMD_BUF_LEN 64

static void handle_command(const char *cmd) {
    if (!cmd[0]) return;

    if (cmd[0] == 'h') {
        char_puts("\r\nCommands: h=help, l=led toggle, r=reboot\r\n");
    } else if (cmd[0] == 'l') {
        char_puts("\r\n[toggle LED stub]\r\n");
    } else if (cmd[0] == 'r') {
        char_puts("\r\n[reboot stub]\r\n");
    } else {
        char_puts("\r\nUnknown\r\n");
    }
}

void shell_run(void) {
    char buf[CMD_BUF_LEN];
    unsigned idx = 0;

    while (1) {
        char c = char_getc();

        if (c == '\r' || c == '\n') {
            buf[idx] = 0;
            char_puts("\r\n");
            handle_command(buf);
            idx = 0;
            char_puts("> ");
        } else if (c == 0x7F || c == '\b') {
            if (idx > 0) {
                idx--;
                char_puts("\b \b");
            }
        } else if (idx < CMD_BUF_LEN - 1) {
            buf[idx++] = c;
            char_putc(c);
        }
    }
}
