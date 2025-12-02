#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"

extern void kernel_main(void);
extern void uart_init(void);

// Entry point expected by Pico SDK crt0.S
int main(void) {
    
    stdio_init_all();
    while (!stdio_usb_connected()) {}
    sleep_ms(1000);
    
    // Initialize UART for shell communication
    uart_init();
    
    kernel_main();
    return 0;
}
