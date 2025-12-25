#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "memory.h"

/*
 * Memory management commands for littleOS shell
 */

// Show memory statistics
int cmd_memory_stats(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    memory_stats_t stats;
    if (!memory_get_stats(&stats)) {
        printf("Failed to get memory statistics\r\n");
        return 1;
    }
    
    uint32_t available = memory_available();
    uint32_t usage_percent = (stats.current_usage * 100) / LITTLEOS_HEAP_SIZE;
    
    printf("\r\n=== Memory Statistics ===\r\n");
    printf("Total Heap:      %d bytes\r\n", LITTLEOS_HEAP_SIZE);
    printf("Current Usage:   %lu bytes (%.1f%%)\r\n",
           stats.current_usage,
           (float)stats.current_usage / LITTLEOS_HEAP_SIZE * 100);
    printf("Available:       %lu bytes (%.1f%%)\r\n",
           available,
           (float)available / LITTLEOS_HEAP_SIZE * 100);
    printf("Peak Usage:      %lu bytes (%.1f%%)\r\n",
           stats.peak_usage,
           (float)stats.peak_usage / LITTLEOS_HEAP_SIZE * 100);
    printf("\r\nAllocation Stats:\r\n");
    printf("Total Allocated: %lu bytes\r\n", stats.total_allocated);
    printf("Total Freed:     %lu bytes\r\n", stats.total_freed);
    printf("Num Allocations: %lu\r\n", stats.num_allocations);
    printf("Num Frees:       %lu\r\n", stats.num_frees);
    printf("Fragmentation:   %lu%%\r\n", stats.fragmentation_ratio);
    printf("========================\r\n");
    
    /* Visual bar */
    printf("\nHeap Usage: [");
    int bar_width = 40;
    int filled = (usage_percent * bar_width) / 100;
    for (int i = 0; i < bar_width; i++) {
        printf("%c", i < filled ? '=' : ' ');
    }
    printf("] %u%%\r\n", usage_percent);
    
    return 0;
}

// Check for memory leaks
int cmd_memory_leaks(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Checking for memory leaks...\r\n");
    uint32_t leaks = memory_check_leaks();
    
    if (leaks == 0) {
        printf("No suspected leaks found\r\n");
    } else {
        printf("Found %lu suspected memory leaks\r\n", leaks);
    }
    
    return leaks > 0 ? 1 : 0;
}

// Test allocation/deallocation
int cmd_memory_test(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: memory test <size> <count>\r\n");
        printf("Allocate and free memory for testing\r\n");
        printf("Example: memory test 256 10  (allocate 256 bytes, 10 times)\r\n");
        return 1;
    }
    
    uint32_t size = atoi(argv[1]);
    uint32_t count = atoi(argv[2]);
    
    if (size == 0 || count == 0) {
        printf("Invalid parameters\r\n");
        return 1;
    }
    
    printf("Test: Allocating %lu blocks of %lu bytes each\r\n", count, size);
    
    void **blocks = (void **)malloc(count * sizeof(void *));
    if (!blocks) {
        printf("Failed to allocate test block array\r\n");
        return 1;
    }
    
    /* Allocate */
    uint32_t successful = 0;
    for (uint32_t i = 0; i < count; i++) {
        blocks[i] = memory_alloc(size);
        if (blocks[i]) {
            successful++;
            memset(blocks[i], 0xAA, size);  /* Write test pattern */
        }
    }
    
    printf("  Allocated: %lu/%lu blocks\r\n", successful, count);
    
    memory_stats_t stats;
    memory_get_stats(&stats);
    printf("  Memory used: %lu bytes\r\n", stats.current_usage);
    
    /* Free */
    printf("Freeing allocated blocks...\r\n");
    for (uint32_t i = 0; i < successful; i++) {
        memory_free(blocks[i]);
    }
    
    memory_get_stats(&stats);
    printf("  Memory used after free: %lu bytes\r\n", stats.current_usage);
    
    free(blocks);
    
    printf("Test complete\r\n");
    return 0;
}

// Defragment memory
int cmd_memory_defrag(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Defragmenting memory...\r\n");
    uint32_t improvement = memory_defragment();
    
    printf("Fragmentation improvement: %lu%%\r\n", improvement);
    
    return 0;
}

// Show available memory
int cmd_memory_available(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    uint32_t available = memory_available();
    uint32_t usage = memory_usage();
    
    printf("Memory Status:\r\n");
    printf("  Available: %lu bytes\r\n", available);
    printf("  In Use:    %lu bytes\r\n", usage);
    printf("  Total:     %d bytes\r\n", LITTLEOS_HEAP_SIZE);
    
    return 0;
}

// Set warning threshold
int cmd_memory_threshold(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: memory threshold <percent>\r\n");
        printf("Set memory usage warning threshold (0-100)\r\n");
        return 1;
    }
    
    int percent = atoi(argv[1]);
    
    if (percent < 0 || percent > 100) {
        printf("Invalid threshold: %d (must be 0-100)\r\n", percent);
        return 1;
    }
    
    memory_set_warning_threshold((uint8_t)percent);
    printf("Memory warning threshold set to %d%%\r\n", percent);
    
    return 0;
}

// Calloc test
int cmd_memory_calloc(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: memory calloc <size>\r\n");
        printf("Test zero-initialized allocation\r\n");
        return 1;
    }
    
    uint32_t size = atoi(argv[1]);
    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }
    
    printf("Allocating and zero-initializing %lu bytes...\r\n", size);
    
    void *ptr = memory_calloc(1, size);
    if (!ptr) {
        printf("Allocation failed\r\n");
        return 1;
    }
    
    printf("Success: allocated at %p\r\n", ptr);
    
    /* Verify zeroed */
    uint8_t *data = (uint8_t *)ptr;
    bool zeroed = true;
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] != 0) {
            zeroed = false;
            break;
        }
    }
    
    printf("Verification: %s\r\n", zeroed ? "All zeros ✓" : "NOT zero ✗");
    
    memory_free(ptr);
    printf("Freed\r\n");
    
    return zeroed ? 0 : 1;
}

// Main memory command dispatcher
int cmd_memory(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: memory <stats|leaks|test|defrag|available|threshold|calloc|help>\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  stats              - Show memory statistics\r\n");
        printf("  available          - Show available memory\r\n");
        printf("  leaks              - Check for memory leaks\r\n");
        printf("  test <sz> <cnt>    - Test allocations\r\n");
        printf("  defrag             - Defragment memory\r\n");
        printf("  threshold <%%>      - Set warning threshold\r\n");
        printf("  calloc <size>      - Test zero-initialized allocation\r\n");
        printf("  help               - Show this help\r\n");
        return 0;
    }
    
    if (strcmp(argv[1], "stats") == 0) {
        return cmd_memory_stats(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "available") == 0) {
        return cmd_memory_available(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "leaks") == 0) {
        return cmd_memory_leaks(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "test") == 0) {
        return cmd_memory_test(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "defrag") == 0) {
        return cmd_memory_defrag(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "threshold") == 0) {
        return cmd_memory_threshold(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "calloc") == 0) {
        return cmd_memory_calloc(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        return cmd_memory(1, argv);
    } else {
        printf("Unknown subcommand: %s\r\n", argv[1]);
        return 1;
    }
}