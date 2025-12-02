// kernel.c
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "sage_embed.h"
#include "config_storage.h"

// Forward declarations
void shell_run(void);
void script_storage_init(void);

// Global SageLang context
sage_context_t* sage_ctx = NULL;

// Core kernel entry point
void kernel_main(void) {
    // The Pico SDK's stdio is already initialized in boot.c
    printf("\r\nRP2040 littleOS kernel\r\n");
    
    // Initialize configuration storage
    config_init();
    
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
    printf("\r\n> ");

    // Start the command shell
    shell_run();
}
