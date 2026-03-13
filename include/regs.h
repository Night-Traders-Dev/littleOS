#ifndef REGS_H
#define REGS_H
#include <stdint.h>

/* Prefer SDK-provided register definitions.
 * The addresses below are common to RP2040 and RP2350.
 * For chip-specific registers, use hardware/regs/addressmap.h */
#include "hardware/regs/addressmap.h"

/* Register Access Macros */
#define REG(addr) (*(volatile uint32_t *)(addr))

/* UART0 Register Offsets (common to RP2040/RP2350) */
#define UART0_DR       (UART0_BASE + 0x00)
#define UART0_FR       (UART0_BASE + 0x18)
#define UART0_IBRD     (UART0_BASE + 0x24)
#define UART0_FBRD     (UART0_BASE + 0x28)
#define UART0_LCR_H    (UART0_BASE + 0x2C)
#define UART0_CR       (UART0_BASE + 0x30)

/* Reset register offsets (common to RP2040/RP2350) */
#define RESETS_RESET   (RESETS_BASE + 0x0)
#define RESETS_WDONE   (RESETS_BASE + 0x8)

#endif
