// kernel.c
#include <stdio.h>

// Forward declaration for the shell entry point
void shell_run(void);

// Core kernel entry point
void kernel_main(void) {
    // The Pico SDK's stdio is already initialized in boot.c
    printf("\r\nRP2040 tiny OS kernel\r\n> ");

    // Start the command shell
    shell_run();
}
