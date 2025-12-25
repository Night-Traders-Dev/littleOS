// src/shell/cmd_perms.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "permissions.h"
#include "users_config.h"

/*
 * Permission and access control commands for littleOS shell
 */

// Helper function to create a security context for a given UID
static task_sec_ctx_t create_task_context(uid_t uid) {
    task_sec_ctx_t ctx = {
        .uid = uid,
        .euid = uid,
        .gid = GID_USERS,
        .egid = GID_USERS,
        .umask = 0022,
        .capabilities = 0
    };
    
    // Root gets all capabilities
    if (uid == UID_ROOT) {
        ctx.gid = GID_ROOT;
        ctx.egid = GID_ROOT;
        ctx.capabilities = CAP_ALL;
    }
    
    return ctx;
}

// Convert permission mode (e.g., "0644") to perm_bits_t
static perm_bits_t parse_mode(const char *mode_str) {
    if (!mode_str || strlen(mode_str) < 3) {
        return 0;
    }
    
    // Skip leading '0' if octal notation
    const char *p = mode_str;
    if (*p == '0') p++;
    
    if (strlen(p) < 3) {
        return 0;
    }
    
    int owner = p[0] - '0';
    int group = p[1] - '0';
    int other = p[2] - '0';
    
    return PERM_MAKE(owner, group, other);
}

// Convert perm_bits_t to string representation (e.g., "0644" or "rwxr-xr-x")
static void perm_to_string(perm_bits_t perms, char *buf, int buf_len) {
    uint8_t owner = PERM_GET_OWNER(perms);
    uint8_t group = PERM_GET_GROUP(perms);
    uint8_t other = PERM_GET_OTHER(perms);
    
    snprintf(buf, buf_len, "0%o%o%o", owner, group, other);
}

// Print permission in rwx format
static void print_perm_rwx(perm_bits_t perms) {
    uint8_t owner = PERM_GET_OWNER(perms);
    uint8_t group = PERM_GET_GROUP(perms);
    uint8_t other = PERM_GET_OTHER(perms);
    
    // Owner
    printf("%c%c%c",
        (owner & PERM_READ) ? 'r' : '-',
        (owner & PERM_WRITE) ? 'w' : '-',
        (owner & PERM_EXEC) ? 'x' : '-'
    );
    
    // Group
    printf("%c%c%c",
        (group & PERM_READ) ? 'r' : '-',
        (group & PERM_WRITE) ? 'w' : '-',
        (group & PERM_EXEC) ? 'x' : '-'
    );
    
    // Other
    printf("%c%c%c",
        (other & PERM_READ) ? 'r' : '-',
        (other & PERM_WRITE) ? 'w' : '-',
        (other & PERM_EXEC) ? 'x' : '-'
    );
}

// Check permission
int cmd_perms_check(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: perms check <uid> <mode> <action>\r\n");
        printf("action: read|write|exec\r\n");
        printf("Example: perms check 1000 0644 read\r\n");
        return 1;
    }
    
    uid_t uid = (uid_t)atoi(argv[1]);
    perm_bits_t perms = parse_mode(argv[2]);
    
    uint8_t required_perm = 0;
    if (strcmp(argv[3], "read") == 0) {
        required_perm = PERM_READ;
    } else if (strcmp(argv[3], "write") == 0) {
        required_perm = PERM_WRITE;
    } else if (strcmp(argv[3], "exec") == 0) {
        required_perm = PERM_EXEC;
    } else {
        printf("Unknown action: %s\r\n", argv[3]);
        return 1;
    }
    
    // Create task context for the given UID
    task_sec_ctx_t task_ctx = create_task_context(uid);
    
    // Create a test resource
    resource_perm_t resource = perm_resource_create(
        UID_ROOT,
        GID_DRIVERS,
        perms,
        RESOURCE_DEVICE
    );
    
    bool allowed = perm_check(&task_ctx, &resource, required_perm);
    
    char mode_buf[16];
    perm_to_string(perms, mode_buf, sizeof(mode_buf));
    
    printf("\r\nPermission Check:\r\n");
    printf("  UID:    %d\r\n", uid);
    printf("  Mode:   %s (", mode_buf);
    print_perm_rwx(perms);
    printf(")\r\n");
    printf("  Action: %s\r\n", argv[3]);
    printf("  Result: %s\r\n", allowed ? "ALLOWED" : "DENIED");
    printf("\r\n");
    
    return allowed ? 0 : 1;
}

// Decode mode
int cmd_perms_decode(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: perms decode <mode>\r\n");
        printf("Example: perms decode 0644\r\n");
        return 1;
    }
    
    perm_bits_t perms = parse_mode(argv[1]);
    
    uint8_t owner = PERM_GET_OWNER(perms);
    uint8_t group = PERM_GET_GROUP(perms);
    uint8_t other = PERM_GET_OTHER(perms);
    
    printf("\r\nPermission Mode: %s\r\n", argv[1]);
    printf("================================\r\n");
    printf("Octal:  0%o%o%o\r\n", owner, group, other);
    printf("Rwx:    ");
    print_perm_rwx(perms);
    printf("\r\n\r\n");
    
    printf("Owner: ");
    print_perm_rwx((owner << 6) | 0);
    printf(" (%d)\r\n", owner);
    
    printf("Group: ");
    print_perm_rwx((group << 3) | 0);
    printf(" (%d)\r\n", group);
    
    printf("Other: ");
    print_perm_rwx(other);
    printf(" (%d)\r\n", other);
    
    printf("================================\r\n");
    return 0;
}

// Show common permission presets
int cmd_perms_presets(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\r\nCommon Permission Presets:\r\n");
    printf("================================\r\n");
    printf("0644 (rw-r--r--) - Owner rw, group/other read\r\n");
    printf("0640 (rw-r-----) - Owner rw, group read\r\n");
    printf("0600 (rw-------) - Owner rw only\r\n");
    printf("0755 (rwxr-xr-x) - Owner rwx, group/other rx\r\n");
    printf("0700 (rwx------) - Owner rwx only\r\n");
    printf("0666 (rw-rw-rw-) - All can read/write\r\n");
    printf("0777 (rwxrwxrwx) - All have full access\r\n");
    printf("================================\r\n");
    return 0;
}

// Main perms command dispatcher
int cmd_perms(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: perms <check|decode|presets|help>\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  check <uid> <mode> <action> - Check if UID has permission\r\n");
        printf("  decode <mode>               - Decode permission mode\r\n");
        printf("  presets                     - Show common presets\r\n");
        printf("  help                        - Show this help\r\n");
        return 0;
    }
    
    if (strcmp(argv[1], "check") == 0) {
        return cmd_perms_check(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "decode") == 0) {
        return cmd_perms_decode(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "presets") == 0) {
        return cmd_perms_presets(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "help") == 0) {
        return cmd_perms(1, argv);
    } else {
        printf("Unknown subcommand: %s\r\n", argv[1]);
        return 1;
    }
}
