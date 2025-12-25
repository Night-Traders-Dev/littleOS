// include/memory.h
#ifndef LITTLEOS_MEMORY_H
#define LITTLEOS_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * littleOS Memory Management System
 * 
 * Provides:
 * - Heap allocation/deallocation with tracking
 * - Memory pool pre-allocation for real-time guarantees
 * - Heap statistics and fragmentation detection
 * - Per-task memory accounting
 */

/* ============================================================================
 * Memory Pool Configuration
 * ============================================================================
 */

/* Total heap size for RP2040 */
#define LITTLEOS_HEAP_SIZE          (32 * 1024)  /* 32 KB */

/* Memory pool buckets for different allocation sizes */
#define LITTLEOS_POOL_SMALL_BUCKET  32           /* For allocations <= 32 bytes */
#define LITTLEOS_POOL_MEDIUM_BUCKET 256          /* For allocations <= 256 bytes */
#define LITTLEOS_POOL_LARGE_BUCKET  2048         /* For allocations <= 2KB */

/* Number of pre-allocated blocks in each pool */
#define LITTLEOS_POOL_SMALL_COUNT   16
#define LITTLEOS_POOL_MEDIUM_COUNT  8
#define LITTLEOS_POOL_LARGE_COUNT   4

/* ============================================================================
 * Memory Allocation Tracking
 * ============================================================================
 */

typedef struct {
    uint32_t total_allocated;      /* Total bytes currently allocated */
    uint32_t total_freed;          /* Total bytes freed */
    uint32_t peak_usage;           /* Peak memory usage */
    uint32_t current_usage;        /* Current memory usage */
    uint32_t num_allocations;      /* Number of active allocations */
    uint32_t num_frees;            /* Number of free operations */
    uint32_t fragmentation_ratio;  /* Fragmentation estimate (0-100%) */
} memory_stats_t;

typedef struct {
    uint32_t address;              /* Allocation address */
    uint32_t size;                 /* Allocation size */
    uint32_t timestamp;            /* Allocation timestamp */
    uint16_t task_id;              /* Task that allocated */
    uint8_t flags;                 /* Allocation flags */
} memory_block_t;

/* ============================================================================
 * Core Memory Functions
 * ============================================================================
 */

/**
 * Initialize memory management system
 * Must be called before any allocations
 */
void memory_init(void);

/**
 * Allocate memory
 * 
 * @param size  Number of bytes to allocate
 * @return      Pointer to allocated memory, or NULL if failed
 */
void *memory_alloc(size_t size);

/**
 * Allocate memory and zero-initialize
 * 
 * @param num   Number of elements
 * @param size  Size of each element
 * @return      Pointer to allocated memory, or NULL if failed
 */
void *memory_calloc(size_t num, size_t size);

/**
 * Reallocate memory
 * 
 * @param ptr   Pointer to existing block (or NULL)
 * @param size  New size in bytes
 * @return      Pointer to reallocated memory, or NULL if failed
 */
void *memory_realloc(void *ptr, size_t size);

/**
 * Free allocated memory
 * 
 * @param ptr   Pointer to memory to free
 */
void memory_free(void *ptr);

/**
 * Get current memory statistics
 * 
 * @param stats Output structure for statistics
 * @return      true if successful
 */
bool memory_get_stats(memory_stats_t *stats);

/**
 * Print memory statistics to console
 */
void memory_print_stats(void);

/**
 * Check for memory leaks and report them
 * 
 * @return Number of suspected leaks
 */
uint32_t memory_check_leaks(void);

/**
 * Get amount of free memory
 * 
 * @return Bytes remaining
 */
uint32_t memory_available(void);

/**
 * Get current memory usage
 * 
 * @return Bytes currently allocated
 */
uint32_t memory_usage(void);

/**
 * Check if a pointer is valid (within allocated regions)
 * 
 * @param ptr   Pointer to check
 * @return      true if valid
 */
bool memory_is_valid(const void *ptr);

/**
 * Defragment memory (compact heap)
 * Should be called periodically from supervisor
 * 
 * @return Bytes reclaimed
 */
uint32_t memory_defragment(void);

/**
 * Set memory warning threshold
 * Supervisor alerts when memory usage exceeds this
 * 
 * @param percent Percentage (0-100)
 */
void memory_set_warning_threshold(uint8_t percent);

#endif /* LITTLEOS_MEMORY_H */
