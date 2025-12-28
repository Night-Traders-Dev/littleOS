// src/drivers/scheduler.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
#include "watchdog.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

static task_descriptor_t task_table[LITTLEOS_MAX_TASKS];
static uint16_t task_count         = 0;
static uint16_t current_task_id    = 0;
static bool     scheduler_initialized = false;

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

// Improved task ID allocation to prevent collisions
static uint16_t alloc_task_id(void) {
    static uint16_t next_id = 1;

    if (task_count >= LITTLEOS_MAX_TASKS - 1) {
        // Table nearly full - search for available ID
        for (uint16_t search_id = 1; search_id < 0xFFFF; search_id++) {
            int found = 0;
            for (uint16_t i = 0; i < task_count; i++) {
                if (task_table[i].task_id == search_id) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                return search_id;
            }
        }
    }

    uint16_t id = next_id++;
    if (next_id == 0) {
        next_id = 1;
    }
    return id;
}

static task_descriptor_t *find_task(uint16_t task_id) {
    for (uint16_t i = 0; i < task_count; i++) {
        if (task_table[i].task_id == task_id) {
            return &task_table[i];
        }
    }
    return NULL;
}

static int find_task_index(uint16_t task_id) {
    for (uint16_t i = 0; i < task_count; i++) {
        if (task_table[i].task_id == task_id) {
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
 * Public API
 * ========================================================================== */

void scheduler_init(void) {
    if (scheduler_initialized) {
        return;
    }

    memset(task_table, 0, sizeof(task_table));
    task_count      = 0;
    current_task_id = 0;

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

    if (task_count >= LITTLEOS_MAX_TASKS) {
        printf("ERROR: Task table full\r\n");
        return 0xFFFF;
    }

    if (!entry) {
        printf("ERROR: Invalid entry function\r\n");
        return 0xFFFF;
    }

    task_descriptor_t *task = &task_table[task_count];

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

    task->stack_base = (uint32_t)malloc(LITTLEOS_TASK_STACK_SIZE);
    task->stack_size = LITTLEOS_TASK_STACK_SIZE;
    if (!task->stack_base) {
        printf("ERROR: Failed to allocate task stack\r\n");
        return 0xFFFF;
    }

    task->created_at_ms      = get_timestamp_ms();
    task->total_runtime_ms   = 0;
    task->context_switches   = 0;
    task->memory_allocated   = 0;
    task->memory_peak        = 0;

    task_count++;

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

    printf("Created task: %s (ID=%d, uid=%d, priority=%d)\r\n",
           task->name, task->task_id, uid, priority);

    return task->task_id;
}

bool task_terminate(uint16_t task_id) {
    int idx = find_task_index(task_id);
    if (idx < 0) {
        return false;
    }

    task_descriptor_t *task = &task_table[idx];

    if (task->stack_base) {
        free((void *)task->stack_base);
        task->stack_base = 0;
    }

    if (task->core_affinity == 0 || task->core_affinity == 2) {
        remove_from_queue(&core0_queue, task_id);
    }

    if (task->core_affinity == 1 || task->core_affinity == 2) {
        remove_from_queue(&core1_queue, task_id);
    }

    task->state = TASK_STATE_TERMINATED;

    if (idx < (int)task_count - 1) {
        memcpy(task, &task_table[task_count - 1], sizeof(task_descriptor_t));
    }

    task_count--;

    printf("Terminated task: %s (ID=%d)\r\n", task->name, task_id);
    return true;
}

bool task_get_descriptor(uint16_t task_id, task_descriptor_t *desc) {
    if (!desc) {
        return false;
    }

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        return false;
    }

    memcpy(desc, task, sizeof(task_descriptor_t));
    return true;
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
                        "\r\n=== Task List (%d tasks) ===\r\n", task_count);

    written += snprintf(buffer + written, buffer_size - written,
                        "ID   Name                 State   Prio Core    Mem UID\r\n");

    written += snprintf(buffer + written, buffer_size - written,
                        "==================================================================\r\n");

    for (uint16_t i = 0; i < task_count; i++) {
        task_descriptor_t *task = &task_table[i];
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

    return written;
}

uint16_t task_get_current(void) {
    return current_task_id;
}

uint16_t task_get_count(void) {
    return task_count;
}

void task_report_memory(uint16_t task_id, int allocated) {
    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        return;
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
            printf("WARNING: Task %d freed more than allocated\r\n", task_id);
            task->memory_allocated = 0;
        }
    }
}

bool task_suspend(uint16_t task_id) {
    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        return false;
    }

    if (task->state == TASK_STATE_RUNNING || task->state == TASK_STATE_READY) {
        task->state = TASK_STATE_SUSPENDED;
        printf("Suspended task: %s (ID=%d)\r\n", task->name, task_id);
        return true;
    }

    return false;
}

bool task_resume(uint16_t task_id) {
    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        return false;
    }

    if (task->state == TASK_STATE_SUSPENDED) {
        task->state = TASK_STATE_READY;
        printf("Resumed task: %s (ID=%d)\r\n", task->name, task_id);
        return true;
    }

    return false;
}

int task_get_stats(uint16_t task_id, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        return snprintf(buffer, size, "Task not found\r\n");
    }

    const char *state_names[] = {
        "IDLE", "READY", "RUNNING", "BLOCKED", "SUSPENDED", "TERMINATED"
    };

    const char *state = (task->state < 6) ? state_names[task->state] : "?";

    return snprintf(buffer, size,
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
        (unsigned long)task->context_switches);
}

/* ============================================================================
 * Scheduler helpers
 * ========================================================================== */

uint16_t scheduler_next_task_core0(void) {
    if (core0_queue.count == 0) {
        return 0;
    }

    task_priority_t max_priority  = (task_priority_t)-1;
    uint16_t        selected_task = 0;

    for (uint16_t i = 0; i < core0_queue.count; i++) {
        uint16_t task_id = core0_queue.tasks[i];
        task_descriptor_t *task = find_task(task_id);

        if (task && (task->state == TASK_STATE_READY ||
                     task->state == TASK_STATE_RUNNING)) {
            if (task->priority > max_priority) {
                max_priority  = task->priority;
                selected_task = task_id;
            }
        }
    }

    if (selected_task) {
        current_task_id = selected_task;
    }

    return selected_task;
}

uint16_t scheduler_next_task_core1(void) {
    if (core1_queue.count == 0) {
        return 0;
    }

    task_priority_t max_priority  = (task_priority_t)-1;
    uint16_t        selected_task = 0;

    for (uint16_t i = 0; i < core1_queue.count; i++) {
        uint16_t task_id = core1_queue.tasks[i];
        task_descriptor_t *task = find_task(task_id);

        if (task && (task->state == TASK_STATE_READY ||
                     task->state == TASK_STATE_RUNNING)) {
            if (task->priority > max_priority) {
                max_priority  = task->priority;
                selected_task = task_id;
            }
        }
    }

    if (selected_task) {
        current_task_id = selected_task;
    }

    return selected_task;
}

void scheduler_update_runtime(uint16_t task_id, uint32_t elapsed_ms) {
    task_descriptor_t *task = find_task(task_id);
    if (task) {
        task->total_runtime_ms += elapsed_ms;
        task->context_switches++;
    }
}

uint16_t scheduler_count_ready_tasks(void) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < task_count; i++) {
        if (task_table[i].state == TASK_STATE_READY ||
            task_table[i].state == TASK_STATE_RUNNING) {
            count++;
        }
    }
    return count;
}
