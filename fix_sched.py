import re

with open('src/drivers/scheduler.c', 'r') as f:
    content = f.read()

# Fix multiple SCHED_LOCK() by scoping them in braces
content = content.replace("SCHED_LOCK();\n        task->state = TASK_STATE_TERMINATED;", "{\n        SCHED_LOCK();\n        task->state = TASK_STATE_TERMINATED;")
content = content.replace("task_count--;\n        SCHED_UNLOCK();\n        printf", "task_count--;\n        SCHED_UNLOCK();\n        }\n        printf")

content = content.replace("SCHED_LOCK();\n    task->created_at_ms", "{\n    SCHED_LOCK();\n    task->created_at_ms")
content = content.replace("    uint16_t ret_id = task->task_id;\n    SCHED_UNLOCK();\n\n    printf", "    uint16_t ret_id = task->task_id;\n    SCHED_UNLOCK();\n    }\n\n    printf")

with open('src/drivers/scheduler.c', 'w') as f:
    f.write(content)
