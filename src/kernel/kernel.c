// kernel.c
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "sage_embed.h"
#include "config_storage.h"
#include "watchdog.h"
#include "supervisor.h"
#include "dmesg.h"
#include "users_config.h"
#include "permissions.h"


// Forward declarations
void shell_run(void);
void script_storage_init(void);


// Global SageLang context
sage_context_t* sage_ctx = NULL;


// Helper function to print capability flags in human-readable format
static void print_capabilities(uint32_t caps) {
    if (caps == CAP_ALL) {
        printf("ALL");
        return;
    }
    if (caps == 0) {
        printf("NONE");
        return;
    }
    
    bool first = true;
    if (caps & CAP_SYS_ADMIN) {
        printf("%sSYS_ADMIN", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_SYS_BOOT) {
        printf("%sSYS_BOOT", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_GPIO_WRITE) {
        printf("%sGPIO_WRITE", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_UART_CONFIG) {
        printf("%sUART_CONFIG", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_TASK_SPAWN) {
        printf("%sTASK_SPAWN", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_TASK_KILL) {
        printf("%sTASK_KILL", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_MEM_LOCK) {
        printf("%sMEM_LOCK", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_NET_ADMIN) {
        printf("%sNET_ADMIN", first ? "" : "|");
        first = false;
    }
}

// Pretty-print user database during boot
static void print_user_info(void) {
    uint16_t user_count = users_get_count();
    
    printf("\r\n=== User Account Configuration ===\r\n");
    printf("Total accounts: %d\r\n\r\n", user_count);
    
    for (uint16_t i = 0; i < user_count; i++) {
        const user_account_t *user = users_get_by_index(i);
        if (!user) continue;
        
        printf("[%d] %s\r\n", i, user->username);
        printf("    UID:          %d\r\n", user->uid);
        printf("    GID:          %d\r\n", user->gid);
        printf("    Umask:        0%03o\r\n", user->umask);
        printf("    Capabilities: ");
        print_capabilities(user->capabilities);
        printf("\r\n");
        
        if (i < user_count - 1) {
            printf("\r\n");
        }
    }
    
    printf("==================================\r\n");
}


// Core kernel entry point
void kernel_main(void) {
    // Initialize dmesg FIRST - allows all subsequent boot stages to log
    dmesg_init();
    
    // The Pico SDK's stdio is already initialized in boot.c
    printf("\r\n");
    printf("========================================\r\n");
    printf("  RP2040 littleOS Kernel\r\n");
    printf("  Built: %s %s\r\n", __DATE__, __TIME__);
    printf("========================================\r\n");
    dmesg_info("RP2040 littleOS kernel starting");
    
    // Initialize watchdog timer (8 second timeout)
    // Don't enable yet - wait until after boot completes
    wdt_init(8000);
    dmesg_info("Watchdog timer initialized (8s timeout)");
    
    // Check if we recovered from a watchdog reset
    if (wdt_get_reset_reason() == WATCHDOG_RESET_TIMEOUT) {
        printf("\r\n");
        printf("*** RECOVERED FROM CRASH ***\r\n");
        printf("System was reset by watchdog timer\r\n");
        printf("\r\n");
        dmesg_crit("System recovered from watchdog reset");
        sleep_ms(2000);  // Give user time to see message
    }
    
    // Initialize configuration storage
    config_init();
    dmesg_info("Configuration storage initialized");

    // Initialize user database
    printf("\r\nInitializing user database...\r\n");
    users_init();
    dmesg_info("User database initialized");
    
    // Display user account information
    print_user_info();
    
    // Create root security context
    task_sec_ctx_t root_ctx = users_root_context();
    printf("\r\nCreated root security context (UID=%d, GID=%d)\r\n", 
           root_ctx.uid, root_ctx.gid);
    dmesg_info("Root security context created");

    // Create UART device with permissions
    printf("\r\nSetting up device permissions...\r\n");
    resource_perm_t uart0_perms = perm_resource_create(
        UID_ROOT,           // Owner: root
        GID_DRIVERS,        // Group: drivers
        PERM_0660,          // rw-rw----
        RESOURCE_DEVICE
    );
    printf("  UART0: owner=root, group=drivers, mode=0660\r\n");
    dmesg_info("UART0 device permissions configured (rw-rw----)");
    
    // Initialize SageLang
    printf("\r\nInitializing SageLang interpreter...\r\n");
    sage_ctx = sage_init();
    if (sage_ctx) {
        printf("  SageLang ready\r\n");
        dmesg_info("SageLang interpreter initialized");
    } else {
        printf("  Warning: SageLang initialization failed\r\n");
        dmesg_err("SageLang initialization failed");
    }
    
    // Initialize script storage system
    script_storage_init();
    printf("  Script storage initialized\r\n");
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
                printf("  Warning: Autoboot script error\r\n");
                dmesg_warn("Autoboot script execution failed");
            } else {
                printf("  Autoboot complete\r\n");
                dmesg_info("Autoboot script completed successfully");
            }
        }
    }
    
    // Display boot log for 2 seconds
    printf("\r\n");
    printf("Boot sequence complete. Starting shell in 2 seconds...\r\n");
    sleep_ms(2000);
    
    // Clear screen (ANSI escape code)
    printf("\033[2J\033[H");
    
    // Display welcome message
    printf("\r\n");
    printf("========================================\r\n");
    printf("  Welcome to littleOS Shell!\r\n");
    printf("========================================\r\n");
    printf("Type 'help' for available commands\r\n");
    printf("\r\n");
    
    // NOW enable watchdog - boot is complete, start monitoring for hangs
    wdt_enable(8000);  // 8 second timeout
    printf("✓ Watchdog: Active (8s timeout - auto-recovery enabled)\r\n");
    dmesg_info("Watchdog enabled - monitoring for system hangs");
    
    // Start supervisor on Core 1 for system health monitoring
    supervisor_init();
    printf("✓ Supervisor: Core 1 monitoring system health\r\n");
    dmesg_info("Supervisor launched on Core 1");
    
    // Show current user context
    printf("✓ Running as: %s (UID=%d, GID=%d)\r\n", 
           "root", root_ctx.uid, root_ctx.gid);
    
    dmesg_info("Boot sequence complete - entering shell");
    printf("\r\n> ");

    // Start the command shell (this will send heartbeats to supervisor)
    shell_run();
}
