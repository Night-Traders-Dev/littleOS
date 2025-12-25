// src/shell/cmd_users.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "users_config.h"
#include "permissions.h"

/*
 * User management commands for littleOS shell
 */

// Helper to print capability flags
static void print_capabilities(uint32_t caps) {
    if (caps == CAP_ALL) {
        printf("ALL");
        return;
    }
    if (caps == 0) {
        printf("NONE");
        return;
    }
    
    bool first = true;
    if (caps & CAP_SYS_ADMIN) {
        printf("%sSYS_ADMIN", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_SYS_BOOT) {
        printf("%sSYS_BOOT", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_GPIO_WRITE) {
        printf("%sGPIO_WRITE", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_UART_CONFIG) {
        printf("%sUART_CONFIG", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_TASK_SPAWN) {
        printf("%sTASK_SPAWN", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_TASK_KILL) {
        printf("%sTASK_KILL", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_MEM_LOCK) {
        printf("%sMEM_LOCK", first ? "" : "|");
        first = false;
    }
    if (caps & CAP_NET_ADMIN) {
        printf("%sNET_ADMIN", first ? "" : "|");
        first = false;
    }
}

// List all users
int cmd_users_list(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    uint16_t user_count = users_get_count();
    
    printf("\r\nConfigured Users (%d total):\r\n", user_count);
    printf("================================\r\n");
    
    for (uint16_t i = 0; i < user_count; i++) {
        const user_account_t *user = users_get_by_index(i);
        if (!user) continue;
        
        printf("[%d] %s\r\n", i, user->username);
        printf("    UID:          %d\r\n", user->uid);
        printf("    GID:          %d\r\n", user->gid);
        printf("    Umask:        0%03o\r\n", user->umask);
        printf("    Capabilities: ");
        print_capabilities(user->capabilities);
        printf("\r\n");
        
        if (i < user_count - 1) {
            printf("\r\n");
        }
    }
    
    printf("================================\r\n");
    return 0;
}

// Get user by ID
int cmd_users_get(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: users get <uid|username>\r\n");
        return 1;
    }
    
    const user_account_t *user = NULL;
    
    // Try parsing as UID first
    int uid = atoi(argv[1]);
    user = users_get_by_uid(uid);
    
    // If not found, try as username
    if (!user) {
        user = users_get_by_name(argv[1]);
    }
    
    if (!user) {
        printf("User not found: %s\r\n", argv[1]);
        return 1;
    }
    
    printf("\r\nUser Info: %s\r\n", user->username);
    printf("================================\r\n");
    printf("UID:          %d\r\n", user->uid);
    printf("GID:          %d\r\n", user->gid);
    printf("Umask:        0%03o\r\n", user->umask);
    printf("Capabilities: ");
    print_capabilities(user->capabilities);
    printf("\r\n");
    printf("================================\r\n");
    
    return 0;
}

// Check if UID/username exists
int cmd_users_exists(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: users exists <uid|username>\r\n");
        return 1;
    }
    
    // Try as UID first
    int uid = atoi(argv[1]);
    if (users_uid_exists(uid)) {
        printf("UID %d exists\r\n", uid);
        return 0;
    }
    
    // Try as username
    if (users_name_exists(argv[1])) {
        printf("User '%s' exists\r\n", argv[1]);
        return 0;
    }
    
    printf("User not found: %s\r\n", argv[1]);
    return 1;
}

// Main users command dispatcher
int cmd_users(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: users <list|get|exists|help>\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  list              - Show all configured users\r\n");
        printf("  get <uid|name>    - Show details for specific user\r\n");
        printf("  exists <uid|name> - Check if user exists\r\n");
        printf("  help              - Show this help\r\n");
        return 0;
    }
    
    if (strcmp(argv[1], "list") == 0) {
        return cmd_users_list(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "get") == 0) {
        return cmd_users_get(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "exists") == 0) {
        return cmd_users_exists(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        return cmd_users(1, argv);
    } else {
        printf("Unknown subcommand: %s\r\n", argv[1]);
        return 1;
    }
}
