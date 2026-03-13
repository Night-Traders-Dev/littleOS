/**
 * littleOS Segmented Memory Manager
 *
 * Separates kernel and interpreter heap spaces to prevent allocation conflicts.
 * Uses Pico SDK linker symbols to place heaps AFTER .data/.bss in SRAM,
 * avoiding overlap with program globals.
 *
 * RP2040: 264 KB SRAM (0x20000000 - 0x20042000)
 *   - .data/.bss: placed by linker at start of SRAM
 *   - Kernel Heap: starts at __end__ (after BSS), 32 KB
 *   - Interpreter Heap: follows kernel heap, 32 KB
 *   - Remaining SRAM: available for newlib malloc
 *
 * Stack lives in SCRATCH_Y on Pico SDK, not main SRAM.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "memory_segmented.h"

/* ============================================================================
 * Linker-provided symbols (from Pico SDK memmap_default.ld)
 * ============================================================================ */

extern char __end__;         /* End of BSS - first free byte in SRAM */
extern char __HeapLimit;     /* Top of usable SRAM (ORIGIN(RAM) + LENGTH(RAM)) */
extern char __StackTop;      /* Top of stack (in SCRATCH_Y on RP2040) */
extern char __StackBottom;   /* Bottom of stack */

/* ============================================================================
 * Heap sizing
 * ============================================================================ */

#define KERNEL_HEAP_SIZE      (32 * 1024)    /* 32 KB */
#define INTERPRETER_HEAP_SIZE (32 * 1024)    /* 32 KB */

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
 * Global memory regions - initialized dynamically in memory_init()
 * ============================================================================ */

static MemoryRegion kernel_heap;
static MemoryRegion interpreter_heap;
static bool memory_initialized = false;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * Initialize memory management system.
 * MUST be called before any kernel_malloc/interpreter_malloc operations!
 * Places heaps after the linker's __end__ to avoid overlapping .data/.bss.
 */
void memory_init(void)
{
    uint8_t *heap_start = (uint8_t *)&__end__;
    uint8_t *sram_end   = (uint8_t *)&__HeapLimit;

    /* Align start to 8 bytes */
    heap_start = (uint8_t *)(((uintptr_t)heap_start + 7) & ~(uintptr_t)7);

    size_t available = (size_t)(sram_end - heap_start);
    size_t k_size = KERNEL_HEAP_SIZE;
    size_t i_size = INTERPRETER_HEAP_SIZE;

    /* If not enough room, shrink proportionally */
    if (k_size + i_size > available) {
        k_size = available / 2;
        i_size = available - k_size;
    }

    /* Kernel heap: starts right after BSS */
    kernel_heap.start            = heap_start;
    kernel_heap.end              = heap_start + k_size;
    kernel_heap.current          = kernel_heap.start;
    kernel_heap.max_size         = k_size;
    kernel_heap.used_size        = 0;
    kernel_heap.peak_size        = 0;
    kernel_heap.allocation_count = 0;
    kernel_heap.name             = "KERNEL_HEAP";

    /* Interpreter heap: follows kernel heap */
    interpreter_heap.start            = kernel_heap.end;
    interpreter_heap.end              = kernel_heap.end + i_size;
    interpreter_heap.current          = interpreter_heap.start;
    interpreter_heap.max_size         = i_size;
    interpreter_heap.used_size        = 0;
    interpreter_heap.peak_size        = 0;
    interpreter_heap.allocation_count = 0;
    interpreter_heap.name             = "INTERPRETER_HEAP";

    memory_initialized = true;
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
    if (size == 0 || size > SIZE_MAX - 7) {
        return NULL;
    }

    /* Align to 8 bytes for proper alignment on ARM */
    size = (size + 7) & ~(size_t)7;

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
    /* Check for multiplication overflow */
    if (count != 0 && size > (SIZE_MAX / count)) {
        return NULL;
    }

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
    if (size == 0 || size > SIZE_MAX - 7) {
        return NULL;
    }

    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

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
    /* Check for multiplication overflow */
    if (count != 0 && size > (SIZE_MAX / count)) {
        return NULL;
    }

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
 * Get current memory statistics
 * @return Populated MemoryStats structure
 */
MemoryStats memory_get_stats(void)
{
    MemoryStats stats = {0};

    if (!memory_initialized) {
        return stats;
    }

    stats.kernel_used = kernel_heap.used_size;
    stats.kernel_free = kernel_heap.max_size - kernel_heap.used_size;
    stats.kernel_peak = kernel_heap.peak_size;
    stats.interpreter_used = interpreter_heap.used_size;
    stats.interpreter_free = interpreter_heap.max_size - interpreter_heap.used_size;
    stats.interpreter_peak = interpreter_heap.peak_size;
    stats.kernel_alloc_count = kernel_heap.allocation_count;
    stats.interpreter_alloc_count = interpreter_heap.allocation_count;

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
 */
void memory_print_stats(void)
{
    MemoryStats stats = memory_get_stats();

    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           MEMORY STATISTICS                          ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");

    printf("║ KERNEL HEAP:                                       ║\n");
    printf("║   Used: %6u bytes (%5.1f%%)                    ║\n",
                (unsigned int)stats.kernel_used, stats.kernel_usage_pct);
    printf("║   Free: %6u bytes                              ║\n",
                (unsigned int)stats.kernel_free);
    printf("║   Peak: %6u bytes                              ║\n",
                (unsigned int)stats.kernel_peak);
    printf("║   Allocations: %u                                ║\n",
                stats.kernel_alloc_count);

    printf("╟────────────────────────────────────────────────────╢\n");

    printf("║ INTERPRETER HEAP:                                  ║\n");
    printf("║   Used: %6u bytes (%5.1f%%)                    ║\n",
                (unsigned int)stats.interpreter_used, stats.interpreter_usage_pct);
    printf("║   Free: %6u bytes                              ║\n",
                (unsigned int)stats.interpreter_free);
    printf("║   Peak: %6u bytes                              ║\n",
                (unsigned int)stats.interpreter_peak);
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
    printf("║           MEMORY LAYOUT (264 KB SRAM)               ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");

    printf("║ BSS end:          0x%08x                       ║\n",
                (unsigned int)(uintptr_t)&__end__);
    printf("║ Kernel Heap:      0x%08x - 0x%08x        ║\n",
                (unsigned int)(uintptr_t)kernel_heap.start,
                (unsigned int)(uintptr_t)kernel_heap.end);
    printf("║ Interpreter Heap: 0x%08x - 0x%08x        ║\n",
                (unsigned int)(uintptr_t)interpreter_heap.start,
                (unsigned int)(uintptr_t)interpreter_heap.end);
    printf("║ SRAM top:         0x%08x                       ║\n",
                (unsigned int)(uintptr_t)&__HeapLimit);

    printf("╚════════════════════════════════════════════════════╝\n\n");
}

/**
 * Validate memory region layout
 * Checks that kernel heap, interpreter heap don't overlap program data
 * @return 1 if layout is valid, 0 if invalid
 */
int memory_validate_layout(void)
{
    if (!memory_initialized) return 0;

    /* Check each region has valid bounds */
    if (kernel_heap.start >= kernel_heap.end) {
        return 0;
    }
    if (interpreter_heap.start >= interpreter_heap.end) {
        return 0;
    }

    /* Kernel heap must start at or after __end__ */
    if (kernel_heap.start < (uint8_t *)&__end__) {
        return 0;  /* Overlaps program data! */
    }

    /* Check regions don't overlap each other */
    if (kernel_heap.end > interpreter_heap.start) {
        return 0;  /* Kernel heap overlaps interpreter heap! */
    }

    /* Interpreter heap must not exceed SRAM */
    if (interpreter_heap.end > (uint8_t *)&__HeapLimit) {
        return 0;  /* Exceeds SRAM! */
    }

    /* All checks passed */
    return 1;
}

/* ============================================================================
 * Stack Monitoring
 *
 * On RP2040 with Pico SDK, the main stack lives in SCRATCH_Y,
 * not in main SRAM.  We use the linker symbols __StackTop/__StackBottom.
 * ============================================================================ */

/**
 * Get current stack pointer value
 * @return Current SP register value
 */
uint32_t stack_get_current_sp(void)
{
    uint32_t sp;
#if __riscv
    asm volatile("mv %0, sp" : "=r" (sp));
#else
    asm volatile("mov %0, sp" : "=r" (sp));
#endif
    return sp;
}

/**
 * Get free stack space remaining
 * @return Number of bytes still available on stack
 */
uint32_t stack_get_free_space(void)
{
    uint32_t sp = stack_get_current_sp();
    uint32_t bottom = (uint32_t)(uintptr_t)&__StackBottom;
    uint32_t top    = (uint32_t)(uintptr_t)&__StackTop;

    if (sp < bottom || sp > top) {
        return 0;  /* Stack pointer out of bounds */
    }

    return sp - bottom;
}

/**
 * Get stack space that has been used
 * @return Number of bytes used on stack
 */
uint32_t stack_get_used_space(void)
{
    uint32_t sp = stack_get_current_sp();
    uint32_t top = (uint32_t)(uintptr_t)&__StackTop;

    if (sp > top) {
        return 0;
    }

    return top - sp;
}

/**
 * Check for heap-stack collision
 * On RP2040 with Pico SDK, stack is in SCRATCH_Y so collisions with
 * SRAM heaps are not possible. We check the interpreter heap
 * hasn't grown past its boundary.
 * @return 1 if collision detected, 0 if safe
 */
int memory_check_collision(void)
{
    if (!memory_initialized) return 1;

    /* Check interpreter heap hasn't overrun */
    if (interpreter_heap.current > interpreter_heap.end) {
        return 1;  /* OVERFLOW! */
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
    printf("║ Stack range: 0x%08x - 0x%08x              ║\n",
                (unsigned int)(uintptr_t)&__StackBottom,
                (unsigned int)(uintptr_t)&__StackTop);
    printf("║ Used Space:  %u bytes                          ║\n", used);
    printf("║ Free Space:  %u bytes                          ║\n", free);

    if (memory_check_collision()) {
        printf("║ WARNING: Heap overflow detected!                   ║\n");
    } else {
        printf("║ OK: No collision detected                          ║\n");
    }

    printf("╚════════════════════════════════════════════════════╝\n\n");
}

/* ============================================================================
 * System Health Check
 * ============================================================================ */

/**
 * Run comprehensive memory health check
 */
void memory_health_check(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║       LITTLEOS MEMORY HEALTH CHECK                  ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");

    /* Validate layout */
    if (!memory_validate_layout()) {
        printf("\nCRITICAL: Memory layout is invalid!\n");
        printf("   Heaps may overlap program data.\n");
        return;
    }
    printf("\nMemory layout is valid\n");

    /* Print memory map */
    memory_print_layout();

    /* Print statistics */
    memory_print_stats();

    /* Print stack status */
    memory_print_stack_status();

    /* Overall status */
    printf("╔════════════════════════════════════════════════════╗\n");
    if (!memory_check_collision() && memory_validate_layout()) {
        printf("║ STATUS: HEALTHY                                    ║\n");
    } else {
        printf("║ STATUS: CRITICAL ERRORS                            ║\n");
    }
    printf("╚════════════════════════════════════════════════════╝\n\n");
}

/* ============================================================================
 * Task-Aware Memory Accounting
 *
 * Tracks per-task allocation statistics on top of the existing bump
 * allocators.  No additional heap is consumed -- only a small static
 * table of accounts.
 * ============================================================================ */

#define MAX_TRACKED_TASKS 16

typedef struct {
    uint16_t task_id;
    uint32_t allocated_bytes;
    uint32_t peak_bytes;
    uint32_t alloc_count;
    uint32_t free_count;
    bool     active;
} task_memory_account_t;

static task_memory_account_t task_accounts[MAX_TRACKED_TASKS];
static bool accounting_initialized = false;

/**
 * Initialize the per-task memory accounting table.
 */
void memory_accounting_init(void)
{
    memset(task_accounts, 0, sizeof(task_accounts));
    accounting_initialized = true;
}

/**
 * Find an existing account for task_id, or allocate a new one.
 * Returns pointer to account, or NULL if table is full.
 */
static task_memory_account_t *find_or_create_account(uint16_t task_id)
{
    int free_slot = -1;

    for (int i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_accounts[i].active && task_accounts[i].task_id == task_id) {
            return &task_accounts[i];
        }
        if (!task_accounts[i].active && free_slot < 0) {
            free_slot = i;
        }
    }

    if (free_slot >= 0) {
        memset(&task_accounts[free_slot], 0, sizeof(task_memory_account_t));
        task_accounts[free_slot].task_id = task_id;
        task_accounts[free_slot].active = true;
        return &task_accounts[free_slot];
    }

    return NULL;
}

/**
 * Record an allocation against a task.
 */
void memory_account_alloc(uint16_t task_id, uint32_t bytes)
{
    if (!accounting_initialized) memory_accounting_init();

    task_memory_account_t *acct = find_or_create_account(task_id);
    if (!acct) return;

    acct->allocated_bytes += bytes;
    acct->alloc_count++;

    if (acct->allocated_bytes > acct->peak_bytes) {
        acct->peak_bytes = acct->allocated_bytes;
    }
}

/**
 * Record a free against a task.
 */
void memory_account_free(uint16_t task_id, uint32_t bytes)
{
    if (!accounting_initialized) return;

    task_memory_account_t *acct = find_or_create_account(task_id);
    if (!acct) return;

    if (bytes > acct->allocated_bytes) {
        acct->allocated_bytes = 0;
    } else {
        acct->allocated_bytes -= bytes;
    }
    acct->free_count++;
}

/**
 * Get a task's current memory statistics.
 * @return 0 on success, -1 if task not found
 */
int memory_account_get(uint16_t task_id, uint32_t *allocated, uint32_t *peak)
{
    if (!accounting_initialized) return -1;

    for (int i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_accounts[i].active && task_accounts[i].task_id == task_id) {
            if (allocated) *allocated = task_accounts[i].allocated_bytes;
            if (peak)      *peak      = task_accounts[i].peak_bytes;
            return 0;
        }
    }
    return -1;
}

/**
 * Print all active task memory accounts.
 */
void memory_account_print_all(void)
{
    printf("\n=== Task Memory Accounts ===\n");
    printf("  %-6s  %-10s  %-10s  %-6s  %-6s\n",
           "TaskID", "Allocated", "Peak", "Allocs", "Frees");

    bool found = false;
    for (int i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_accounts[i].active) {
            found = true;
            printf("  %-6u  %-10u  %-10u  %-6u  %-6u\n",
                   (unsigned)task_accounts[i].task_id,
                   (unsigned)task_accounts[i].allocated_bytes,
                   (unsigned)task_accounts[i].peak_bytes,
                   (unsigned)task_accounts[i].alloc_count,
                   (unsigned)task_accounts[i].free_count);
        }
    }

    if (!found) {
        printf("  (no active task accounts)\n");
    }
}

/**
 * Reset a single task's accounting entry.
 */
void memory_account_reset(uint16_t task_id)
{
    for (int i = 0; i < MAX_TRACKED_TASKS; i++) {
        if (task_accounts[i].active && task_accounts[i].task_id == task_id) {
            memset(&task_accounts[i], 0, sizeof(task_memory_account_t));
            return;
        }
    }
}

/**
 * Allocate from the kernel heap and track against a task.
 */
void *kernel_malloc_tracked(size_t size, uint16_t task_id)
{
    void *ptr = kernel_malloc(size);
    if (ptr) {
        /* Account the aligned size (same rounding as kernel_malloc) */
        uint32_t aligned = (uint32_t)((size + 7) & ~(size_t)7);
        memory_account_alloc(task_id, aligned);
    }
    return ptr;
}

/**
 * Allocate from the interpreter heap and track against a task.
 */
void *interpreter_malloc_tracked(size_t size, uint16_t task_id)
{
    void *ptr = interpreter_malloc(size);
    if (ptr) {
        uint32_t aligned = (uint32_t)((size + 7) & ~(size_t)7);
        memory_account_alloc(task_id, aligned);
    }
    return ptr;
}
