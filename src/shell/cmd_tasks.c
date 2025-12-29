// src/shell/cmd_tasks_enhanced.c
// Enhanced task management with module integration

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"

/*
 * Task management commands for littleOS shell
 * Enables creating and managing processes for modules
 */

/* ============================================================================
 * Simple Task Registry for Module Integration
 * ========================================================================== */

typedef struct {
    uint16_t task_id;
    char module_name[32];
    char task_name[LITTLEOS_MAX_TASK_NAME];
} task_registry_entry_t;

#define MAX_REGISTRY_ENTRIES 32
static task_registry_entry_t task_registry[MAX_REGISTRY_ENTRIES] = {0};
static uint16_t registry_count = 0;

// Register a task to a module
static void registry_add(uint16_t task_id, const char *module, const char *task_name) {
    if (registry_count >= MAX_REGISTRY_ENTRIES) {
        printf("ERROR: Task registry full\n");
        return;
    }
    
    task_registry_entry_t *entry = &task_registry[registry_count++];
    entry->task_id = task_id;
    strncpy(entry->module_name, module ? module : "system", 31);
    strncpy(entry->task_name, task_name ? task_name : "unnamed", LITTLEOS_MAX_TASK_NAME - 1);
}

// Find all tasks for a module
static int registry_find_module_tasks(const char *module, uint16_t *out, int max_out) {
    int count = 0;
    for (uint16_t i = 0; i < registry_count && count < max_out; i++) {
        if (strcmp(task_registry[i].module_name, module) == 0) {
            out[count++] = task_registry[i].task_id;
        }
    }
    return count;
}

// Remove task from registry
static void registry_remove(uint16_t task_id) {
    for (uint16_t i = 0; i < registry_count; i++) {
        if (task_registry[i].task_id == task_id) {
            // Shift remaining entries
            memmove(&task_registry[i], &task_registry[i + 1], 
                    (registry_count - i - 1) * sizeof(task_registry_entry_t));
            registry_count--;
            return;
        }
    }
}

/* ============================================================================
 * Shell Commands
 * ========================================================================== */

// List all tasks
int cmd_tasks_list(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    char buffer[2048];
    int written = task_list(buffer, sizeof(buffer));
    printf("%s", buffer);
    return 0;
}

// Get task details
int cmd_tasks_info(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks info <task_id>\n");
        return 1;
    }

    uint16_t task_id = (uint16_t)atoi(argv[1]);
    task_descriptor_t desc;
    if (!task_get_descriptor(task_id, &desc)) {
        printf("Task not found: %d\n", task_id);
        return 1;
    }

    const char *state_names[] = {
        "IDLE", "READY", "RUNNING", "BLOCKED", "SUSPENDED", "TERMINATED"
    };
    const char *state = (desc.state < 6) ? state_names[desc.state] : "?";
    const char *core_str = (desc.core_affinity == 0) ? "0" :
                           (desc.core_affinity == 1) ? "1" : "Any";

    printf("\n=== Task Information ===\n");
    printf("Name: %s\n", desc.name);
    printf("Task ID: %d\n", desc.task_id);
    printf("State: %s\n", state);
    printf("Priority: %d\n", desc.priority);
    printf("Core Affinity: %s\n", core_str);
    printf("UID: %d\n", desc.sec_ctx.uid);
    printf("GID: %d\n", desc.sec_ctx.gid);
    printf("Memory Used: %lu bytes\n", (unsigned long)desc.memory_allocated);
    printf("Memory Peak: %lu bytes\n", (unsigned long)desc.memory_peak);
    printf("Stack Size: %lu bytes\n", (unsigned long)desc.stack_size);
    printf("Runtime: %lu ms\n", (unsigned long)desc.total_runtime_ms);
    printf("Context Switches: %lu\n", (unsigned long)desc.context_switches);
    printf("Created: %lu ms\n", (unsigned long)desc.created_at_ms);
    printf("========================\n");
    return 0;
}

// Suspend a task
int cmd_tasks_suspend(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks suspend <task_id>\n");
        return 1;
    }

    uint16_t task_id = (uint16_t)atoi(argv[1]);
    if (task_suspend(task_id)) {
        printf("Suspended task %d\n", task_id);
        return 0;
    } else {
        printf("Failed to suspend task %d\n", task_id);
        return 1;
    }
}

// Resume a task
int cmd_tasks_resume(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks resume <task_id>\n");
        return 1;
    }

    uint16_t task_id = (uint16_t)atoi(argv[1]);
    if (task_resume(task_id)) {
        printf("Resumed task %d\n", task_id);
        return 0;
    } else {
        printf("Failed to resume task %d\n", task_id);
        return 1;
    }
}

// Kill a task
int cmd_tasks_kill(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks kill <task_id>\n");
        return 1;
    }

    uint16_t task_id = (uint16_t)atoi(argv[1]);
    if (task_terminate(task_id)) {
        registry_remove(task_id);  // Also remove from registry
        printf("Killed task %d\n", task_id);
        return 0;
    } else {
        printf("Failed to kill task %d\n", task_id);
        return 1;
    }
}

// Count tasks
int cmd_tasks_count(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    uint16_t count = task_get_count();
    printf("Active tasks: %d/%d\n", count, LITTLEOS_MAX_TASKS);
    return 0;
}

// Task statistics
int cmd_tasks_stats(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks stats <task_id>\n");
        return 1;
    }

    uint16_t task_id = (uint16_t)atoi(argv[1]);
    char buffer[1024];
    int written = task_get_stats(task_id, buffer, sizeof(buffer));
    if (written > 0) {
        printf("%s", buffer);
        return 0;
    } else {
        printf("Task not found: %d\n", task_id);
        return 1;
    }
}

// System-wide task status
int cmd_tasks_status(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n=== littleOS Task Status ===\n");
    printf("Total Tasks: %d / %d\n", task_get_count(), LITTLEOS_MAX_TASKS);
    printf("Current Task: %d\n", task_get_current());
    printf("Max Stack Size: %d bytes\n", LITTLEOS_TASK_STACK_SIZE);
    printf("\nRegistry Entries: %d\n", registry_count);
    
    if (registry_count > 0) {
        printf("\nModule → Tasks:\n");
        printf("─────────────────────────────────────\n");
        
        // Print by module
        const char *current_module = NULL;
        for (uint16_t i = 0; i < registry_count; i++) {
            if (current_module == NULL || strcmp(current_module, task_registry[i].module_name) != 0) {
                current_module = task_registry[i].module_name;
                printf("  %s:\n", current_module);
            }
            printf("    [%d] %s\n", task_registry[i].task_id, task_registry[i].task_name);
        }
    }
    
    printf("─────────────────────────────────────\n\n");
    return 0;
}

// Module task management
int cmd_tasks_module(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: tasks module <list|kill> <module_name>\n");
        return 1;
    }

    const char *action = argv[1];
    const char *module = argv[2];

    if (strcmp(action, "list") == 0) {
        uint16_t tasks[LITTLEOS_MAX_TASKS];
        int count = registry_find_module_tasks(module, tasks, LITTLEOS_MAX_TASKS);
        
        if (count == 0) {
            printf("No tasks found for module: %s\n", module);
            return 1;
        }
        
        printf("Tasks for module '%s':\n", module);
        for (int i = 0; i < count; i++) {
            task_descriptor_t desc;
            if (task_get_descriptor(tasks[i], &desc)) {
                printf("  [%d] %s\n", tasks[i], desc.name);
            }
        }
        return 0;
        
    } else if (strcmp(action, "kill") == 0) {
        uint16_t tasks[LITTLEOS_MAX_TASKS];
        int count = registry_find_module_tasks(module, tasks, LITTLEOS_MAX_TASKS);
        
        if (count == 0) {
            printf("No tasks found for module: %s\n", module);
            return 1;
        }
        
        printf("Killing %d task(s) from module '%s':\n", count, module);
        for (int i = 0; i < count; i++) {
            if (task_terminate(tasks[i])) {
                registry_remove(tasks[i]);
                printf("  ✓ Killed task %d\n", tasks[i]);
            }
        }
        return 0;
        
    } else {
        printf("Unknown action: %s\n", action);
        return 1;
    }
}

// Create test task (for demo/testing)
// This is a simple infinite loop task for testing scheduler
void test_task_entry(void *arg) {
    int id = (intptr_t)arg;
    printf("[Task %d] Started on Core 0\n", id);
    
    // Just yield
    for (int i = 0; i < 100; i++) {
        // Simulate work
        volatile int x = i * i;
        (void)x;
    }
    
    printf("[Task %d] Completed\n", id);
}

int cmd_tasks_test(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks test <count>\n");
        return 1;
    }

    int count = atoi(argv[1]);
    if (count <= 0 || count > LITTLEOS_MAX_TASKS - task_get_count()) {
        printf("Invalid count: %d (max %d available)\n", count, 
               LITTLEOS_MAX_TASKS - task_get_count());
        return 1;
    }

    printf("Creating %d test tasks...\n", count);
    for (int i = 0; i < count; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "test_%d", i);
        
        uint16_t task_id = task_create(
            task_name,
            test_task_entry,
            (void*)(intptr_t)i,
            TASK_PRIORITY_NORMAL,
            i % 2,  // Alternate cores
            UID_ROOT
        );
        
        if (task_id != 0xFFFF) {
            registry_add(task_id, "test", task_name);
            printf("  Created: [%d] %s\n", task_id, task_name);
        }
    }
    return 0;
}

// Usage info
int cmd_tasks_help(void) {
    printf("\nTask Management Commands:\n");
    printf("─────────────────────────────────────────────────────────────\n");
    printf("  tasks list              - List all tasks\n");
    printf("  tasks info <id>         - Show task details\n");
    printf("  tasks status            - System-wide task status\n");
    printf("  tasks count             - Show task count\n");
    printf("  tasks suspend <id>      - Suspend a task\n");
    printf("  tasks resume <id>       - Resume a task\n");
    printf("  tasks kill <id>         - Terminate a task\n");
    printf("  tasks stats <id>        - Show task statistics\n");
    printf("  tasks module list <mod> - List tasks for module\n");
    printf("  tasks module kill <mod> - Kill all tasks for module\n");
    printf("  tasks test <count>      - Create test tasks\n");
    printf("  tasks help              - Show this help\n");
    printf("─────────────────────────────────────────────────────────────\n\n");
    return 0;
}

/* ============================================================================
 * Main Command Dispatcher
 * ========================================================================== */

int cmd_tasks(int argc, char *argv[]) {
    if (argc < 2) {
        return cmd_tasks_help();
    }

    const char *subcommand = argv[1];

    if (strcmp(subcommand, "list") == 0) {
        return cmd_tasks_list(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "info") == 0) {
        return cmd_tasks_info(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "suspend") == 0) {
        return cmd_tasks_suspend(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "resume") == 0) {
        return cmd_tasks_resume(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "kill") == 0) {
        return cmd_tasks_kill(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "count") == 0) {
        return cmd_tasks_count(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "stats") == 0) {
        return cmd_tasks_stats(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "status") == 0) {
        return cmd_tasks_status(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "module") == 0) {
        return cmd_tasks_module(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "test") == 0) {
        return cmd_tasks_test(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "help") == 0) {
        return cmd_tasks_help();
    } else {
        printf("Unknown subcommand: %s\n", subcommand);
        return cmd_tasks_help();
    }
}