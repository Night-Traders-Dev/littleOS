#include "regs.h"

// Forward declarations
void uart_init();
void shell_run();

int main() {
    // 1. Hardware Initialization
    uart_init();

    // 2. Start Shell
    shell_run();

    // Should not reach here
    while(1);
}
