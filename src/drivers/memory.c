// src/memory.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "memory.h"

/*
 * littleOS Memory Management Implementation
 * 
 * Strategy:
 * - Linked-list based allocator for flexibility
 * - Block headers track allocation metadata
 * - Statistics for debugging and monitoring
 * - Supervisor integration for memory pressure alerts
 */

/* ============================================================================
 * Memory Block Header
 * ============================================================================
 */

typedef struct memory_node {
    struct memory_node *prev;      /* Previous block */
    struct memory_node *next;      /* Next block */
    uint32_t size;                 /* Block size (including header) */
    uint8_t allocated;             /* 1 if allocated, 0 if free */
    uint32_t timestamp;            /* Allocation timestamp */
    uint16_t task_id;              /* Allocating task */
    uint8_t guard_bytes[8];        /* Guard bytes to detect corruption */
} memory_node_t;

#define MEMORY_GUARD_VALUE          0xDEADBEEF
#define MEMORY_HEADER_SIZE          sizeof(memory_node_t)

/* ============================================================================
 * Global Memory State
 * ============================================================================
 */

static uint8_t memory_heap[LITTLEOS_HEAP_SIZE];
static memory_node_t *memory_head = NULL;
static memory_node_t *memory_tail = NULL;
static memory_stats_t memory_stats = {0};
static uint8_t memory_warning_threshold = 80;  /* 80% default */

/* ============================================================================
 * Helper Functions
 * ============================================================================
 */

static inline uint32_t get_ticks(void) {
    /* Would use timer_get_ticks() in actual implementation */
    return 0;
}

static inline uint32_t align_size(uint32_t size) {
    /* Align to 4-byte boundary */
    return (size + 3) & ~3;
}

static void write_guard(memory_node_t *node) {
    uint32_t *guard = (uint32_t *)node->guard_bytes;
    guard[0] = MEMORY_GUARD_VALUE;
    guard[1] = MEMORY_GUARD_VALUE;
}

static bool check_guard(memory_node_t *node) {
    uint32_t *guard = (uint32_t *)node->guard_bytes;
    return guard[0] == MEMORY_GUARD_VALUE && guard[1] == MEMORY_GUARD_VALUE;
}

static memory_node_t *find_free_block(uint32_t size) {
    memory_node_t *node = memory_head;
    
    while (node) {
        if (!node->allocated && node->size >= size) {
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}

static void split_block(memory_node_t *node, uint32_t size) {
    if (node->size <= size + MEMORY_HEADER_SIZE) {
        return;  /* Block too small to split */
    }
    
    /* Create new block for remainder */
    memory_node_t *new_node = (memory_node_t *)((uint8_t *)node + MEMORY_HEADER_SIZE + size);
    new_node->size = node->size - size - MEMORY_HEADER_SIZE;
    new_node->allocated = 0;
    new_node->prev = node;
    new_node->next = node->next;
    
    if (node->next) {
        node->next->prev = new_node;
    } else {
        memory_tail = new_node;
    }
    
    node->next = new_node;
    node->size = size + MEMORY_HEADER_SIZE;
    
    write_guard(new_node);
}

static void coalesce_blocks(void) {
    memory_node_t *node = memory_head;
    
    while (node && node->next) {
        /* Coalesce adjacent free blocks */
        if (!node->allocated && !node->next->allocated) {
            node->size += MEMORY_HEADER_SIZE + node->next->size;
            node->next = node->next->next;
            
            if (node->next) {
                node->next->prev = node;
            } else {
                memory_tail = node;
            }
            
            write_guard(node);
            continue;
        }
        
        node = node->next;
    }
}

static uint32_t calculate_fragmentation(void) {
    uint32_t free_blocks = 0;
    uint32_t max_free = 0;
    uint32_t total_free = 0;
    
    memory_node_t *node = memory_head;
    
    while (node) {
        if (!node->allocated) {
            free_blocks++;
            max_free = (node->size > max_free) ? node->size : max_free;
            total_free += node->size;
        }
        node = node->next;
    }
    
    if (total_free == 0) {
        return 0;
    }
    
    if (free_blocks <= 1) {
        return 0;
    }
    
    /* Fragmentation = 100 - (largest_free / total_free * 100) */
    return 100 - (max_free * 100) / total_free;
}

/* ============================================================================
 * Core Memory Functions
 * ============================================================================
 */

void memory_init(void) {
    /* Initialize the heap as one large free block */
    memory_head = (memory_node_t *)memory_heap;
    memory_head->prev = NULL;
    memory_head->next = NULL;
    memory_head->size = LITTLEOS_HEAP_SIZE - MEMORY_HEADER_SIZE;
    memory_head->allocated = 0;
    memory_head->timestamp = 0;
    memory_head->task_id = 0;
    
    write_guard(memory_head);
    
    memory_tail = memory_head;
    memset(&memory_stats, 0, sizeof(memory_stats_t));
}

void *memory_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    uint32_t aligned_size = align_size(size);
    memory_node_t *block = find_free_block(aligned_size);
    
    if (!block) {
        /* Try coalescing to free up space */
        coalesce_blocks();
        block = find_free_block(aligned_size);
        
        if (!block) {
            return NULL;  /* Out of memory */
        }
    }
    
    /* Split block if it's larger than needed */
    split_block(block, aligned_size);
    
    /* Mark as allocated */
    block->allocated = 1;
    block->timestamp = get_ticks();
    block->task_id = 0;  /* Would be actual task ID */
    write_guard(block);
    
    /* Update statistics */
    memory_stats.total_allocated += aligned_size;
    memory_stats.current_usage += aligned_size;
    memory_stats.num_allocations++;
    
    if (memory_stats.current_usage > memory_stats.peak_usage) {
        memory_stats.peak_usage = memory_stats.current_usage;
    }
    
    /* Return pointer after header */
    return (void *)((uint8_t *)block + MEMORY_HEADER_SIZE);
}

void *memory_calloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = memory_alloc(total);
    
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

void *memory_realloc(void *ptr, size_t size) {
    if (!ptr) {
        return memory_alloc(size);
    }
    
    if (size == 0) {
        memory_free(ptr);
        return NULL;
    }
    
    /* Allocate new block */
    void *new_ptr = memory_alloc(size);
    if (!new_ptr) {
        return NULL;
    }
    
    /* Copy old data (assuming we can determine old size) */
    /* For simplicity, copy minimum of old/new sizes */
    memory_node_t *node = (memory_node_t *)((uint8_t *)ptr - MEMORY_HEADER_SIZE);
    uint32_t old_size = node->size - MEMORY_HEADER_SIZE;
    uint32_t copy_size = (size < old_size) ? size : old_size;
    
    memcpy(new_ptr, ptr, copy_size);
    
    /* Free old block */
    memory_free(ptr);
    
    return new_ptr;
}

void memory_free(void *ptr) {
    if (!ptr) {
        return;
    }
    
    memory_node_t *node = (memory_node_t *)((uint8_t *)ptr - MEMORY_HEADER_SIZE);
    
    /* Sanity checks */
    if (!check_guard(node)) {
        printf("ERROR: Memory corruption detected at %p\r\n", ptr);
        return;
    }
    
    if (!node->allocated) {
        printf("ERROR: Double-free detected at %p\r\n", ptr);
        return;
    }
    
    /* Mark as free */
    node->allocated = 0;
    memory_stats.current_usage -= node->size;
    memory_stats.total_freed += node->size;
    memory_stats.num_frees++;
    
    /* Try coalescing adjacent blocks */
    coalesce_blocks();
}

bool memory_get_stats(memory_stats_t *stats) {
    if (!stats) {
        return false;
    }
    
    /* Update fragmentation before copying */
    memory_stats.fragmentation_ratio = calculate_fragmentation();
    
    memcpy(stats, &memory_stats, sizeof(memory_stats_t));
    return true;
}

void memory_print_stats(void) {
    memory_stats_t stats;
    if (!memory_get_stats(&stats)) {
        return;
    }
    
    printf("\r\n=== Memory Statistics ===\r\n");
    printf("Total heap size: %d bytes\r\n", LITTLEOS_HEAP_SIZE);
    printf("Current usage:   %lu bytes (%.1f%%)\r\n", 
           stats.current_usage, 
           (float)stats.current_usage / LITTLEOS_HEAP_SIZE * 100);
    printf("Peak usage:      %lu bytes\r\n", stats.peak_usage);
    printf("Allocations:     %lu (frees: %lu)\r\n", 
           stats.num_allocations, stats.num_frees);
    printf("Fragmentation:   %lu%%\r\n", stats.fragmentation_ratio);
    printf("========================\r\n");
}

uint32_t memory_check_leaks(void) {
    uint32_t suspected_leaks = 0;
    memory_node_t *node = memory_head;
    
    uint32_t now = get_ticks();
    uint32_t leak_threshold_ms = 30000;  /* 30 seconds */
    
    while (node) {
        if (node->allocated && node->timestamp > 0) {
            if ((now - node->timestamp) > leak_threshold_ms) {
                printf("Suspected leak: %lu bytes at %p (age: %lu ms)\r\n",
                       node->size, (void *)((uint8_t *)node + MEMORY_HEADER_SIZE),
                       now - node->timestamp);
                suspected_leaks++;
            }
        }
        node = node->next;
    }
    
    return suspected_leaks;
}

uint32_t memory_available(void) {
    return LITTLEOS_HEAP_SIZE - memory_stats.current_usage;
}

uint32_t memory_usage(void) {
    return memory_stats.current_usage;
}

bool memory_is_valid(const void *ptr) {
    if (!ptr) {
        return false;
    }
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t heap_start = (uintptr_t)memory_heap;
    uintptr_t heap_end = heap_start + LITTLEOS_HEAP_SIZE;
    
    if (addr < heap_start || addr >= heap_end) {
        return false;
    }
    
    memory_node_t *node = (memory_node_t *)((uint8_t *)ptr - MEMORY_HEADER_SIZE);
    return check_guard(node) && node->allocated;
}

uint32_t memory_defragment(void) {
    uint32_t before = calculate_fragmentation();
    coalesce_blocks();
    uint32_t after = calculate_fragmentation();
    
    return (before > after) ? (before - after) : 0;
}

void memory_set_warning_threshold(uint8_t percent) {
    if (percent <= 100) {
        memory_warning_threshold = percent;
    }
}
