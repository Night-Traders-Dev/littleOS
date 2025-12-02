// kernel.c
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "sage_embed.h"
#include "config_storage.h"
#include "watchdog.h"
#include "multicore.h"

// Forward declarations
void shell_run(void);
void script_storage_init(void);

// Global SageLang context
sage_context_t* sage_ctx = NULL;

// Core kernel entry point
void kernel_main(void) {
    // The Pico SDK's stdio is already initialized in boot.c
    printf("\r\nRP2040 littleOS kernel\r\n");
    
    // Initialize watchdog timer (8 second timeout)
    // Don't enable yet - wait until after boot completes
    wdt_init(8000);
    
    // Check if we recovered from a watchdog reset
    if (wdt_get_reset_reason() == WATCHDOG_RESET_TIMEOUT) {
        printf("\r\n*** RECOVERED FROM CRASH ***\r\n");
        printf("System was reset by watchdog timer\r\n\r\n");
        sleep_ms(2000);  // Give user time to see message
    }
    
    // Initialize configuration storage
    config_init();
    
    // Initialize multi-core system
    multicore_init();
    printf("Multi-core system initialized\r\n");
    
    // Initialize SageLang
    sage_ctx = sage_init();
    if (sage_ctx) {
        printf("SageLang initialized\r\n");
    } else {
        printf("Warning: SageLang initialization failed\r\n");
    }
    
    // Initialize script storage system
    script_storage_init();
    printf("Script storage initialized\r\n");
    
    // Check for autoboot script
    if (config_has_autoboot()) {
        char autoboot_script[CONFIG_AUTOBOOT_SCRIPT_SIZE];
        if (config_get_autoboot(autoboot_script, sizeof(autoboot_script))) {
            printf("\r\nRunning autoboot script...\r\n");
            sage_result_t result = sage_eval_string(sage_ctx, autoboot_script, 
                                                     strlen(autoboot_script));
            if (result != SAGE_OK) {
                printf("Warning: Autoboot script error\r\n");
            }
            printf("Autoboot complete\r\n");
        }
    }
    
    // Display boot log for 3 seconds
    printf("\r\n");
    sleep_ms(3000);
    
    // Clear screen (ANSI escape code)
    printf("\033[2J\033[H");
    
    // Display welcome message
    printf("\r\n");
    printf("Welcome to littleOS Shell!\r\n");
    printf("Type 'help' for available commands\r\n");
    printf("\r\n");
    
    // NOW enable watchdog - boot is complete, start monitoring for hangs
    wdt_enable(8000);  // 8 second timeout
    printf("Watchdog: Active (8s timeout - system will auto-recover from hangs)\r\n");
    printf("Multi-core: Ready (use core1_launch_script or core1_launch_code)\r\n");
    printf("\r\n> ");

    // Start the command shell (this will feed watchdog periodically)
    shell_run();
}
