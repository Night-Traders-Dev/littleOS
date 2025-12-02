// cmd_dmesg.c - Shell command for viewing kernel messages
#include <stdio.h>
#include <string.h>
#include "dmesg.h"

void cmd_dmesg(int argc, char** argv) {
    if (argc == 1) {
        // No arguments - show all messages
        dmesg_print_all();
        return;
    }
    
    // Handle options
    if (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--clear") == 0) {
        // Clear buffer
        dmesg_clear();
        printf("Kernel message buffer cleared\r\n");
        return;
    }
    
    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--level") == 0) {
        // Filter by log level
        if (argc < 3) {
            printf("Usage: dmesg --level <level>\r\n");
            printf("Levels: emerg, alert, crit, err, warn, notice, info, debug\r\n");
            return;
        }
        
        uint8_t level = DMESG_LEVEL_INFO;  // Default
        
        if (strcmp(argv[2], "emerg") == 0) level = DMESG_LEVEL_EMERG;
        else if (strcmp(argv[2], "alert") == 0) level = DMESG_LEVEL_ALERT;
        else if (strcmp(argv[2], "crit") == 0) level = DMESG_LEVEL_CRIT;
        else if (strcmp(argv[2], "err") == 0) level = DMESG_LEVEL_ERR;
        else if (strcmp(argv[2], "warn") == 0) level = DMESG_LEVEL_WARN;
        else if (strcmp(argv[2], "notice") == 0) level = DMESG_LEVEL_NOTICE;
        else if (strcmp(argv[2], "info") == 0) level = DMESG_LEVEL_INFO;
        else if (strcmp(argv[2], "debug") == 0) level = DMESG_LEVEL_DEBUG;
        else {
            printf("Unknown log level: %s\r\n", argv[2]);
            return;
        }
        
        dmesg_print_level(level);
        return;
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        printf("\r\nUsage: dmesg [options]\r\n");
        printf("\r\n");
        printf("View kernel ring buffer messages\r\n");
        printf("\r\n");
        printf("Options:\r\n");
        printf("  (no args)       Show all messages\r\n");
        printf("  -c, --clear     Clear message buffer\r\n");
        printf("  -l, --level <L> Show messages at level L and above\r\n");
        printf("  -h, --help      Show this help\r\n");
        printf("\r\n");
        printf("Log levels (most to least severe):\r\n");
        printf("  emerg   - System is unusable\r\n");
        printf("  alert   - Action required immediately\r\n");
        printf("  crit    - Critical conditions\r\n");
        printf("  err     - Error conditions\r\n");
        printf("  warn    - Warning conditions\r\n");
        printf("  notice  - Normal but significant\r\n");
        printf("  info    - Informational messages\r\n");
        printf("  debug   - Debug-level messages\r\n");
        printf("\r\n");
        printf("Examples:\r\n");
        printf("  dmesg              # Show all messages\r\n");
        printf("  dmesg -l err       # Show errors and above\r\n");
        printf("  dmesg --clear      # Clear buffer\r\n");
        printf("\r\n");
        return;
    }
    
    printf("Unknown option: %s\r\n", argv[1]);
    printf("Try 'dmesg --help' for usage information\r\n");
}
