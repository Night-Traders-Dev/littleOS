#ifndef SAGE_EMBED_H
#define SAGE_EMBED_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file sage_embed.h
 * @brief Platform abstraction layer for embedding SageLang into littleOS
 * 
 * This header provides the interface for running SageLang code on both
 * embedded RP2040 devices and PC platforms.
 */

// Platform detection
#ifdef PICO_BUILD
#define SAGE_PLATFORM_EMBEDDED 1
#define SAGE_PLATFORM_PC 0
#else
#define SAGE_PLATFORM_EMBEDDED 0
#define SAGE_PLATFORM_PC 1
#endif

/**
 * @brief Result codes for SageLang operations
 */
typedef enum {
    SAGE_OK = 0,
    SAGE_ERROR_PARSE = 1,
    SAGE_ERROR_RUNTIME = 2,
    SAGE_ERROR_MEMORY = 3,
    SAGE_ERROR_IO = 4,
    SAGE_ERROR_NOT_SUPPORTED = 5
} sage_result_t;

/**
 * @brief SageLang execution context
 */
typedef struct sage_context sage_context_t;

/**
 * @brief Initialize SageLang runtime
 * @return Opaque context pointer or NULL on failure
 */
sage_context_t* sage_init(void);

/**
 * @brief Cleanup SageLang runtime and free resources
 * @param ctx Context to cleanup
 */
void sage_cleanup(sage_context_t* ctx);

/**
 * @brief Execute a SageLang script from a string
 * @param ctx Execution context
 * @param source Source code string
 * @param source_len Length of source code
 * @return Result code
 */
sage_result_t sage_eval_string(sage_context_t* ctx, const char* source, size_t source_len);

/**
 * @brief Execute a SageLang script from a file (PC only)
 * @param ctx Execution context
 * @param filename Path to .sage file
 * @return Result code
 */
sage_result_t sage_eval_file(sage_context_t* ctx, const char* filename);

/**
 * @brief Run interactive REPL (Read-Eval-Print Loop)
 * @param ctx Execution context
 * @return Result code
 */
sage_result_t sage_repl(sage_context_t* ctx);

/**
 * @brief Get last error message
 * @param ctx Execution context
 * @return Error message string or NULL
 */
const char* sage_get_error(sage_context_t* ctx);

/**
 * @brief Set memory limit for garbage collector (embedded only)
 * @param ctx Execution context
 * @param max_bytes Maximum heap size in bytes
 */
void sage_set_memory_limit(sage_context_t* ctx, size_t max_bytes);

/**
 * @brief Get memory statistics
 * @param ctx Execution context
 * @param allocated_bytes Pointer to store allocated bytes
 * @param num_objects Pointer to store number of objects
 */
void sage_get_memory_stats(sage_context_t* ctx, size_t* allocated_bytes, size_t* num_objects);

// Forward declaration for Env type from SageLang
typedef struct Env Env;

/**
 * @brief Register GPIO native functions with SageLang environment
 * @param env SageLang environment to register functions into
 */
void sage_register_gpio_functions(Env* env);

/**
 * @brief Register system information native functions with SageLang environment
 * @param env SageLang environment to register functions into
 */
void sage_register_system_functions(Env* env);

/**
 * @brief Register time/delay native functions with SageLang environment
 * @param env SageLang environment to register functions into
 */
void sage_register_time_functions(Env* env);

/**
 * @brief Register configuration storage native functions with SageLang environment
 * @param env SageLang environment to register functions into
 */
void sage_register_config_functions(Env* env);

/**
 * @brief Register watchdog timer native functions with SageLang environment
 * @param env SageLang environment to register functions into
 */
void sage_register_watchdog_functions(Env* env);

#endif // SAGE_EMBED_H
