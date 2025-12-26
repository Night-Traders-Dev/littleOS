/**
 * littleOS Segmented Memory Manager - Production Version
 * 
 * Separates kernel and interpreter heap spaces to prevent allocation conflicts
 * RP2040: 256 KB SRAM divided into:
 *   - Kernel Heap: 64 KB (0x20000000-0x2000FFFF)
 *   - Interpreter Heap: 64 KB (0x20010000-0x2001FFFF)
 *   - Stack: 128 KB (0x20020000-0x2003FFFF, grows down)
 * 
 * No external linker symbols required - uses hardcoded RP2040 addresses
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Memory region constants for RP2040 (256 KB SRAM)
 * ============================================================================ */

#define SRAM_BASE               0x20000000
#define KERNEL_HEAP_BASE        0x20000000
#define KERNEL_HEAP_SIZE        (64 * 1024)    /* 64 KB */
#define KERNEL_HEAP_END         (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)

#define INTERPRETER_HEAP_BASE   (KERNEL_HEAP_END)
#define INTERPRETER_HEAP_SIZE   (64 * 1024)    /* 64 KB */
#define INTERPRETER_HEAP_END    (INTERPRETER_HEAP_BASE + INTERPRETER_HEAP_SIZE)

#define STACK_BASE              (INTERPRETER_HEAP_END)
#define STACK_SIZE              (128 * 1024)   /* 128 KB */
#define STACK_TOP               (STACK_BASE + STACK_SIZE)

/* ============================================================================
 * Memory region structure
 * ============================================================================ */

typedef struct {
    uint8_t *start;              /* Start address of region */
    uint8_t *end;                /* End address of region */
    uint8_t *current;            /* Current allocation pointer (bump allocator) */
    size_t max_size;             /* Total region size */
    size_t used_size;            /* Currently allocated bytes */
    size_t peak_size;            /* Peak allocation */
    uint32_t allocation_count;   /* Number of allocations */
    const char *name;            /* Region name for debugging */
} MemoryRegion;

/* ============================================================================
 * Global memory regions - initialized at compile time
 * ============================================================================ */

static MemoryRegion kernel_heap = {
    .start = (uint8_t *)KERNEL_HEAP_BASE,
    .end = (uint8_t *)KERNEL_HEAP_END,
    .current = (uint8_t *)KERNEL_HEAP_BASE,
    .max_size = KERNEL_HEAP_SIZE,
    .used_size = 0,
    .peak_size = 0,
    .allocation_count = 0,
    .name = "KERNEL_HEAP"
};

static MemoryRegion interpreter_heap = {
    .start = (uint8_t *)INTERPRETER_HEAP_BASE,
    .end = (uint8_t *)INTERPRETER_HEAP_END,
    .current = (uint8_t *)INTERPRETER_HEAP_BASE,
    .max_size = INTERPRETER_HEAP_SIZE,
    .used_size = 0,
    .peak_size = 0,
    .allocation_count = 0,
    .name = "INTERPRETER_HEAP"
};

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * Initialize memory management system
 * MUST be called before any malloc/calloc operations!
 */
void memory_init(void)
{
    /* Reset kernel heap */
    kernel_heap.current = kernel_heap.start;
    kernel_heap.used_size = 0;
    kernel_heap.peak_size = 0;
    kernel_heap.allocation_count = 0;
    
    /* Reset interpreter heap */
    interpreter_heap.current = interpreter_heap.start;
    interpreter_heap.used_size = 0;
    interpreter_heap.peak_size = 0;
    interpreter_heap.allocation_count = 0;
}

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
 * Uses simple bump allocator (fast, no fragmentation)
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL if out of memory
 */
void *kernel_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    
    /* Align to 8 bytes for proper alignment on ARM */
    size = (size + 7) & ~7;
    
    /* Check if allocation would overflow */
    if ((kernel_heap.current + size) > kernel_heap.end) {
        return NULL;  /* Out of memory */
    }
    
    /* Get current position and advance */
    void *ptr = (void *)kernel_heap.current;
    kernel_heap.current += size;
    
    /* Update statistics */
    kernel_heap.used_size += size;
    kernel_heap.allocation_count++;
    
    if (kernel_heap.used_size > kernel_heap.peak_size) {
        kernel_heap.peak_size = kernel_heap.used_size;
    }
    
    return ptr;
}

/**
 * Allocate and zero memory from kernel heap
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed memory, or NULL if out of memory
 */
void *kernel_calloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = kernel_malloc(total);
    
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

/**
 * Allocate from kernel heap with debug information
 * @param size Number of bytes
 * @param file Source file name
 * @param line Source line number
 * @return Pointer to allocated memory
 */
void *kernel_malloc_debug(size_t size, const char *file, int line)
{
    (void)file;
    (void)line;
    return kernel_malloc(size);
}

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
void *interpreter_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    
    /* Align to 8 bytes */
    size = (size + 7) & ~7;
    
    /* Check overflow */
    if ((interpreter_heap.current + size) > interpreter_heap.end) {
        return NULL;  /* Out of memory */
    }
    
    /* Allocate */
    void *ptr = (void *)interpreter_heap.current;
    interpreter_heap.current += size;
    
    /* Update statistics */
    interpreter_heap.used_size += size;
    interpreter_heap.allocation_count++;
    
    if (interpreter_heap.used_size > interpreter_heap.peak_size) {
        interpreter_heap.peak_size = interpreter_heap.used_size;
    }
    
    return ptr;
}

/**
 * Allocate and zero memory from interpreter heap
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed memory, or NULL if out of memory
 */
void *interpreter_calloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *ptr = interpreter_malloc(total);
    
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

/**
 * Allocate from interpreter heap with debug information
 * @param size Number of bytes
 * @param file Source file name
 * @param line Source line number
 * @return Pointer to allocated memory
 */
void *interpreter_malloc_debug(size_t size, const char *file, int line)
{
    (void)file;
    (void)line;
    return interpreter_malloc(size);
}

/**
 * Reset interpreter heap to free all allocations at once
 * 
 * Call this between script executions to prevent fragmentation.
 * All previous interpreter_malloc() results become INVALID!
 */
void interpreter_heap_reset(void)
{
    interpreter_heap.current = interpreter_heap.start;
    interpreter_heap.used_size = 0;
    interpreter_heap.allocation_count = 0;
}

/**
 * Get remaining interpreter heap space
 * @return Number of free bytes in interpreter heap
 */
size_t interpreter_heap_remaining(void)
{
    return (size_t)(interpreter_heap.end - interpreter_heap.current);
}

/* ============================================================================
 * Memory Statistics and Diagnostics
 * ============================================================================ */

/**
 * Memory statistics structure
 */
typedef struct {
    size_t kernel_used;              /* Kernel heap bytes in use */
    size_t kernel_free;              /* Kernel heap bytes free */
    size_t kernel_peak;              /* Kernel heap peak usage */
    size_t interpreter_used;         /* Interpreter heap bytes in use */
    size_t interpreter_free;         /* Interpreter heap bytes free */
    size_t interpreter_peak;         /* Interpreter heap peak usage */
    float kernel_usage_pct;          /* Kernel heap usage percentage */
    float interpreter_usage_pct;     /* Interpreter heap usage percentage */
    uint32_t kernel_alloc_count;     /* Number of kernel allocations */
    uint32_t interpreter_alloc_count; /* Number of interpreter allocations */
} MemoryStats;

/**
 * Get current memory statistics
 * @return Populated MemoryStats structure
 */
MemoryStats memory_get_stats(void)
{
    MemoryStats stats = {
        .kernel_used = kernel_heap.used_size,
        .kernel_free = kernel_heap.max_size - kernel_heap.used_size,
        .kernel_peak = kernel_heap.peak_size,
        .interpreter_used = interpreter_heap.used_size,
        .interpreter_free = interpreter_heap.max_size - interpreter_heap.used_size,
        .interpreter_peak = interpreter_heap.peak_size,
        .kernel_alloc_count = kernel_heap.allocation_count,
        .interpreter_alloc_count = interpreter_heap.allocation_count,
    };
    
    if (kernel_heap.max_size > 0) {
        stats.kernel_usage_pct = (float)stats.kernel_used / kernel_heap.max_size * 100.0f;
    }
    
    if (interpreter_heap.max_size > 0) {
        stats.interpreter_usage_pct = (float)stats.interpreter_used / interpreter_heap.max_size * 100.0f;
    }
    
    return stats;
}

/**
 * Print formatted memory statistics to UART
 * Requires printf() to be defined
 */
void memory_print_stats(void)
{
    MemoryStats stats = memory_get_stats();
    
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           MEMORY STATISTICS                          ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    
    printf("║ KERNEL HEAP:                                       ║\n");
    printf("║   Used: %6zu bytes (%5.1f%%)                    ║\n", 
                stats.kernel_used, stats.kernel_usage_pct);
    printf("║   Free: %6zu bytes                              ║\n", 
                stats.kernel_free);
    printf("║   Peak: %6zu bytes                              ║\n", 
                stats.kernel_peak);
    printf("║   Allocations: %u                                ║\n", 
                stats.kernel_alloc_count);
    
    printf("╟────────────────────────────────────────────────────╢\n");
    
    printf("║ INTERPRETER HEAP:                                  ║\n");
    printf("║   Used: %6zu bytes (%5.1f%%)                    ║\n", 
                stats.interpreter_used, stats.interpreter_usage_pct);
    printf("║   Free: %6zu bytes                              ║\n", 
                stats.interpreter_free);
    printf("║   Peak: %6zu bytes                              ║\n", 
                stats.interpreter_peak);
    printf("║   Allocations: %u                                ║\n", 
                stats.interpreter_alloc_count);
    
    printf("╚════════════════════════════════════════════════════╝\n\n");
}

/**
 * Print memory layout diagram
 * Shows address ranges of heap and stack regions
 */
void memory_print_layout(void)
{
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           MEMORY LAYOUT (256 KB SRAM)               ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    
    printf("║ Kernel Heap:      0x%08x - 0x%08x        ║\n",
                KERNEL_HEAP_BASE, KERNEL_HEAP_END);
    printf("║ Interpreter Heap: 0x%08x - 0x%08x        ║\n",
                INTERPRETER_HEAP_BASE, INTERPRETER_HEAP_END);
    printf("║ Stack (grows ↓):  0x%08x - 0x%08x        ║\n",
                STACK_BASE, STACK_TOP);
    
    printf("╚════════════════════════════════════════════════════╝\n\n");
}

/**
 * Validate memory region layout
 * Checks that kernel heap, interpreter heap, and stack don't overlap
 * @return 1 if layout is valid, 0 if invalid
 */
int memory_validate_layout(void)
{
    /* Check each region has valid bounds */
    if (kernel_heap.start >= kernel_heap.end) {
        return 0;
    }
    if (interpreter_heap.start >= interpreter_heap.end) {
        return 0;
    }
    
    /* Check regions don't overlap */
    if (kernel_heap.end > interpreter_heap.start) {
        return 0;  /* Kernel heap overlaps interpreter heap! */
    }
    if (interpreter_heap.end > (uint8_t *)STACK_BASE) {
        return 0;  /* Interpreter heap overlaps stack! */
    }
    
    /* All checks passed */
    return 1;
}

/* ============================================================================
 * Stack Monitoring
 * ============================================================================ */

/**
 * Get current stack pointer value
 * @return Current SP register value
 */
uint32_t stack_get_current_sp(void)
{
    uint32_t sp;
    asm volatile("mov %0, sp" : "=r" (sp));
    return sp;
}

/**
 * Get free stack space remaining
 * Stack grows downward, so free space = SP - stack_bottom
 * @return Number of bytes still available on stack
 */
uint32_t stack_get_free_space(void)
{
    uint32_t sp = stack_get_current_sp();
    
    if (sp < STACK_BASE || sp > STACK_TOP) {
        return 0;  /* Stack pointer out of bounds - critical error! */
    }
    
    return sp - STACK_BASE;
}

/**
 * Get stack space that has been used
 * @return Number of bytes used on stack
 */
uint32_t stack_get_used_space(void)
{
    uint32_t sp = stack_get_current_sp();
    
    if (sp < STACK_BASE || sp > STACK_TOP) {
        return 0;
    }
    
    return STACK_TOP - sp;
}

/**
 * Check for heap-stack collision
 * Returns 1 if stack has grown into heap space (critical error!)
 * @return 1 if collision detected, 0 if safe
 */
int memory_check_collision(void)
{
    uint32_t sp = stack_get_current_sp();
    
    /* Check if stack has grown into interpreter heap */
    if (sp <= (uint32_t)(intptr_t)interpreter_heap.current) {
        return 1;  /* COLLISION! */
    }
    
    return 0;  /* Safe */
}

/**
 * Print formatted stack status
 */
void memory_print_stack_status(void)
{
    uint32_t sp = stack_get_current_sp();
    uint32_t free = stack_get_free_space();
    uint32_t used = stack_get_used_space();
    
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           STACK STATUS                              ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ Current SP:  0x%08x                             ║\n", sp);
    printf("║ Used Space:  %u bytes                          ║\n", used);
    printf("║ Free Space:  %u bytes                          ║\n", free);
    
    if (memory_check_collision()) {
        printf("║ ⚠️  COLLISION DETECTED!                            ║\n");
    } else {
        printf("║ ✓ No collision detected                            ║\n");
    }
    
    printf("╚════════════════════════════════════════════════════╝\n\n");
}

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
 * Output goes to printf
 */
void memory_health_check(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║       LITTLEOS MEMORY HEALTH CHECK                  ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    
    /* Validate layout */
    if (!memory_validate_layout()) {
        printf("\n❌ CRITICAL: Memory layout is invalid!\n");
        printf("   Kernel and interpreter heaps may overlap.\n");
        return;
    }
    printf("\n✓ Memory layout is valid\n");
    
    /* Print memory map */
    memory_print_layout();
    
    /* Print statistics */
    memory_print_stats();
    
    /* Print stack status */
    memory_print_stack_status();
    
    /* Overall status */
    printf("╔════════════════════════════════════════════════════╗\n");
    if (!memory_check_collision() && memory_validate_layout()) {
        printf("║ STATUS: ✓ HEALTHY                                  ║\n");
    } else {
        printf("║ STATUS: ❌ CRITICAL ERRORS                         ║\n");
    }
    printf("╚════════════════════════════════════════════════════╝\n\n");
}
