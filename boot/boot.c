#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"

#ifdef PICO_RUNTIME_NO_INIT_CLOCKS
#include "hardware/regs/resets.h"
#include "hardware/regs/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/structs/resets.h"
#include "hardware/clocks.h"

// Emulator mode: provide a minimal clock init that skips PLL/XOSC setup
// and avoids polling CLK_SELECTED registers (which may hang in emulators).
// Sets configured_freq[] directly so clock_get_hz() returns correct values.
void runtime_init_clocks(void) {
    // Disable clock resuscitation
    clocks_hw->resus.ctrl = 0;

    // Set the SDK's internal frequency tracking via public API.
    clock_set_reported_hz(clk_ref,  12000000);
    clock_set_reported_hz(clk_sys,  125000000);
    clock_set_reported_hz(clk_peri, 125000000);
    clock_set_reported_hz(clk_usb,  48000000);
    clock_set_reported_hz(clk_adc,  48000000);
    clock_set_reported_hz(clk_rtc,  46875);

    // Release all peripheral resets (CLR alias = base + 0x3000)
    // Write directly to avoid polling RESET_DONE (emulator may not update it)
    hw_clear_bits(&resets_hw->reset, RESETS_RESET_BITS);
}
#endif

extern void kernel_main(void);

// Entry point expected by Pico SDK crt0.S
int main(void) {

    stdio_init_all();

#ifndef PICO_RUNTIME_NO_INIT_CLOCKS
    // Give UART time to settle on real hardware
    sleep_ms(100);
#endif

    kernel_main();
    return 0;
}
