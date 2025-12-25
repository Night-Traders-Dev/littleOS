#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"



extern void kernel_main(void);

// Entry point expected by Pico SDK crt0.S
int main(void) {

    stdio_init_all();

    // Wait for USB connection if using USB stdio
    // Comment out these lines if connecting via hardware UART
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(1000);

    kernel_main();
    return 0;
}
