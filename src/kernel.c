// kernel.c
#include <stdio.h>
#include "sage_embed.h"

// Forward declarations
void shell_run(void);
void script_storage_init(void);

// Global SageLang context
sage_context_t* sage_ctx = NULL;

// Core kernel entry point
void kernel_main(void) {
    // The Pico SDK's stdio is already initialized in boot.c
    printf("\r\nRP2040 tiny OS kernel\r\n");
    
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
    
    printf("> ");

    // Start the command shell
    shell_run();
}
