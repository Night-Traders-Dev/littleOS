#ifndef REGS_H
#define REGS_H
#include <stdint.h>

// Base Addresses
#define RESETS_BASE    0x4000C000
#define CLOCKS_BASE    0x40008000
#define UART0_BASE     0x40034000
#define SIO_BASE       0xD0000000
#define IO_BANK0_BASE  0x40014000

// Register Access Macros
#define REG(addr) (*(volatile uint32_t *)(addr))

// Specific Registers
#define RESETS_RESET   (RESETS_BASE + 0x0)
#define RESETS_WDONE   (RESETS_BASE + 0x8)
#define UART0_DR       (UART0_BASE + 0x00)
#define UART0_FR       (UART0_BASE + 0x18)
#define UART0_IBRD     (UART0_BASE + 0x24)
#define UART0_FBRD     (UART0_BASE + 0x28)
#define UART0_LCR_H    (UART0_BASE + 0x2C)
#define UART0_CR       (UART0_BASE + 0x30)

#endif
