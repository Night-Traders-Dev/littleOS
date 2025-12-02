// src/sage_time.c
// SageLang Native Function Bindings for Time/Delay Functions
#include "sage_embed.h"
#include <stdio.h>
#include <stdlib.h>

// Include SageLang headers
#include "value.h"
#include "env.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/**
 * @brief SageLang native function: sleep(milliseconds)
 * Sleep/delay for specified milliseconds
 * 
 * Usage in SageLang:
 *   sleep(1000);  // Sleep for 1 second
 *   sleep(500);   // Sleep for 500ms
 */
static Value sage_sleep(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "sleep() requires 1 argument: milliseconds\r\n");
        return val_nil();
    }
    
    if (args[0].type != VAL_NUMBER) {
        fprintf(stderr, "sleep() argument must be a number\r\n");
        return val_nil();
    }
    
    int ms = (int)args[0].as.number;
    
    if (ms < 0) {
        fprintf(stderr, "sleep() milliseconds must be positive\r\n");
        return val_nil();
    }
    
#ifdef PICO_BUILD
    sleep_ms(ms);
#else
    // PC implementation would use platform-specific sleep
    // For now, just busy-wait (not ideal but works)
    uint64_t start = clock();
    while ((clock() - start) * 1000 / CLOCKS_PER_SEC < (uint64_t)ms) {
        // Busy wait
    }
#endif
    
    return val_nil();
}

/**
 * @brief Register time/delay native functions with SageLang environment
 */
void sage_register_time_functions(Env* env) {
    if (!env) return;
    
    // Register sleep function
    env_define(env, "sleep", 5, val_native(sage_sleep));
    
    printf("Time: Registered sleep() function\r\n");
}
