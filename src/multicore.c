#include "multicore.h"
#include "sage_embed.h"
#include "script_storage.h"
#include "watchdog.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#endif

// Core 1 state management
static volatile core1_state_t core1_state = CORE1_STATE_IDLE;
static char core1_script_name[MULTICORE_MAX_SCRIPT_NAME] = {0};
static char* core1_code_buffer = NULL;
static size_t core1_code_buffer_size = 0;

// SageLang context for Core 1 (separate from Core 0)
static sage_context_t* core1_sage_ctx = NULL;

/**
 * @brief Core 1 entry point - executes SageLang code
 */
static void core1_entry(void) {
#ifdef PICO_BUILD
    // Initialize stdio for Core 1 (thread-safe printf)
    // Note: stdio_init_all() already called on Core 0, this is safe
    
    printf("[Core 1] Starting...\r\n");
    
    // Initialize SageLang context for Core 1
    core1_sage_ctx = sage_init();
    if (!core1_sage_ctx) {
        printf("[Core 1] Error: Failed to initialize SageLang\r\n");
        core1_state = CORE1_STATE_ERROR;
        return;
    }
    
    core1_state = CORE1_STATE_RUNNING;
    
    sage_result_t result = SAGE_OK;
    
    // Execute code from buffer (either script or inline code)
    if (core1_code_buffer && core1_code_buffer_size > 0) {
        printf("[Core 1] Executing code...\r\n");
        
        // Feed watchdog before starting
        wdt_feed();
        
        result = sage_eval_string(core1_sage_ctx, 
                                   core1_code_buffer, 
                                   core1_code_buffer_size);
        
        // Feed watchdog after completion
        wdt_feed();
    }
    
    if (result != SAGE_OK) {
        printf("[Core 1] Error: %s\r\n", sage_get_error(core1_sage_ctx));
        core1_state = CORE1_STATE_ERROR;
    } else {
        printf("[Core 1] Execution complete\r\n");
        core1_state = CORE1_STATE_STOPPED;
    }
    
    // Cleanup
    sage_cleanup(core1_sage_ctx);
    core1_sage_ctx = NULL;
    
    // Free code buffer if it was allocated
    if (core1_code_buffer) {
        free(core1_code_buffer);
        core1_code_buffer = NULL;
        core1_code_buffer_size = 0;
    }
    
    printf("[Core 1] Stopped\r\n");
#endif
}

void multicore_init(void) {
#ifdef PICO_BUILD
    // Initialize FIFO (already done by SDK, but we ensure it's ready)
    multicore_fifo_drain();
    core1_state = CORE1_STATE_IDLE;
    printf("Multi-core system initialized\r\n");
#endif
}

bool multicore_launch_script(const char* script_name) {
#ifdef PICO_BUILD
    if (!script_name) {
        return false;
    }
    
    // Check if Core 1 is already running
    if (core1_state == CORE1_STATE_RUNNING) {
        printf("Error: Core 1 already running\r\n");
        return false;
    }
    
    // Load script from storage - use fixed buffer size
    char script_buffer[2048];  // 2KB max script size
    size_t script_size = script_storage_load(script_name, script_buffer, sizeof(script_buffer));
    
    if (script_size == 0) {
        printf("Error: Script not found: %s\r\n", script_name);
        return false;
    }
    
    // Allocate buffer for Core 1 (Core 1 will free it)
    core1_code_buffer = (char*)malloc(script_size + 1);
    if (!core1_code_buffer) {
        printf("Error: Failed to allocate memory for script\r\n");
        return false;
    }
    
    memcpy(core1_code_buffer, script_buffer, script_size);
    core1_code_buffer[script_size] = '\0';
    core1_code_buffer_size = script_size;
    
    // Store script name
    strncpy(core1_script_name, script_name, MULTICORE_MAX_SCRIPT_NAME - 1);
    core1_script_name[MULTICORE_MAX_SCRIPT_NAME - 1] = '\0';
    
    // Launch Core 1
    printf("Launching Core 1 with script: %s\r\n", script_name);
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);
    
    return true;
#else
    (void)script_name;
    return false;
#endif
}

bool multicore_launch_code(const char* code) {
#ifdef PICO_BUILD
    if (!code) {
        return false;
    }
    
    // Check if Core 1 is already running
    if (core1_state == CORE1_STATE_RUNNING) {
        printf("Error: Core 1 already running\r\n");
        return false;
    }
    
    size_t code_len = strlen(code);
    if (code_len == 0) {
        printf("Error: Empty code\r\n");
        return false;
    }
    
    // Allocate buffer for Core 1 (Core 1 will free it)
    core1_code_buffer = (char*)malloc(code_len + 1);
    if (!core1_code_buffer) {
        printf("Error: Failed to allocate memory for code\r\n");
        return false;
    }
    
    strcpy(core1_code_buffer, code);
    core1_code_buffer_size = code_len;
    
    // Clear script name (inline code)
    core1_script_name[0] = '\0';
    
    // Launch Core 1
    printf("Launching Core 1 with inline code\r\n");
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);
    
    return true;
#else
    (void)code;
    return false;
#endif
}

bool multicore_stop(void) {
#ifdef PICO_BUILD
    if (core1_state == CORE1_STATE_IDLE) {
        return false;
    }
    
    // Reset Core 1 (hard stop)
    // TODO: Implement graceful shutdown mechanism
    multicore_reset_core1();
    core1_state = CORE1_STATE_IDLE;
    
    // Clean up buffers
    if (core1_code_buffer) {
        free(core1_code_buffer);
        core1_code_buffer = NULL;
        core1_code_buffer_size = 0;
    }
    
    printf("Core 1 stopped\r\n");
    return true;
#else
    return false;
#endif
}

void multicore_reset_core1(void) {
#ifdef PICO_BUILD
    multicore_reset_core1();
    core1_state = CORE1_STATE_IDLE;
#endif
}

core1_state_t multicore_get_state(void) {
    return core1_state;
}

bool multicore_is_running(void) {
    return core1_state == CORE1_STATE_RUNNING;
}

void multicore_send(uint32_t data) {
#ifdef PICO_BUILD
    multicore_fifo_push_blocking(data);
#else
    (void)data;
#endif
}

bool multicore_send_nb(uint32_t data) {
#ifdef PICO_BUILD
    return multicore_fifo_push_timeout_us(data, 0);
#else
    (void)data;
    return false;
#endif
}

uint32_t multicore_receive(void) {
#ifdef PICO_BUILD
    return multicore_fifo_pop_blocking();
#else
    return 0;
#endif
}

bool multicore_receive_nb(uint32_t* data) {
#ifdef PICO_BUILD
    if (!data) {
        return false;
    }
    
    if (multicore_fifo_rvalid()) {
        *data = multicore_fifo_pop_blocking();
        return true;
    }
    return false;
#else
    (void)data;
    return false;
#endif
}

int multicore_fifo_available(void) {
#ifdef PICO_BUILD
    // Check if FIFO has data
    // RP2040 FIFO is 8 entries deep, but SDK doesn't expose count
    // We can only check if at least one entry is available
    return multicore_fifo_rvalid() ? 1 : 0;
#else
    return 0;
#endif
}

uint32_t multicore_get_core_num(void) {
#ifdef PICO_BUILD
    return get_core_num();
#else
    return 0;
#endif
}
