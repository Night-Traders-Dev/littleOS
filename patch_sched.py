import re

with open('src/drivers/scheduler.c', 'r') as f:
    content = f.read()

# 1. Add hardware/sync.h and lock definition
header_old = """#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/scb.h"
#endif"""

header_new = """#ifdef PICO_BUILD
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
"""
content = content.replace(header_old, header_new)

# 2. scheduler_init
init_old = """void scheduler_init(void) {
    if (scheduler_initialized) {
        return;
    }"""
init_new = """void scheduler_init(void) {
    if (scheduler_initialized) {
        return;
    }
#ifdef PICO_BUILD
    sched_lock = spin_lock_instance(spin_lock_claim_unused(true));
#endif"""
content = content.replace(init_old, init_new)

# 3. alloc_task_id
alloc_task_id_old = """static uint16_t alloc_task_id(void) {
    static uint16_t next_id = 1;

    if (task_count >= LITTLEOS_MAX_TASKS - 1) {
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
}"""

alloc_task_id_new = """static uint16_t alloc_task_id(void) {
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
}"""
content = content.replace(alloc_task_id_old, alloc_task_id_new)

# 4. find_task
find_task_old = """static task_descriptor_t *find_task(uint16_t task_id) {
    for (uint16_t i = 0; i < task_count; i++) {
        if (task_table[i].task_id == task_id) {
            return &task_table[i];
        }
    }
    return NULL;
}"""
find_task_new = """static task_descriptor_t *find_task(uint16_t task_id) {
    if (task_id == 0) return NULL;
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state != TASK_STATE_IDLE && task_table[i].state != TASK_STATE_TERMINATED && task_table[i].task_id == task_id) {
            return &task_table[i];
        }
    }
    return NULL;
}"""
content = content.replace(find_task_old, find_task_new)

# 5. find_task_index
find_task_index_old = """static int find_task_index(uint16_t task_id) {
    for (uint16_t i = 0; i < task_count; i++) {
        if (task_table[i].task_id == task_id) {
            return (int)i;
        }
    }
    return -1;
}"""
find_task_index_new = """static int find_task_index(uint16_t task_id) {
    if (task_id == 0) return -1;
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state != TASK_STATE_IDLE && task_table[i].state != TASK_STATE_TERMINATED && task_table[i].task_id == task_id) {
            return (int)i;
        }
    }
    return -1;
}"""
content = content.replace(find_task_index_old, find_task_index_new)

# 6. task_create (needs locking and empty slot search)
task_create_old = """uint16_t task_create(const char *name, task_entry_t entry, void *arg,
                     task_priority_t priority, uint8_t core, uid_t uid) {
    if (!scheduler_initialized) {
        printf("ERROR: Scheduler not initialized\\r\\n");
        return 0xFFFF;
    }

    if (task_count >= LITTLEOS_MAX_TASKS) {
        printf("ERROR: Task table full\\r\\n");
        return 0xFFFF;
    }

    if (!entry) {
        printf("ERROR: Invalid entry function\\r\\n");
        return 0xFFFF;
    }

    task_descriptor_t *task = &task_table[task_count];"""

task_create_new = """uint16_t task_create(const char *name, task_entry_t entry, void *arg,
                     task_priority_t priority, uint8_t core, uid_t uid) {
    if (!scheduler_initialized) {
        printf("ERROR: Scheduler not initialized\\r\\n");
        return 0xFFFF;
    }

    if (!entry) {
        printf("ERROR: Invalid entry function\\r\\n");
        return 0xFFFF;
    }

    SCHED_LOCK();
    if (task_count >= LITTLEOS_MAX_TASKS) {
        SCHED_UNLOCK();
        printf("ERROR: Task table full\\r\\n");
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
        printf("ERROR: Task table full\\r\\n");
        return 0xFFFF;
    }"""
content = content.replace(task_create_old, task_create_new)

task_create_mid_old = """    task->sec_ctx.capabilities = CAP_ALL;
    }

    task->stack_base = (uint32_t)malloc(LITTLEOS_TASK_STACK_SIZE);
    task->stack_size = LITTLEOS_TASK_STACK_SIZE;
    if (!task->stack_base) {
        printf("ERROR: Failed to allocate task stack\\r\\n");
        return 0xFFFF;
    }

    task->created_at_ms      = get_timestamp_ms();"""

task_create_mid_new = """    task->sec_ctx.capabilities = CAP_ALL;
    }
    
    // Temporarily mark as blocked to reserve slot while mallocing
    task->state = TASK_STATE_BLOCKED;
    task_count++;
    SCHED_UNLOCK();

    task->stack_base = (uint32_t)malloc(LITTLEOS_TASK_STACK_SIZE);
    task->stack_size = LITTLEOS_TASK_STACK_SIZE;
    if (!task->stack_base) {
        SCHED_LOCK();
        task->state = TASK_STATE_TERMINATED;
        task_count--;
        SCHED_UNLOCK();
        printf("ERROR: Failed to allocate task stack\\r\\n");
        return 0xFFFF;
    }

    SCHED_LOCK();
    task->created_at_ms      = get_timestamp_ms();"""
content = content.replace(task_create_mid_old, task_create_mid_new)

task_create_cfs_loop_old = """        for (uint16_t vi = 0; vi < task_count; vi++) {
            if (task_table[vi].state == TASK_STATE_READY ||
                task_table[vi].state == TASK_STATE_RUNNING) {
                if (task_table[vi].vruntime < min_vrt) {
                    min_vrt = task_table[vi].vruntime;
                }
            }
        }"""
task_create_cfs_loop_new = """        for (uint16_t vi = 0; vi < LITTLEOS_MAX_TASKS; vi++) {
            if (task_table[vi].state == TASK_STATE_READY ||
                task_table[vi].state == TASK_STATE_RUNNING) {
                if (task_table[vi].vruntime < min_vrt) {
                    min_vrt = task_table[vi].vruntime;
                }
            }
        }"""
content = content.replace(task_create_cfs_loop_old, task_create_cfs_loop_new)

task_create_end_old = """    task_count++;

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

    printf("Created task: %s (ID=%d, uid=%d, priority=%d)\\r\\n",
           task->name, task->task_id, uid, priority);

    return task->task_id;
}"""
task_create_end_new = """    task->state = TASK_STATE_READY;

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
    uint16_t ret_id = task->task_id;
    SCHED_UNLOCK();

    printf("Created task: %s (ID=%d, uid=%d, priority=%d)\\r\\n",
           task->name, ret_id, uid, priority);

    return ret_id;
}"""
content = content.replace(task_create_end_old, task_create_end_new)

# 7. task_terminate (remove shift, add locks)
task_terminate_old = """bool task_terminate(uint16_t task_id) {
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

    /* Invalidate cached pointer if this was the running task */
    if (running_task_ptr == task) {
        running_task_ptr = NULL;
    }

    if (idx < (int)task_count - 1) {
        memcpy(task, &task_table[task_count - 1], sizeof(task_descriptor_t));
    }

    task_count--;

    printf("Terminated task: %s (ID=%d)\\r\\n", task->name, task_id);
    return true;
}"""
task_terminate_new = """bool task_terminate(uint16_t task_id) {
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

    printf("Terminated task: %s (ID=%d)\\r\\n", task->name, task_id);
    return true;
}"""
content = content.replace(task_terminate_old, task_terminate_new)


# 8. Wrappers for simple functions
def wrap_func(func_name, code, ret_type='bool'):
    start_idx = code.find(func_name + '(')
    if start_idx == -1: return code
    open_brace = code.find('{', start_idx)
    close_brace = open_brace
    brace_count = 1
    i = open_brace + 1
    while brace_count > 0 and i < len(code):
        if code[i] == '{': brace_count += 1
        elif code[i] == '}': brace_count -= 1
        if brace_count == 0:
            close_brace = i
            break
        i += 1
    
    func_body = code[open_brace+1:close_brace]
    patched_body = "\n    SCHED_LOCK();\n" + func_body
    
    if ret_type == 'bool' or ret_type == 'int' or ret_type == 'uint16_t':
        patched_body = re.sub(r'return\s+(.+?);', r'SCHED_RETURN(\1);', patched_body)
    elif ret_type == 'void':
        patched_body = re.sub(r'return;', r'SCHED_RETURN_VOID();', patched_body)
    else:
        patched_body = re.sub(r'return\s+(.+?);', r'SCHED_RETURN_PTR(\1);', patched_body)
        
    return code[:open_brace+1] + patched_body + "\n    SCHED_UNLOCK();\n" + code[close_brace:]

# Apply wrappers to straightforward functions
content = wrap_func('task_get_descriptor', content, 'bool')
content = wrap_func('task_suspend', content, 'bool')
content = wrap_func('task_resume', content, 'bool')
content = wrap_func('task_report_memory', content, 'void')
content = wrap_func('scheduler_set_timeslice', content, 'void')
content = wrap_func('scheduler_update_runtime', content, 'void')


# 9. other task_count loops
task_list_loop_old = """    for (uint16_t i = 0; i < task_count; i++) {
        task_descriptor_t *task = &task_table[i];
        const char *state = (task->state < 6) ? state_names[task->state] : "?";"""
task_list_loop_new = """    SCHED_LOCK();
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        task_descriptor_t *task = &task_table[i];
        if (task->state == TASK_STATE_IDLE || task->state == TASK_STATE_TERMINATED) continue;
        const char *state = (task->state < 6) ? state_names[task->state] : "?";"""
content = content.replace(task_list_loop_old, task_list_loop_new)

task_list_end_old = """    written += snprintf(buffer + written, buffer_size - written,
                        "==================================================================\\r\\n");

    return written;
}"""
task_list_end_new = """    written += snprintf(buffer + written, buffer_size - written,
                        "==================================================================\\r\\n");
    SCHED_UNLOCK();
    return written;
}"""
content = content.replace(task_list_end_old, task_list_end_new)


task_get_stats_start_old = """int task_get_stats(uint16_t task_id, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        return snprintf(buffer, size, "Task not found\\r\\n");
    }"""
task_get_stats_start_new = """int task_get_stats(uint16_t task_id, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return 0;
    }

    SCHED_LOCK();
    task_descriptor_t *task = find_task(task_id);
    if (!task) {
        SCHED_UNLOCK();
        return snprintf(buffer, size, "Task not found\\r\\n");
    }"""
content = content.replace(task_get_stats_start_old, task_get_stats_start_new)

task_get_stats_end_old = """        (unsigned long)task->context_switches,
        (unsigned long)task->vruntime);
}"""
task_get_stats_end_new = """        (unsigned long)task->context_switches,
        (unsigned long)task->vruntime);
    SCHED_UNLOCK();
    return __ret;
}"""
content = content.replace(task_get_stats_end_old, task_get_stats_end_new)
content = content.replace("return snprintf(buffer, size,\n", "int __ret = snprintf(buffer, size,\n")

# scheduler_set_policy
scheduler_set_policy_old = """bool scheduler_set_policy(sched_policy_t policy) {
    if (policy >= SCHED_POLICY_COUNT) {
        return false;
    }
    active_policy = policy;

    /* Adjust time slices and reset CFS state for new policy */
    for (uint16_t i = 0; i < task_count; i++) {
        task_descriptor_t *task = &task_table[i];"""
scheduler_set_policy_new = """bool scheduler_set_policy(sched_policy_t policy) {
    if (policy >= SCHED_POLICY_COUNT) {
        return false;
    }
    SCHED_LOCK();
    active_policy = policy;

    /* Adjust time slices and reset CFS state for new policy */
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        task_descriptor_t *task = &task_table[i];
        if (task->state == TASK_STATE_IDLE || task->state == TASK_STATE_TERMINATED) continue;"""
content = content.replace(scheduler_set_policy_old, scheduler_set_policy_new)

scheduler_set_policy_end_old = """    printf("Scheduler policy: %s\\r\\n", policy_names[policy]);
    return true;
}"""
scheduler_set_policy_end_new = """    SCHED_UNLOCK();
    printf("Scheduler policy: %s\\r\\n", policy_names[policy]);
    return true;
}"""
content = content.replace(scheduler_set_policy_end_old, scheduler_set_policy_end_new)

# scheduler_count_ready_tasks
count_ready_old = """uint16_t scheduler_count_ready_tasks(void) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < task_count; i++) {
        if (task_table[i].state == TASK_STATE_READY ||"""
count_ready_new = """uint16_t scheduler_count_ready_tasks(void) {
    uint16_t count = 0;
    SCHED_LOCK();
    for (uint16_t i = 0; i < LITTLEOS_MAX_TASKS; i++) {
        if (task_table[i].state == TASK_STATE_READY ||"""
content = content.replace(count_ready_old, count_ready_new)

count_ready_end_old = """        }
    }
    return count;
}"""
count_ready_end_new = """        }
    }
    SCHED_UNLOCK();
    return count;
}"""
content = content.replace(count_ready_end_old, count_ready_end_new)

# ISR functions wrapper
content = wrap_func('scheduler_tick', content, 'void')
content = wrap_func('scheduler_context_switch', content, 'void')

with open('src/drivers/scheduler.c', 'w') as f:
    f.write(content)

print("Patching scheduler complete.")