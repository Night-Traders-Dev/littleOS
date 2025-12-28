// include/scheduler.h

#ifndef LITTLEOS_SCHEDULER_H
#define LITTLEOS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "permissions.h"
#include "memory_segmented.h"

/*
 * littleOS Multi-Core Scheduler
 *
 * Provides:
 * - Task creation/destruction
 * - Per-task memory tracking
 * - Per-task security contexts
 * - Task scheduling across cores
 * - IPC (Inter-Process Communication)
 */

/* ============================================================================
 * Task Definitions
 * ========================================================================== */

#define LITTLEOS_MAX_TASKS       16
#define LITTLEOS_TASK_STACK_SIZE 4096
#define LITTLEOS_MAX_TASK_NAME   32

typedef enum {
    TASK_STATE_IDLE,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_SUSPENDED,
    TASK_STATE_TERMINATED,
} task_state_t;

typedef enum {
    TASK_PRIORITY_LOW      = 0,
    TASK_PRIORITY_NORMAL   = 1,
    TASK_PRIORITY_HIGH     = 2,
    TASK_PRIORITY_CRITICAL = 3,
} task_priority_t;

typedef void (*task_entry_t)(void *arg);

typedef struct {
    uint16_t        task_id;              /* Unique task ID           */
    char            name[LITTLEOS_MAX_TASK_NAME]; /* Task name        */
    task_state_t    state;                /* Current state            */
    task_priority_t priority;             /* Task priority            */
    uint8_t         core_affinity;        /* 0=Core0, 1=Core1, 2=Any  */
    task_entry_t    entry_func;           /* Entry point              */
    void           *arg;                  /* Argument to entry        */

    task_sec_ctx_t  sec_ctx;              /* Security context         */

    uint32_t        stack_base;           /* Stack base address       */
    uint32_t        stack_size;           /* Stack size               */

    uint32_t        memory_allocated;     /* Bytes allocated by task  */
    uint32_t        memory_peak;          /* Peak memory usage        */

    uint32_t        created_at_ms;        /* Creation timestamp       */
    uint32_t        total_runtime_ms;     /* Total execution time     */
    uint32_t        context_switches;     /* Number of context switches */

    uint8_t         reserved[64];         /* Reserved for future use  */
} task_descriptor_t;

/* ============================================================================
 * Task Management Functions
 * ========================================================================== */

/**
 * Initialize task management system
 */
void scheduler_init(void);

/**
 * Backwards-compatible alias (if older code calls task_manager_init)
 */
static inline void task_manager_init(void) {
    scheduler_init();
}

/**
 * Create a new task
 *
 * @param name    Task name (for debugging)
 * @param entry   Entry function pointer
 * @param arg     Argument to pass to entry
 * @param priority Task priority
 * @param core    Core affinity (0, 1, or 2 for any)
 * @param uid     User ID for task
 * @return Task ID, or 0xFFFF if failed
 */
uint16_t task_create(const char *name, task_entry_t entry, void *arg,
                     task_priority_t priority, uint8_t core, uid_t uid);

/**
 * Terminate a task
 *
 * @param task_id Task to terminate
 * @return true if successful
 */
bool task_terminate(uint16_t task_id);

/**
 * Get task descriptor
 *
 * @param task_id Task ID
 * @param desc    Output descriptor
 * @return true if found
 */
bool task_get_descriptor(uint16_t task_id, task_descriptor_t *desc);

/**
 * List all tasks
 *
 * @param buffer      Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written
 */
int task_list(char *buffer, size_t buffer_size);

/**
 * Get current task ID
 *
 * @return Current task ID
 */
uint16_t task_get_current(void);

/**
 * Get number of active tasks
 *
 * @return Task count
 */
uint16_t task_get_count(void);

/**
 * Report memory allocation from a task
 *
 * @param task_id   Task ID
 * @param allocated Bytes allocated (positive) or freed (negative)
 */
void task_report_memory(uint16_t task_id, int allocated);

/**
 * Suspend a task
 *
 * @param task_id Task to suspend
 * @return true if successful
 */
bool task_suspend(uint16_t task_id);

/**
 * Resume a suspended task
 *
 * @param task_id Task to resume
 * @return true if successful
 */
bool task_resume(uint16_t task_id);

/**
 * Get task statistics
 *
 * @param task_id Task ID
 * @param buffer  Output buffer
 * @param size    Buffer size
 * @return Number of bytes written
 */
int task_get_stats(uint16_t task_id, char *buffer, size_t size);

/* ============================================================================
 * Scheduler helpers
 * ========================================================================== */

/**
 * Select next runnable task for Core 0
 */
uint16_t scheduler_next_task_core0(void);

/**
 * Select next runnable task for Core 1
 */
uint16_t scheduler_next_task_core1(void);

/**
 * Update runtime statistics for a task
 *
 * @param task_id    Task ID
 * @param elapsed_ms Time spent running since last update
 */
void scheduler_update_runtime(uint16_t task_id, uint32_t elapsed_ms);

/**
 * Count tasks that are READY or RUNNING
 */
uint16_t scheduler_count_ready_tasks(void);

#endif /* LITTLEOS_SCHEDULER_H */
