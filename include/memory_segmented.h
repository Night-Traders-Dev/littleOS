/**
 * littleOS Memory Management Header
 * 
 * Provides segmented heap allocation for kernel and interpreter
 */

#ifndef _KERNEL_MEMORY_SEGMENTED_H
#define _KERNEL_MEMORY_SEGMENTED_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Memory Initialization
 * ============================================================================ */

/**
 * Initialize memory management system
 * MUST be called before any malloc/calloc operations!
 */
void memory_init(void);

/* ============================================================================
 * Kernel Heap Allocation
 * 
 * For long-lived kernel data structures:
 * - Task control blocks
 * - Driver state
 * - System tables
 * - Persistent objects
 * ============================================================================ */

/**
 * Allocate memory from kernel heap
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void *kernel_malloc(size_t size);

/**
 * Allocate and zero memory from kernel heap
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed memory, or NULL if out of memory
 */
void *kernel_calloc(size_t count, size_t size);

/**
 * Allocate with debug information
 * @param size Number of bytes
 * @param file Source file name
 * @param line Source line number
 * @return Pointer to allocated memory
 */
void *kernel_malloc_debug(size_t size, const char *file, int line);

/* Macro for convenient debug allocation */
#define KERNEL_MALLOC_DEBUG(size) kernel_malloc_debug(size, __FILE__, __LINE__)

/* ============================================================================
 * Interpreter Heap Allocation
 * 
 * For SageLang interpreter:
 * - AST nodes
 * - Runtime values
 * - Module symbols
 * - Temporary objects
 * 
 * Note: All interpreter allocations can be freed at once with
 * interpreter_heap_reset() to avoid fragmentation
 * ============================================================================ */

/**
 * Allocate memory from interpreter heap
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void *interpreter_malloc(size_t size);

/**
 * Allocate and zero memory from interpreter heap
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed memory, or NULL if out of memory
 */
void *interpreter_calloc(size_t count, size_t size);

/**
 * Allocate with debug information
 * @param size Number of bytes
 * @param file Source file name
 * @param line Source line number
 * @return Pointer to allocated memory
 */
void *interpreter_malloc_debug(size_t size, const char *file, int line);

/**
 * Reset interpreter heap to free all allocations at once
 * 
 * Call this between script executions to prevent fragmentation.
 * All previous interpreter_malloc() results become INVALID!
 */
void interpreter_heap_reset(void);

/**
 * Get remaining interpreter heap space
 * @return Number of free bytes in interpreter heap
 */
size_t interpreter_heap_remaining(void);

/* ============================================================================
 * Memory Statistics and Diagnostics
 * ============================================================================ */

/**
 * Memory statistics structure
 */
typedef struct {
    size_t kernel_used;             /* Kernel heap bytes in use */
    size_t kernel_free;             /* Kernel heap bytes free */
    size_t kernel_peak;             /* Kernel heap peak usage */
    size_t interpreter_used;        /* Interpreter heap bytes in use */
    size_t interpreter_free;        /* Interpreter heap bytes free */
    size_t interpreter_peak;        /* Interpreter heap peak usage */
    float kernel_usage_pct;         /* Kernel heap usage percentage */
    float interpreter_usage_pct;    /* Interpreter heap usage percentage */
    uint32_t kernel_alloc_count;    /* Number of kernel allocations */
    uint32_t interpreter_alloc_count; /* Number of interpreter allocations */
} MemoryStats;

/**
 * Get current memory statistics
 * @return Populated MemoryStats structure
 */
MemoryStats memory_get_stats(void);

/**
 * Print formatted memory statistics to UART
 * Requires uart_puts() and uart_printf() to be defined
 */
void memory_print_stats(void);

/**
 * Print memory layout diagram
 * Shows address ranges of heap and stack regions
 */
void memory_print_layout(void);

/**
 * Validate memory region layout
 * Checks that kernel heap, interpreter heap, and stack don't overlap
 * @return 1 if layout is valid, 0 if invalid
 */
int memory_validate_layout(void);

/* ============================================================================
 * Stack Monitoring
 * ============================================================================ */

/**
 * Get current stack pointer value
 * @return Current SP register value
 */
uint32_t stack_get_current_sp(void);

/**
 * Get free stack space remaining
 * Stack grows downward, so free space = SP - stack_bottom
 * @return Number of bytes still available on stack
 */
uint32_t stack_get_free_space(void);

/**
 * Get stack space that has been used
 * @return Number of bytes used on stack
 */
uint32_t stack_get_used_space(void);

/**
 * Check for heap-stack collision
 * Returns 1 if stack has grown into heap space (critical error!)
 * @return 1 if collision detected, 0 if safe
 */
int memory_check_collision(void);

/**
 * Print formatted stack status
 */
void memory_print_stack_status(void);

/* ============================================================================
 * System Health Check
 * ============================================================================ */

/**
 * Run comprehensive memory health check
 * 
 * Performs:
 * - Memory layout validation
 * - Heap-stack collision detection
 * - Statistics reporting
 * - Status summary
 * 
 * Output goes to UART
 */
void memory_health_check(void);

#endif /* _KERNEL_MEMORY_SEGMENTED_H */
