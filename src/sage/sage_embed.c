#include "sage_embed.h"
#include "watchdog.h"
#include "supervisor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include SageLang headers
#include "lexer.h"
#include "ast.h"
#include "interpreter.h"
#include "env.h"
#include "gc.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

// Forward declarations for SageLang functions
extern Stmt* parse();
extern void parser_init();
extern void init_stdlib(Env* env);

// Heartbeat tracking
static uint32_t last_heartbeat_ms = 0;
static uint32_t heartbeat_count = 0;
static bool heartbeat_enabled = true;

#define HEARTBEAT_INTERVAL_MS 250  // Send heartbeat every 250ms (more frequent)

/**
 * @brief Internal SageLang context structure
 */
struct sage_context {
    Env* global_env;
    char error_msg[256];
    bool initialized;
    uint32_t execution_start_time;
    uint32_t max_execution_time_ms;  // Maximum execution time before warning
};

/**
 * @brief Send heartbeat if enough time has passed
 * This is the core function that maintains system responsiveness
 */
static inline void sage_try_heartbeat(void) {
    if (!heartbeat_enabled) return;
    
#ifdef PICO_BUILD
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
        supervisor_heartbeat();
        wdt_feed();
        last_heartbeat_ms = now;
        heartbeat_count++;
    }
#endif
}

/**
 * @brief Force an immediate heartbeat
 */
static inline void sage_force_heartbeat(void) {
#ifdef PICO_BUILD
    supervisor_heartbeat();
    wdt_feed();
    last_heartbeat_ms = to_ms_since_boot(get_absolute_time());
    heartbeat_count++;
#endif
}

/**
 * @brief Enable/disable heartbeat mechanism
 */
void sage_set_heartbeat_enabled(sage_context_t* ctx, bool enabled) {
    (void)ctx;
    heartbeat_enabled = enabled;
}

/**
 * @brief Get heartbeat statistics
 */
void sage_get_heartbeat_stats(uint32_t* count, uint32_t* last_ms) {
    if (count) *count = heartbeat_count;
    if (last_ms) *last_ms = last_heartbeat_ms;
}

/**
 * @brief Initialize SageLang runtime
 */
sage_context_t* sage_init(void) {
    sage_context_t* ctx = (sage_context_t*)malloc(sizeof(sage_context_t));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(sage_context_t));
    
    // Initialize heartbeat tracking
#ifdef PICO_BUILD
    last_heartbeat_ms = to_ms_since_boot(get_absolute_time());
#endif
    heartbeat_count = 0;
    heartbeat_enabled = true;
    
    // Initialize garbage collector
    gc_init();
    
#ifdef PICO_BUILD
    printf("SageLang: Embedded mode (64KB heap)\r\n");
    sage_force_heartbeat();  // Send heartbeat during init
#else
    printf("SageLang: PC mode (unlimited heap)\n");
#endif
    
    // Create global environment and initialize standard library
    ctx->global_env = env_create(NULL);
    if (!ctx->global_env) {
        free(ctx);
        return NULL;
    }
    
    sage_force_heartbeat();  // Heartbeat after env creation
    
    init_stdlib(ctx->global_env);
    
    sage_force_heartbeat();  // Heartbeat after stdlib init
    
    // Register native functions
    sage_register_gpio_functions(ctx->global_env);
    sage_register_system_functions(ctx->global_env);
    sage_register_time_functions(ctx->global_env);
    sage_register_config_functions(ctx->global_env);
    sage_register_watchdog_functions(ctx->global_env);
    
    sage_force_heartbeat();  // Heartbeat after registration
    
    // Set default execution timeout to 5 seconds
    ctx->max_execution_time_ms = 5000;
    
    ctx->initialized = true;
    return ctx;
}

/**
 * @brief Cleanup SageLang runtime
 */
void sage_cleanup(sage_context_t* ctx) {
    if (!ctx) return;
    
    sage_force_heartbeat();  // Heartbeat before cleanup
    
    if (ctx->global_env) {
        ctx->global_env = NULL;
    }
    
    // Run final GC sweep
    gc_collect();
    
    sage_force_heartbeat();  // Heartbeat after cleanup
    
    ctx->initialized = false;
    free(ctx);
}

/**
 * @brief Check if execution time limit exceeded
 */
static bool sage_check_timeout(sage_context_t* ctx) {
#ifdef PICO_BUILD
    if (ctx->max_execution_time_ms == 0) return false;  // No limit
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed = now - ctx->execution_start_time;
    
    if (elapsed >= ctx->max_execution_time_ms) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Execution timeout (%u ms exceeded)", elapsed);
        return true;
    }
#else
    (void)ctx;
#endif
    return false;
}

/**
 * @brief Execute SageLang source code with aggressive heartbeat maintenance
 */
sage_result_t sage_eval_string(sage_context_t* ctx, const char* source, size_t source_len) {
    if (!ctx || !ctx->initialized) {
        return SAGE_ERROR_RUNTIME;
    }
    
    if (!source || source_len == 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Empty source code");
        return SAGE_ERROR_RUNTIME;
    }
    
    // Record start time
#ifdef PICO_BUILD
    ctx->execution_start_time = to_ms_since_boot(get_absolute_time());
#endif
    
    // Force heartbeat before starting execution
    sage_force_heartbeat();
    
    // Initialize lexer with source
    init_lexer(source);
    parser_init();
    
    sage_try_heartbeat();  // Heartbeat after lexer init
    
    // Parse and execute all statements
    int statement_count = 0;
    while (1) {
        // Check timeout before each statement
        if (sage_check_timeout(ctx)) {
            return SAGE_ERROR_TIMEOUT;
        }
        
        // Send heartbeat before parsing
        sage_try_heartbeat();
        
        Stmt* stmt = parse();
        if (stmt == NULL) {
            break; // End of input
        }
        
        statement_count++;
        
        // Send heartbeat after parsing (AST construction can be expensive)
        sage_try_heartbeat();
        
#ifdef PICO_BUILD
        // For embedded: force heartbeat every 10th statement
        if (statement_count % 10 == 0) {
            sage_force_heartbeat();
        }
#endif
        
        // Interpret the statement
        interpret(stmt, ctx->global_env);
        
        // Force heartbeat after each statement interpretation
        sage_force_heartbeat();
        
        // Check timeout after each statement
        if (sage_check_timeout(ctx)) {
            return SAGE_ERROR_TIMEOUT;
        }
    }
    
    // Final heartbeat after all execution
    sage_force_heartbeat();
    
    return SAGE_OK;
}

/**
 * @brief Execute SageLang file (PC only)
 */
sage_result_t sage_eval_file(sage_context_t* ctx, const char* filename) {
#ifdef PICO_BUILD
    snprintf(ctx->error_msg, sizeof(ctx->error_msg),
             "File I/O not supported on embedded platform");
    return SAGE_ERROR_NOT_SUPPORTED;
#else
    if (!ctx || !ctx->initialized) {
        return SAGE_ERROR_RUNTIME;
    }
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Cannot open file: %s", filename);
        return SAGE_ERROR_IO;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* source = (char*)malloc(length + 1);
    if (!source) {
        fclose(file);
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Memory allocation failed");
        return SAGE_ERROR_MEMORY;
    }
    
    size_t read = fread(source, 1, length, file);
    source[read] = '\0';
    fclose(file);
    
    sage_result_t result = sage_eval_string(ctx, source, read);
    free(source);
    
    return result;
#endif
}

/**
 * @brief Run interactive REPL with enhanced heartbeat maintenance
 */
sage_result_t sage_repl(sage_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return SAGE_ERROR_RUNTIME;
    }
    
    char buffer[512];
    
#ifdef PICO_BUILD
    printf("\r\nSageLang REPL (embedded mode)\r\n");
    printf("Enhanced watchdog maintenance active\r\n");
    printf("Type 'exit' to quit\r\n\r\n");
#else
    printf("\nSageLang REPL v0.8.0\n");
    printf("Type 'exit' to quit\n\n");
#endif
    
    sage_force_heartbeat();  // Heartbeat at REPL start
    
    while (1) {
        // Aggressive heartbeat maintenance in input loop
#ifdef PICO_BUILD
        sage_try_heartbeat();  // Will send heartbeat every 250ms
        
        printf("sage> ");
        fflush(stdout);
        
        int idx = 0;
        uint32_t input_start = to_ms_since_boot(get_absolute_time());
        
        while (1) {
            // Critical: heartbeat while waiting for input
            sage_try_heartbeat();
            
            int c = getchar_timeout_us(0);
            if (c == PICO_ERROR_TIMEOUT) {
                sleep_ms(10);
                
                // Extra safety: force heartbeat every 2 seconds of input wait
                uint32_t now = to_ms_since_boot(get_absolute_time());
                if (now - input_start >= 2000) {
                    sage_force_heartbeat();
                    input_start = now;
                }
                continue;
            }
            
            if (c == '\r' || c == '\n') {
                putchar('\r');
                putchar('\n');
                buffer[idx] = '\0';
                break;
            } else if (c == '\b' || c == 0x7F) {
                if (idx > 0) {
                    idx--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (c >= 32 && c < 127 && idx < sizeof(buffer) - 1) {
                buffer[idx++] = (char)c;
                putchar(c);
                fflush(stdout);
            }
        }
#else
        printf("sage> ");
        fflush(stdout);
        
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            break;
        }
        
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
#endif
        
        // Check for exit
        if (strcmp(buffer, "exit") == 0) {
            sage_force_heartbeat();
            break;
        }
        
        // Skip empty lines
        if (strlen(buffer) == 0) {
            continue;
        }
        
        // Force heartbeat before evaluation
        sage_force_heartbeat();
        
        // Evaluate with enhanced heartbeat maintenance
        sage_result_t result = sage_eval_string(ctx, buffer, strlen(buffer));
        
        if (result == SAGE_ERROR_TIMEOUT) {
#ifdef PICO_BUILD
            printf("Warning: Command execution timeout\r\n");
            printf("Consider breaking long operations into smaller chunks\r\n");
#else
            printf("Warning: Command execution timeout\n");
#endif
        } else if (result != SAGE_OK) {
#ifdef PICO_BUILD
            printf("Error: %s\r\n", ctx->error_msg);
#else
            printf("Error: %s\n", ctx->error_msg);
#endif
        }
        
        // Force heartbeat after evaluation
        sage_force_heartbeat();
    }
    
    sage_force_heartbeat();  // Final heartbeat before exit
    return SAGE_OK;
}

/**
 * @brief Get last error message
 */
const char* sage_get_error(sage_context_t* ctx) {
    if (!ctx) return NULL;
    return ctx->error_msg;
}

/**
 * @brief Set execution timeout
 */
void sage_set_execution_timeout(sage_context_t* ctx, uint32_t timeout_ms) {
    if (!ctx || !ctx->initialized) return;
    ctx->max_execution_time_ms = timeout_ms;
}

/**
 * @brief Get execution timeout
 */
uint32_t sage_get_execution_timeout(sage_context_t* ctx) {
    if (!ctx || !ctx->initialized) return 0;
    return ctx->max_execution_time_ms;
}

/**
 * @brief Set memory limit
 */
void sage_set_memory_limit(sage_context_t* ctx, size_t max_bytes) {
    if (!ctx || !ctx->initialized) return;
    (void)max_bytes;  // Not implemented yet
}

/**
 * @brief Get memory statistics
 */
void sage_get_memory_stats(sage_context_t* ctx, size_t* allocated_bytes, size_t* num_objects) {
    if (!ctx || !ctx->initialized) {
        if (allocated_bytes) *allocated_bytes = 0;
        if (num_objects) *num_objects = 0;
        return;
    }
    
    GCStats stats = gc_get_stats();
    if (allocated_bytes) *allocated_bytes = stats.bytes_allocated;
    if (num_objects) *num_objects = stats.num_objects;
}
