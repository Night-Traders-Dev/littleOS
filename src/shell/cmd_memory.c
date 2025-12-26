#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "memory_segmented.h"

/*
 * Memory management commands for littleOS shell
 * Updated to work with new segmented memory API
 */

// Show memory statistics
int cmd_memory_stats(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    memory_print_stats();
    return 0;
}

// Show memory layout
int cmd_memory_layout(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    memory_print_layout();
    return 0;
}

// Show stack status
int cmd_memory_stack(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    memory_print_stack_status();
    return 0;
}

// Run comprehensive health check
int cmd_memory_health(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    memory_health_check();
    return 0;
}

// Test kernel allocations
int cmd_memory_test_kernel(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: memory test-kernel <size>\r\n");
        printf("Test kernel heap allocation\r\n");
        return 1;
    }
    
    uint32_t size = atoi(argv[1]);
    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }
    
    printf("Testing kernel malloc(%u bytes)...\r\n", size);
    void *ptr = kernel_malloc(size);
    
    if (!ptr) {
        printf("❌ Allocation failed - heap full?\r\n");
        return 1;
    }
    
    printf("✓ Allocated at %p\r\n", ptr);
    
    /* Write test pattern */
    memset(ptr, 0xAA, size);
    printf("✓ Wrote test pattern\r\n");
    
    /* Memory is persistent - no free in segmented allocator */
    printf("Note: Kernel heap allocations are permanent (bump allocator)\r\n");
    
    memory_print_stats();
    return 0;
}

// Test interpreter allocations and reset
int cmd_memory_test_interpreter(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: memory test-interp <size>\r\n");
        printf("Test interpreter heap allocation and reset\r\n");
        return 1;
    }
    
    uint32_t size = atoi(argv[1]);
    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }
    
    printf("Testing interpreter_malloc(%u bytes)...\r\n", size);
    void *ptr = interpreter_malloc(size);
    
    if (!ptr) {
        printf("❌ Allocation failed - heap full?\r\n");
        return 1;
    }
    
    printf("✓ Allocated at %p\r\n", ptr);
    memset(ptr, 0xBB, size);
    printf("✓ Wrote test pattern\r\n");
    
    printf("\nBefore reset:\r\n");
    MemoryStats stats = memory_get_stats();
    printf("Interpreter used: %zu bytes\r\n", stats.interpreter_used);
    
    printf("\nResetting interpreter heap...\r\n");
    interpreter_heap_reset();
    
    printf("After reset:\r\n");
    stats = memory_get_stats();
    printf("Interpreter used: %zu bytes\r\n", stats.interpreter_used);
    
    printf("✓ Reset successful - all allocations freed\r\n");
    return 0;
}

// Collision detection test
int cmd_memory_collision(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    printf("Checking for heap-stack collision...\r\n");
    
    if (memory_check_collision()) {
        printf("❌ CRITICAL: Heap-stack collision detected!\r\n");
        memory_health_check();
        return 1;
    }
    
    printf("✓ No collision detected\r\n");
    printf("Stack free space: %u bytes\r\n", stack_get_free_space());
    return 0;
}

// Validate layout
int cmd_memory_validate(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    printf("Validating memory layout...\r\n");
    
    if (!memory_validate_layout()) {
        printf("❌ Memory layout is invalid!\r\n");
        printf("Kernel and interpreter heaps may overlap\r\n");
        memory_print_layout();
        return 1;
    }
    
    printf("✓ Memory layout is valid\r\n");
    memory_print_layout();
    return 0;
}

// Show remaining interpreter heap
int cmd_memory_interp_remaining(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    size_t remaining = interpreter_heap_remaining();
    printf("Interpreter heap remaining: %zu bytes\r\n", remaining);
    
    if (remaining < 4096) {
        printf("⚠️  Warning: Low interpreter heap space\r\n");
    }
    
    return 0;
}

// Main memory command dispatcher
int cmd_memory(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: memory <subcommand> [args]\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  stats           - Show memory statistics\r\n");
        printf("  layout          - Show memory layout diagram\r\n");
        printf("  stack           - Show stack status\r\n");
        printf("  health          - Run comprehensive health check\r\n");
        printf("  validate        - Validate memory layout\r\n");
        printf("  collision       - Check for heap-stack collision\r\n");
        printf("  remaining       - Show interpreter heap remaining\r\n");
        printf("  test-kernel <sz> - Test kernel allocation\r\n");
        printf("  test-interp <sz> - Test interpreter allocation/reset\r\n");
        printf("  help            - Show this help\r\n");
        return 0;
    }
    
    if (strcmp(argv[1], "stats") == 0) {
        return cmd_memory_stats(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "layout") == 0) {
        return cmd_memory_layout(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "stack") == 0) {
        return cmd_memory_stack(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "health") == 0) {
        return cmd_memory_health(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "validate") == 0) {
        return cmd_memory_validate(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "collision") == 0) {
        return cmd_memory_collision(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "remaining") == 0) {
        return cmd_memory_interp_remaining(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "test-kernel") == 0) {
        return cmd_memory_test_kernel(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "test-interp") == 0) {
        return cmd_memory_test_interpreter(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        return cmd_memory(1, argv);
    } else {
        printf("Unknown subcommand: %s\r\n", argv[1]);
        printf("Type 'memory help' for usage\r\n");
        return 1;
    }
}