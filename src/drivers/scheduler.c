// src/drivers/scheduler.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
#include "watchdog.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/scb.h"
#include "hardware/sync.h"
#endif

#ifdef PICO_BUILD
static spin_lock_t *sched_lock;
#define SCHED_LOCK() uint32_t __save = spin_lock_blocking(sched_lock)
#define SCHED_UNLOCK() spin_unlock(sched_lock, __save)
#define SCHED_RETURN(val) do { int __ret = (val); SCHED_UNLOCK(); return __ret; } while(0)
#define SCHED_RETURN_PTR(val) do { void *__ret = (val); SCHED_UNLOCK(); return __ret; } while(0)
#define SCHED_RETURN_VOID() do { SCHED_UNLOCK(); return; } while(0)
#else
#define SCHED_LOCK()
#define SCHED_UNLOCK()
#define SCHED_RETURN(val) return (val)
#define SCHED_RETURN_PTR(val) return (val)
#define SCHED_RETURN_VOID() return
#endif


static task_descriptor_t task_table[LITTLEOS_MAX_TASKS];
static uint16_t task_count         = 0;
static uint16_t current_task_id    = 0;
static bool     scheduler_initialized = false;

/* Preemptive scheduling state */
static volatile uint32_t system_ticks       = 0;
static volatile bool     preemption_enabled = false;
static volatile uint16_t running_task_core0 = 0;

/* Cached pointer to current running task (avoids find_task in hot path) */
static volatile task_descriptor_t *running_task_ptr = NULL;

/* Active scheduling policy */
static sched_policy_t active_policy = SCHED_POLICY_PRIORITY;

/* CFS weight table: lower priority = higher weight = slower vruntime growth */
static const uint32_t cfs_weight[] = {
    [TASK_PRIORITY_LOW]      = 4,   /* runs 4x slower than critical */
    [TASK_PRIORITY_NORMAL]   = 2,
    [TASK_PRIORITY_HIGH]     = 1,
    [TASK_PRIORITY_CRITICAL] = 1,
};

/* Default time slices per priority level (ms) */
static const uint32_t default_timeslice[] = {
    [TASK_PRIORITY_LOW]      = 50,
    [TASK_PRIORITY_NORMAL]   = 20,
    [TASK_PRIORITY_HIGH]     = 10,
    [TASK_PRIORITY_CRITICAL] = 5,
};

/* Round-robin uses a fixed time slice regardless of priority */
#define RR_TIMESLICE_MS 20

typedef struct {
    uint16_t tasks[LITTLEOS_MAX_TASKS];
    uint16_t count;
    uint16_t current_index;
} task_queue_t;

static task_queue_t core0_queue = {0};
static task_queue_t core1_queue = {0};

/* ============================================================================
 * Internal helpers
 * ========================================================================== */

static uint16_t alloc_task_id(void) {
    static uint16_t next_id = 1;

    for (uint16_t search_id = 1; search_id < 0xFFFF; search_id++) {
        uint16_t candidate = next_id++;
        if (next_id == 0) next_id = 1;
        int found = 0;
        for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
            if (task_table[i].state != TASK_STATE_IDLE && task_table[i].state != TASK_STATE_TERMINATED && task_table[i].task_id == candidate) {
                found = 1;
                break;
            }
        }
        if (!found) return candidate;
    }
    return 1;
}

static task_descriptor_t *find_task(uint16_t task_id) {
    if (task_id == 0) return NULL;
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state != TASK_STATE_IDLE && task_table[i].state != TASK_STATE_TERMINATED && task_table[i].task_id == task_id) {
            return &task_table[i];
        }
    }
    return NULL;
}

static int find_task_index(uint16_t task_id) {
    if (task_id == 0) return -1;
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state != TASK_STATE_IDLE && task_table[i].state != TASK_STATE_TERMINATED && task_table[i].task_id == task_id) {
            return (int)i;
        }
    }
    return -1;
}

static void add_to_queue(task_queue_t *queue, uint16_t task_id) {
    if (queue->count < LITTLEOS_MAX_TASKS) {
        queue->tasks[queue->count++] = task_id;
    }
}

static void remove_from_queue(task_queue_t *queue, uint16_t task_id) {
    for (uint16_t i = 0; i < queue->count; i++) {
        if (queue->tasks[i] == task_id) {
            for (uint16_t j = i; j < queue->count - 1; j++) {
                queue->tasks[j] = queue->tasks[j + 1];
            }
            queue->count--;
            if (queue->current_index >= queue->count && queue->count > 0) {
                queue->current_index = 0;
            }
            return;
        }
    }
}

static uint32_t get_timestamp_ms(void) {
#ifdef PICO_BUILD
    return to_ms_since_boot(get_absolute_time());
#else
    return 0;
#endif
}

/* ============================================================================
 * Scheduling Policies
 * ========================================================================== */

/**
 * Fixed-priority scheduler: always picks the highest-priority ready task.
 * Among equal priority, round-robins via current_index so tasks at the
 * same level get fair rotation.
 */
static uint16_t select_priority(task_queue_t *queue) {
    if (!queue || queue->count == 0) return 0;

    /* First pass: find the highest priority among ready tasks */
    int best_priority = -1;
    for (uint16_t i = 0; i < queue->count; i++) {
        task_descriptor_t *task = find_task(queue->tasks[i]);
        if (!task) continue;
        if (task->state != TASK_STATE_READY && task->state != TASK_STATE_RUNNING) continue;
        if ((int)task->priority > best_priority) {
            best_priority = (int)task->priority;
        }
    }
    if (best_priority < 0) return 0;

    /* Second pass: round-robin among tasks at that priority level */
    uint16_t start = queue->current_index % queue->count;
    for (uint16_t offset = 0; offset < queue->count; offset++) {
        uint16_t idx = (uint16_t)((start + offset) % queue->count);
        task_descriptor_t *task = find_task(queue->tasks[idx]);
        if (!task) continue;
        if (task->state != TASK_STATE_READY && task->state != TASK_STATE_RUNNING) continue;
        if ((int)task->priority == best_priority) {
            queue->current_index = (uint16_t)((idx + 1) % queue->count);
            return queue->tasks[idx];
        }
    }

    return 0;
}

/**
 * Round-robin scheduler: rotate through all ready tasks regardless of priority.
 * Each task gets an equal time slice.
 */
static uint16_t select_round_robin(task_queue_t *queue) {
    if (!queue || queue->count == 0) return 0;

    uint16_t start = queue->current_index % queue->count;

    for (uint16_t offset = 0; offset < queue->count; offset++) {
        uint16_t idx = (uint16_t)((start + offset) % queue->count);
        uint16_t task_id = queue->tasks[idx];
        task_descriptor_t *task = find_task(task_id);

        if (!task) continue;
        if (task->state != TASK_STATE_READY && task->state != TASK_STATE_RUNNING) continue;

        queue->current_index = (uint16_t)((idx + 1) % queue->count);
        return task_id;
    }

    return 0;
}

/**
 * CFS-like scheduler: pick the task with the lowest virtual runtime.
 * Virtual runtime grows proportionally to actual runtime, weighted by priority.
 * Lower-priority tasks accumulate vruntime faster, so higher-priority tasks
 * are naturally favored while still ensuring fairness.
 */
static uint16_t select_cfs(task_queue_t *queue) {
    if (!queue || queue->count == 0) return 0;

    uint64_t min_vruntime = UINT64_MAX;
    uint16_t selected_task = 0;

    for (uint16_t i = 0; i < queue->count; i++) {
        uint16_t task_id = queue->tasks[i];
        task_descriptor_t *task = find_task(task_id);

        if (!task) continue;
        if (task->state != TASK_STATE_READY && task->state != TASK_STATE_RUNNING) continue;

        if (task->vruntime < min_vruntime) {
            min_vruntime = task->vruntime;
            selected_task = task_id;
        }
    }

    return selected_task;
}

static uint16_t scheduler_select_next(task_queue_t *queue) {
    if (!queue || queue->count == 0) return 0;

    switch (active_policy) {
    case SCHED_POLICY_ROUND_ROBIN:
        return select_round_robin(queue);
    case SCHED_POLICY_CFS:
        return select_cfs(queue);
    case SCHED_POLICY_PRIORITY:
    default:
        return select_priority(queue);
    }
}

/* ============================================================================
 * Public API
 * ========================================================================== */

void scheduler_init(void) {
    if (scheduler_initialized) {
        return;
    }
#ifdef PICO_BUILD
    sched_lock = spin_lock_instance(spin_lock_claim_unused(true));
#endif

    memset(task_table, 0, sizeof(task_table));
    task_count      = 0;
    current_task_id = 0;
    running_task_ptr = NULL;
    active_policy = SCHED_POLICY_PRIORITY;

    memset(&core0_queue, 0, sizeof(task_queue_t));
    memset(&core1_queue, 0, sizeof(task_queue_t));

    scheduler_initialized = true;
    printf("Task scheduler initialized\r\n");
}

uint16_t task_create(const char *name, task_entry_t entry, void *arg,
                     task_priority_t priority, uint8_t core, uid_t uid) {
    if (!scheduler_initialized) {
        printf("ERROR: Scheduler not initialized\r\n");
        return 0xFFFF;
    }

    if (!entry) {
        printf("ERROR: Invalid entry function\r\n");
        return 0xFFFF;
    }

    SCHED_LOCK();
    if (task_count >= LITTLEOS_MAX_TASKS) {
        SCHED_UNLOCK();
        printf("ERROR: Task table full\r\n");
        return 0xFFFF;
    }

    task_descriptor_t *task = NULL;
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state == TASK_STATE_IDLE || task_table[i].state == TASK_STATE_TERMINATED) {
            task = &task_table[i];
            break;
        }
    }

    if (!task) {
        SCHED_UNLOCK();
        printf("ERROR: Task table full\r\n");
        return 0xFFFF;
    }

    task->task_id = alloc_task_id();
    strncpy(task->name, name ? name : "unnamed", LITTLEOS_MAX_TASK_NAME - 1);
    task->name[LITTLEOS_MAX_TASK_NAME - 1] = '\0';
    task->state         = TASK_STATE_READY;
    task->priority      = priority;
    task->core_affinity = core;
    task->entry_func    = entry;
    task->arg           = arg;

    task->sec_ctx.uid  = uid;
    task->sec_ctx.euid = uid;
    task->sec_ctx.gid  = GID_USERS;
    task->sec_ctx.egid = GID_USERS;
    task->sec_ctx.umask = 0022;
    task->sec_ctx.capabilities = 0;

    if (uid == UID_ROOT) {
        task->sec_ctx.gid  = GID_ROOT;
        task->sec_ctx.egid = GID_ROOT;
        task->sec_ctx.capabilities = CAP_ALL;
    }
    
    // Temporarily mark as blocked to reserve slot while mallocing
    task->state = TASK_STATE_BLOCKED;
    task_count++;
    SCHED_UNLOCK();

    task->stack_base = (uint32_t)malloc(LITTLEOS_TASK_STACK_SIZE);
    task->stack_size = LITTLEOS_TASK_STACK_SIZE;
    if (!task->stack_base) {
        {
        SCHED_LOCK();
        task->state = TASK_STATE_TERMINATED;
        task_count--;
        SCHED_UNLOCK();
        }
        printf("ERROR: Failed to allocate task stack\r\n");
        return 0xFFFF;
    }

        uint16_t ret_id;
    {
    SCHED_LOCK();
    task->created_at_ms      = get_timestamp_ms();
    task->total_runtime_ms   = 0;
    task->context_switches   = 0;
    task->memory_allocated   = 0;
    task->memory_peak        = 0;

    /* CFS: new tasks start at the minimum vruntime of existing ready
     * tasks.  Starting at 0 would starve all other tasks until this
     * task's vruntime catches up. */
    uint64_t min_vrt = 0;
    if (active_policy == SCHED_POLICY_CFS && task_count > 0) {
        min_vrt = UINT64_MAX;
        for (uint16_t vi = 0; vi < LITTLEOS_MAX_TASKS; vi++) {
            if (task_table[vi].state == TASK_STATE_READY ||
                task_table[vi].state == TASK_STATE_RUNNING) {
                if (task_table[vi].vruntime < min_vrt) {
                    min_vrt = task_table[vi].vruntime;
                }
            }
        }
        if (min_vrt == UINT64_MAX) min_vrt = 0;
    }
    task->vruntime = min_vrt;

    /* Set time slice based on policy */
    uint32_t timeslice;
    if (active_policy == SCHED_POLICY_ROUND_ROBIN) {
        timeslice = RR_TIMESLICE_MS;
    } else {
        timeslice = default_timeslice[priority];
    }
    task->time_slice_ms     = timeslice;
    task->time_remaining_ms = timeslice;
    task->needs_switch      = false;

    /* Set up initial stack frame for context switching.
     * ARM Cortex-M0+ exception entry automatically pushes:
     *   xPSR, PC, LR, R12, R3, R2, R1, R0  (8 words, high-to-low)
     * We also reserve space for manually-saved R4-R11 (8 words).
     * Stack grows downward; stack_ptr points to top of saved context. */
    uint32_t *stack_top = (uint32_t *)(task->stack_base + task->stack_size);
    /* Hardware-saved exception frame (pushed automatically on exception entry) */
    *(--stack_top) = 0x01000000;              /* xPSR: Thumb bit set           */
    *(--stack_top) = (uint32_t)entry;         /* PC: task entry point          */
    *(--stack_top) = 0xFFFFFFFD;              /* LR: EXC_RETURN thread/PSP     */
    *(--stack_top) = 0;                       /* R12                           */
    *(--stack_top) = 0;                       /* R3                            */
    *(--stack_top) = 0;                       /* R2                            */
    *(--stack_top) = 0;                       /* R1                            */
    *(--stack_top) = (uint32_t)arg;           /* R0: argument to entry func    */
    /* Manually-saved registers (R4-R11) */
    for (int r = 0; r < 8; r++) {
        *(--stack_top) = 0;                   /* R4 through R11                */
    }
    task->stack_ptr = stack_top;

    task->state = TASK_STATE_READY;

    if (core == 0) {
        add_to_queue(&core0_queue, task->task_id);
    } else if (core == 1) {
        add_to_queue(&core1_queue, task->task_id);
    } else {
        if (core0_queue.count <= core1_queue.count) {
            add_to_queue(&core0_queue, task->task_id);
        } else {
            add_to_queue(&core1_queue, task->task_id);
        }
    }
    ret_id = task->task_id;
    SCHED_UNLOCK();
    }

    printf("Created task: %s (ID=%d, uid=%d, priority=%d)\r\n",
           task->name, ret_id, uid, priority);

    return ret_id;
}

bool task_terminate(uint16_t task_id) {
    SCHED_LOCK();
    int idx = find_task_index(task_id);
    if (idx < 0) {
        SCHED_UNLOCK();
        return false;
    }

    task_descriptor_t *task = &task_table[idx];

    // we must unlock to call free safely
    uint32_t stack_base = task->stack_base;
    task->stack_base = 0;
    
    if (task->core_affinity == 0 || task->core_affinity == 2) {
        remove_from_queue(&core0_queue, task_id);
    }

    if (task->core_affinity == 1 || task->core_affinity == 2) {
        remove_from_queue(&core1_queue, task_id);
    }

    task->state = TASK_STATE_TERMINATED;

    if (running_task_ptr == task) {
        running_task_ptr = NULL;
    }

    task_count--;
    SCHED_UNLOCK();

    if (stack_base) {
        free((void *)stack_base);
    }

    printf("Terminated task: %s (ID=%d)\r\n", task->name, task_id);
    return true;
}

bool task_get_descriptor(uint16_t task_id, task_descriptor_t *desc) {
    SCHED_LOCK();

    if (!desc) {
        SCHED_RETURN(false);
    }

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        SCHED_RETURN(false);
    }

    memcpy(desc, task, sizeof(task_descriptor_t));
    SCHED_RETURN(true);

    SCHED_UNLOCK();
}

int task_list(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return 0;
    }

    int written = 0;
    const char *state_names[] = {
        "IDLE", "READY", "RUNNING", "BLOCKED", "SUSPEND", "TERM"
    };

    written += snprintf(buffer + written, buffer_size - written,
                        "\r\n=== Task List (%d tasks, policy: %s) ===\r\n",
                        task_count, scheduler_policy_name(active_policy));

    written += snprintf(buffer + written, buffer_size - written,
                        "ID   Name                 State   Prio Core    Mem UID\r\n");

    written += snprintf(buffer + written, buffer_size - written,
                        "==================================================================\r\n");

    SCHED_LOCK();
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        task_descriptor_t *task = &task_table[i];
        if (task->state == TASK_STATE_IDLE || task->state == TASK_STATE_TERMINATED) continue;
        const char *state = (task->state < 6) ? state_names[task->state] : "?";
        const char *core  = (task->core_affinity == 0) ? "0" :
                            (task->core_affinity == 1) ? "1" : "Any";

        written += snprintf(buffer + written, buffer_size - written,
                            "%-5d %-20s %-7s %d %-4s %7lu %d\r\n",
                            task->task_id,
                            task->name,
                            state,
                            task->priority,
                            core,
                            (unsigned long)task->memory_allocated,
                            task->sec_ctx.uid);
    }

    written += snprintf(buffer + written, buffer_size - written,
                        "==================================================================\r\n");
    SCHED_UNLOCK();
    return written;
}

uint16_t task_get_current(void) {
    return current_task_id;
}

uint16_t task_get_count(void) {
    return task_count;
}

void task_report_memory(uint16_t task_id, int allocated) {
    SCHED_LOCK();

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        SCHED_RETURN_VOID();
    }

    if (allocated > 0) {
        task->memory_allocated += (uint32_t)allocated;
        if (task->memory_allocated > task->memory_peak) {
            task->memory_peak = task->memory_allocated;
        }
    } else if (allocated < 0) {
        uint32_t freed = (uint32_t)(-allocated);
        if (task->memory_allocated >= freed) {
            task->memory_allocated -= freed;
        } else {
            task->memory_allocated = 0;
        }
    }

    SCHED_UNLOCK();
}

bool task_suspend(uint16_t task_id) {
    SCHED_LOCK();

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        SCHED_RETURN(false);
    }

    if (task->state == TASK_STATE_RUNNING || task->state == TASK_STATE_READY) {
        task->state = TASK_STATE_SUSPENDED;
        printf("Suspended task: %s (ID=%d)\r\n", task->name, task_id);
        SCHED_RETURN(true);
    }

    SCHED_RETURN(false);

    SCHED_UNLOCK();
}

bool task_resume(uint16_t task_id) {
    SCHED_LOCK();

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        SCHED_RETURN(false);
    }

    if (task->state == TASK_STATE_SUSPENDED) {
        task->state = TASK_STATE_READY;
        printf("Resumed task: %s (ID=%d)\r\n", task->name, task_id);
        SCHED_RETURN(true);
    }

    SCHED_RETURN(false);

    SCHED_UNLOCK();
}

int task_get_stats(uint16_t task_id, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    SCHED_LOCK();
    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        SCHED_UNLOCK();
        return snprintf(buffer, size, "Task not found\r\n");
    }

    const char *state_names[] = {
        "IDLE", "READY", "RUNNING", "BLOCKED", "SUSPENDED", "TERMINATED"
    };

    const char *state = (task->state < 6) ? state_names[task->state] : "?";

    int __ret = snprintf(buffer, size,
        "\r\n=== Task Statistics: %s ===\r\n"
        "Task ID: %d\r\n"
        "State: %s\r\n"
        "Priority: %d\r\n"
        "Core Affinity: %d\r\n"
        "UID: %d\r\n"
        "Memory Used: %lu bytes\r\n"
        "Memory Peak: %lu bytes\r\n"
        "Stack Size: %lu bytes\r\n"
        "Runtime: %lu ms\r\n"
        "Context Switches: %lu\r\n"
        "Virtual Runtime: %lu\r\n"
        "==============================\r\n",
        task->name,
        task->task_id,
        state,
        task->priority,
        task->core_affinity,
        task->sec_ctx.uid,
        (unsigned long)task->memory_allocated,
        (unsigned long)task->memory_peak,
        (unsigned long)task->stack_size,
        (unsigned long)task->total_runtime_ms,
        (unsigned long)task->context_switches,
        (unsigned long)task->vruntime);
    SCHED_UNLOCK();
    return __ret;
}

/* ============================================================================
 * Scheduler Policy API
 * ========================================================================== */

static const char *policy_names[] = {
    [SCHED_POLICY_PRIORITY]    = "priority",
    [SCHED_POLICY_ROUND_ROBIN] = "round-robin",
    [SCHED_POLICY_CFS]         = "cfs",
};

bool scheduler_set_policy(sched_policy_t policy) {
    if (policy >= SCHED_POLICY_COUNT) {
        return false;
    }
    SCHED_LOCK();
    active_policy = policy;

    /* Adjust time slices and reset CFS state for new policy */
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        task_descriptor_t *task = &task_table[i];
        if (task->state == TASK_STATE_IDLE || task->state == TASK_STATE_TERMINATED) continue;
        if (policy == SCHED_POLICY_ROUND_ROBIN) {
            task->time_slice_ms = RR_TIMESLICE_MS;
        } else {
            task->time_slice_ms = default_timeslice[task->priority];
        }
        task->time_remaining_ms = task->time_slice_ms;

        /* Reset virtual runtimes so all tasks start fair under CFS */
        if (policy == SCHED_POLICY_CFS) {
            task->vruntime = 0;
        }
    }

    SCHED_UNLOCK();
    printf("Scheduler policy: %s\r\n", policy_names[policy]);
    return true;
}

sched_policy_t scheduler_get_policy(void) {
    return active_policy;
}

const char *scheduler_policy_name(sched_policy_t policy) {
    if (policy < SCHED_POLICY_COUNT) {
        return policy_names[policy];
    }
    return "unknown";
}

/* ============================================================================
 * Scheduler helpers
 * ========================================================================== */

uint16_t scheduler_next_task_core0(void) {
    uint16_t selected_task = scheduler_select_next(&core0_queue);

    if (selected_task) {
        current_task_id = selected_task;
    }

    return selected_task;
}

uint16_t scheduler_next_task_core1(void) {
    uint16_t selected_task = scheduler_select_next(&core1_queue);

    if (selected_task) {
        current_task_id = selected_task;
    }

    return selected_task;
}

void scheduler_update_runtime(uint16_t task_id, uint32_t elapsed_ms) {
    SCHED_LOCK();

    task_descriptor_t *task = find_task(task_id);
    if (task) {
        task->total_runtime_ms += elapsed_ms;
        task->context_switches++;
    }

    SCHED_UNLOCK();
}

uint16_t scheduler_count_ready_tasks(void) {
    uint16_t count = 0;
    SCHED_LOCK();
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state == TASK_STATE_READY ||
            task_table[i].state == TASK_STATE_RUNNING) {
            count++;
        }
    }
    SCHED_UNLOCK();
    return count;
}

/* ============================================================================
 * Preemptive Scheduling
 * ========================================================================== */

/* Trigger PendSV exception to perform context switch at lowest priority */
static inline void trigger_pendsv(void) {
#ifdef PICO_BUILD
    /* Set PENDSVSET bit in ICSR (Interrupt Control and State Register) */
    *(volatile uint32_t *)0xE000ED04 = (1 << 28);
#endif
}

void scheduler_tick(void) {
    SCHED_LOCK();

    system_ticks++;

    if (!preemption_enabled) {
        SCHED_RETURN_VOID();
    }

    /* Use cached pointer to avoid O(n) lookup every 1ms tick */
    task_descriptor_t *task = (task_descriptor_t *)running_task_ptr;
    if (!task || task->state != TASK_STATE_RUNNING) {
        SCHED_RETURN_VOID();
    }

    /* Track runtime */
    task->total_runtime_ms++;

    /* CFS: advance virtual runtime weighted by priority */
    if (active_policy == SCHED_POLICY_CFS) {
        uint32_t weight = cfs_weight[task->priority];
        task->vruntime += weight;
    }

    /* Decrement time remaining; trigger switch when expired */
    if (task->time_slice_ms > 0 && task->time_remaining_ms > 0) {
        task->time_remaining_ms--;
        if (task->time_remaining_ms == 0) {
            task->needs_switch = true;
            trigger_pendsv();
        }
    }

    SCHED_UNLOCK();
}

void scheduler_context_switch(void) {
    SCHED_LOCK();

    if (!preemption_enabled) {
        SCHED_RETURN_VOID();
    }

    task_descriptor_t *current = (task_descriptor_t *)running_task_ptr;

    /* Move current task back to READY if it was RUNNING */
    if (current && current->state == TASK_STATE_RUNNING) {
        current->state = TASK_STATE_READY;
        current->needs_switch = false;
        current->context_switches++;
    }

    /* Select next task using active policy.
     * Use find_task once here rather than in the ISR-hot path repeatedly.
     * scheduler_next_task_core0() already calls scheduler_select_next()
     * which walks the queue — we look up the result once. */
    uint16_t next_id = scheduler_next_task_core0();
    if (next_id == 0) {
        /* No ready tasks; keep running current if available */
        if (current) {
            current->state = TASK_STATE_RUNNING;
        }
        SCHED_RETURN_VOID();
    }

    /* Cache the descriptor pointer to avoid repeated O(n) find_task
     * calls from the SysTick handler. */
    task_descriptor_t *next = find_task(next_id);
    if (!next) {
        SCHED_RETURN_VOID();
    }

    running_task_core0 = next_id;
    running_task_ptr = next;
    next->state = TASK_STATE_RUNNING;
    next->time_remaining_ms = next->time_slice_ms;

    SCHED_UNLOCK();
}

void scheduler_start(void) {
    if (!scheduler_initialized) {
        return;
    }

#ifdef PICO_BUILD
    /* Configure PendSV to lowest priority (0xC0 for Cortex-M0+, 2-bit priority) */
    *(volatile uint32_t *)0xE000ED20 |= (0x3 << 22);

    /* Configure SysTick for 1ms ticks at 125 MHz system clock */
    systick_hw->rvr = (125000000 / 1000) - 1;  /* Reload value for 1ms */
    systick_hw->cvr = 0;                        /* Clear current value  */
    systick_hw->csr = 0x7;                      /* Enable, interrupt, processor clock */
#endif

    /* Start the first task */
    uint16_t first = scheduler_next_task_core0();
    if (first) {
        task_descriptor_t *task = find_task(first);
        if (task) {
            task->state = TASK_STATE_RUNNING;
            running_task_core0 = first;
            running_task_ptr = task;
        }
    }

    preemption_enabled = true;
    printf("Preemptive scheduler started (SysTick 1ms, policy: %s)\r\n",
           scheduler_policy_name(active_policy));
}

void scheduler_yield(void) {
    task_descriptor_t *task = (task_descriptor_t *)running_task_ptr;
    if (task) {
        task->time_remaining_ms = 0;
        task->needs_switch = true;
    }
    trigger_pendsv();
}

void scheduler_set_timeslice(uint16_t task_id, uint32_t ms) {
    SCHED_LOCK();

    task_descriptor_t *task = find_task(task_id);
    if (task) {
        task->time_slice_ms     = ms;
        task->time_remaining_ms = ms;
    }

    SCHED_UNLOCK();
}

uint32_t scheduler_get_tick(void) {
    return system_ticks;
}

/* ============================================================================
 * Interrupt Handlers (Pico SDK naming convention)
 * ========================================================================== */

#ifdef PICO_BUILD
void isr_systick(void) {
    scheduler_tick();
}

void isr_pendsv(void) {
    scheduler_context_switch();
}
#endif
