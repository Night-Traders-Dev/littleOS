#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scheduler.h"

/*
 * Task management commands for littleOS shell
 */

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
        printf("Usage: tasks info <task_id>\r\n");
        return 1;
    }
    
    uint16_t task_id = (uint16_t)atoi(argv[1]);
    
    task_descriptor_t desc;
    if (!task_get_descriptor(task_id, &desc)) {
        printf("Task not found: %d\r\n", task_id);
        return 1;
    }
    
    const char *state_names[] = {
        "IDLE", "READY", "RUNNING", "BLOCKED", "SUSPENDED", "TERMINATED"
    };
    const char *state = (desc.state < 6) ? state_names[desc.state] : "?";
    
    printf("\r\n=== Task Information ===\r\n");
    printf("Name:             %s\r\n", desc.name);
    printf("Task ID:          %d\r\n", desc.task_id);
    printf("State:            %s\r\n", state);
    printf("Priority:         %d\r\n", desc.priority);
    printf("Core Affinity:    %d\r\n", desc.core_affinity);
    printf("UID:              %d\r\n", desc.sec_ctx.uid);
    printf("GID:              %d\r\n", desc.sec_ctx.gid);
    printf("Memory Used:      %lu bytes\r\n", desc.memory_allocated);
    printf("Memory Peak:      %lu bytes\r\n", desc.memory_peak);
    printf("Stack Size:       %lu bytes\r\n", desc.stack_size);
    printf("Runtime:          %lu ms\r\n", desc.total_runtime_ms);
    printf("Context Switches: %lu\r\n", desc.context_switches);
    printf("========================\r\n");
    
    return 0;
}

// Suspend a task
int cmd_tasks_suspend(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks suspend <task_id>\r\n");
        return 1;
    }
    
    uint16_t task_id = (uint16_t)atoi(argv[1]);
    
    if (task_suspend(task_id)) {
        printf("Suspended task %d\r\n", task_id);
        return 0;
    } else {
        printf("Failed to suspend task %d\r\n", task_id);
        return 1;
    }
}

// Resume a task
int cmd_tasks_resume(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks resume <task_id>\r\n");
        return 1;
    }
    
    uint16_t task_id = (uint16_t)atoi(argv[1]);
    
    if (task_resume(task_id)) {
        printf("Resumed task %d\r\n", task_id);
        return 0;
    } else {
        printf("Failed to resume task %d\r\n", task_id);
        return 1;
    }
}

// Kill a task
int cmd_tasks_kill(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks kill <task_id>\r\n");
        return 1;
    }
    
    uint16_t task_id = (uint16_t)atoi(argv[1]);
    
    if (task_terminate(task_id)) {
        printf("Killed task %d\r\n", task_id);
        return 0;
    } else {
        printf("Failed to kill task %d\r\n", task_id);
        return 1;
    }
}

// Count tasks
int cmd_tasks_count(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    uint16_t count = task_get_count();
    printf("Active tasks: %d/%d\r\n", count, LITTLEOS_MAX_TASKS);
    
    return 0;
}

// Task statistics
int cmd_tasks_stats(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks stats <task_id>\r\n");
        return 1;
    }
    
    uint16_t task_id = (uint16_t)atoi(argv[1]);
    
    char buffer[1024];
    int written = task_get_stats(task_id, buffer, sizeof(buffer));
    
    if (written > 0) {
        printf("%s", buffer);
        return 0;
    } else {
        printf("Task not found: %d\r\n", task_id);
        return 1;
    }
}

// Create a task (for testing)
int cmd_tasks_create(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: tasks create <name> <priority> <core>\r\n");
        printf("priority: 0=low, 1=normal, 2=high, 3=critical\r\n");
        printf("core: 0=core0, 1=core1, 2=any\r\n");
        return 1;
    }
    
    const char *name = argv[1];
    int priority = atoi(argv[2]);
    int core = atoi(argv[3]);
    
    if (priority < 0 || priority > 3) {
        printf("Invalid priority: %d\r\n", priority);
        return 1;
    }
    
    if (core < 0 || core > 2) {
        printf("Invalid core: %d\r\n", core);
        return 1;
    }
    
    /* Would need actual task entry function */
    printf("Task creation not fully implemented yet\r\n");
    printf("Name: %s, Priority: %d, Core: %d\r\n", name, priority, core);
    
    return 0;
}

// Main tasks command dispatcher
int cmd_tasks(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: tasks <list|info|suspend|resume|kill|count|stats|help>\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  list                   - Show all tasks\r\n");
        printf("  info <task_id>         - Show task details\r\n");
        printf("  suspend <task_id>      - Suspend a task\r\n");
        printf("  resume <task_id>       - Resume a task\r\n");
        printf("  kill <task_id>         - Terminate a task\r\n");
        printf("  count                  - Show task count\r\n");
        printf("  stats <task_id>        - Show task statistics\r\n");
        printf("  create <name> <p> <c> - Create a task (testing)\r\n");
        printf("  help                   - Show this help\r\n");
        return 0;
    }
    
    if (strcmp(argv[1], "list") == 0) {
        return cmd_tasks_list(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "info") == 0) {
        return cmd_tasks_info(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "suspend") == 0) {
        return cmd_tasks_suspend(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "resume") == 0) {
        return cmd_tasks_resume(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "kill") == 0) {
        return cmd_tasks_kill(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "count") == 0) {
        return cmd_tasks_count(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "stats") == 0) {
        return cmd_tasks_stats(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "create") == 0) {
        return cmd_tasks_create(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        return cmd_tasks(1, argv);
    } else {
        printf("Unknown subcommand: %s\r\n", argv[1]);
        return 1;
    }
}