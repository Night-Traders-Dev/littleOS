// kernel.c
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "sage_embed.h"
#include "config_storage.h"
#include "watchdog.h"
#include "supervisor.h"
#include "dmesg.h"

// Forward declarations
void shell_run(void);
void script_storage_init(void);

// Global SageLang context
sage_context_t* sage_ctx = NULL;

// Core kernel entry point
void kernel_main(void) {
    // Initialize dmesg FIRST - allows all subsequent boot stages to log
    dmesg_init();
    
    // The Pico SDK's stdio is already initialized in boot.c
    printf("\r\nRP2040 littleOS kernel\r\n");
    dmesg_info("RP2040 littleOS kernel starting");
    
    // Initialize watchdog timer (8 second timeout)
    // Don't enable yet - wait until after boot completes
    wdt_init(8000);
    dmesg_info("Watchdog timer initialized (8s timeout)");
    
    // Check if we recovered from a watchdog reset
    if (wdt_get_reset_reason() == WATCHDOG_RESET_TIMEOUT) {
        printf("\r\n*** RECOVERED FROM CRASH ***\r\n");
        printf("System was reset by watchdog timer\r\n\r\n");
        dmesg_crit("System recovered from watchdog reset");
        sleep_ms(2000);  // Give user time to see message
    }
    
    // Initialize configuration storage
    config_init();
    dmesg_info("Configuration storage initialized");
    
    // Initialize SageLang
    sage_ctx = sage_init();
    if (sage_ctx) {
        printf("SageLang initialized\r\n");
        dmesg_info("SageLang interpreter initialized");
    } else {
        printf("Warning: SageLang initialization failed\r\n");
        dmesg_err("SageLang initialization failed");
    }
    
    // Initialize script storage system
    script_storage_init();
    printf("Script storage initialized\r\n");
    dmesg_info("Script storage system initialized");
    
    // Check for autoboot script
    if (config_has_autoboot()) {
        char autoboot_script[CONFIG_AUTOBOOT_SCRIPT_SIZE];
        if (config_get_autoboot(autoboot_script, sizeof(autoboot_script))) {
            printf("\r\nRunning autoboot script...\r\n");
            dmesg_info("Executing autoboot script");
            sage_result_t result = sage_eval_string(sage_ctx, autoboot_script, 
                                                     strlen(autoboot_script));
            if (result != SAGE_OK) {
                printf("Warning: Autoboot script error\r\n");
                dmesg_warn("Autoboot script execution failed");
            } else {
                printf("Autoboot complete\r\n");
                dmesg_info("Autoboot script completed successfully");
            }
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
    dmesg_info("Watchdog enabled - monitoring for system hangs");
    
    // Start supervisor on Core 1 for system health monitoring
    supervisor_init();
    printf("Supervisor: Core 1 monitoring system health\r\n");
    dmesg_info("Supervisor launched on Core 1");
    
    dmesg_info("Boot sequence complete - entering shell");
    printf("\r\n> ");

    // Start the command shell (this will send heartbeats to supervisor)
    shell_run();
}
