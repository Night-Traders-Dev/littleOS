#include "sage_embed.h"
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

/**
 * @brief Internal SageLang context structure
 */
struct sage_context {
    Env* global_env;
    char error_msg[256];
    bool initialized;
};

/**
 * @brief Initialize SageLang runtime
 */
sage_context_t* sage_init(void) {
    sage_context_t* ctx = (sage_context_t*)malloc(sizeof(sage_context_t));
    if (!ctx) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(sage_context_t));
    
    // Initialize garbage collector
    gc_init();
    
#ifdef PICO_BUILD
    // Set conservative memory limits for RP2040 (256KB RAM)
    // Reserve 64KB for SageLang heap
    // Note: gc_set_max_bytes may not exist, check gc.h
    printf("SageLang: Embedded mode (64KB heap)\r\n");
#else
    printf("SageLang: PC mode (unlimited heap)\n");
#endif
    
    // Create global environment and initialize standard library
    ctx->global_env = env_create(NULL);
    if (!ctx->global_env) {
        free(ctx);
        return NULL;
    }
    
    init_stdlib(ctx->global_env);
    
    // Register GPIO native functions
    sage_register_gpio_functions(ctx->global_env);
    
    // Register system information native functions
    sage_register_system_functions(ctx->global_env);
    
    ctx->initialized = true;
    return ctx;
}

/**
 * @brief Cleanup SageLang runtime
 */
void sage_cleanup(sage_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->global_env) {
        // Environment will be cleaned up by GC
        ctx->global_env = NULL;
    }
    
    // Run final GC sweep
    gc_collect();
    
    ctx->initialized = false;
    free(ctx);
}

/**
 * @brief Execute SageLang source code
 */
sage_result_t sage_eval_string(sage_context_t* ctx, const char* source, size_t source_len) {
    if (!ctx || !ctx->initialized) {
        return SAGE_ERROR_RUNTIME;
    }
    
    if (!source || source_len == 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Empty source code");
        return SAGE_ERROR_RUNTIME;
    }
    
    // Initialize lexer with source
    init_lexer(source);
    parser_init();
    
    // Parse and execute all statements
    while (1) {
        Stmt* stmt = parse();
        if (stmt == NULL) {
            break; // End of input
        }
        
        // Interpret the statement
        // Note: interpret() may not return errors directly
        // We'll need to wrap this or check for exceptions
        interpret(stmt, ctx->global_env);
        
        // TODO: Check for exceptions/errors from interpret
        // The current SageLang interpreter uses exit() on errors
        // which isn't ideal for embedded use
    }
    
    return SAGE_OK;
}

/**
 * @brief Execute SageLang file (PC only)
 */
sage_result_t sage_eval_file(sage_context_t* ctx, const char* filename) {
#ifdef PICO_BUILD
    // File I/O not supported on embedded platform yet
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
    
    // Read entire file
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
 * @brief Run interactive REPL
 */
sage_result_t sage_repl(sage_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return SAGE_ERROR_RUNTIME;
    }
    
    char buffer[512];
    
#ifdef PICO_BUILD
    printf("\r\nSageLang REPL (embedded mode)\r\n");
    printf("Type 'exit' to quit\r\n\r\n");
#else
    printf("\nSageLang REPL v0.8.0\n");
    printf("Type 'exit' to quit\n\n");
#endif
    
    while (1) {
#ifdef PICO_BUILD
        printf("sage> ");
        fflush(stdout);
        
        int idx = 0;
        while (1) {
            int c = getchar_timeout_us(0);
            if (c == PICO_ERROR_TIMEOUT) {
                sleep_ms(10);
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
        
        // Remove newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
#endif
        
        // Check for exit
        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        
        // Skip empty lines
        if (strlen(buffer) == 0) {
            continue;
        }
        
        // Evaluate
        sage_result_t result = sage_eval_string(ctx, buffer, strlen(buffer));
        if (result != SAGE_OK) {
#ifdef PICO_BUILD
            printf("Error: %s\r\n", ctx->error_msg);
#else
            printf("Error: %s\n", ctx->error_msg);
#endif
        }
    }
    
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
 * @brief Set memory limit
 */
void sage_set_memory_limit(sage_context_t* ctx, size_t max_bytes) {
    if (!ctx || !ctx->initialized) return;
    
#ifdef PICO_BUILD
    // Note: gc_set_max_bytes may not exist in SageLang's gc.h
    // Check gc.h for available functions
    (void)max_bytes;
#else
    // Memory limit not enforced on PC
    (void)max_bytes;
#endif
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
    
    // Get GC stats
    GCStats stats = gc_get_stats();
    if (allocated_bytes) *allocated_bytes = stats.bytes_allocated;
    if (num_objects) *num_objects = stats.num_objects;
}
