#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"



extern void kernel_main(void);

// Entry point expected by Pico SDK crt0.S
int main(void) {

    stdio_init_all();

    // Skip USB wait for emulator — use UART only
    sleep_ms(100);

    kernel_main();
    return 0;
}
