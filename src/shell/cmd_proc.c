/* cmd_proc.c - Shell commands for /proc virtual filesystem */

#include <stdio.h>
#include <string.h>

#include "procfs.h"

#define PROCFS_READ_BUF_SIZE 2048

static void cmd_proc_usage(void) {
    printf("/proc filesystem commands:\r\n");
    printf("  proc                - List /proc entries\r\n");
    printf("  proc list [PATH]    - List entries in directory\r\n");
    printf("  proc read PATH      - Read a virtual file\r\n");
    printf("  cat PATH            - Alias for proc read\r\n");
}

static int cmd_proc_list(const char *path) {
    static char buf[PROCFS_READ_BUF_SIZE];

    int n = procfs_list(path, buf, sizeof(buf));
    if (n < 0) {
        printf("procfs: cannot list '%s' (err %d)\r\n", path, n);
        return -1;
    }
    if (n == 0) {
        printf("(empty)\r\n");
        return 0;
    }
    printf("%s", buf);
    return 0;
}

static int cmd_proc_read(const char *path) {
    static char buf[PROCFS_READ_BUF_SIZE];

    int n = procfs_read(path, buf, sizeof(buf));
    if (n < 0) {
        printf("procfs: cannot read '%s' (err %d)\r\n", path, n);
        return -1;
    }
    if (n == 0) {
        printf("(empty)\r\n");
        return 0;
    }
    /* Ensure null termination for safety */
    if ((size_t)n < sizeof(buf))
        buf[n] = '\0';
    else
        buf[sizeof(buf) - 1] = '\0';

    printf("%s", buf);
    return 0;
}

int cmd_proc(int argc, char *argv[]) {
    /* No arguments: list /proc */
    if (argc < 2) {
        return cmd_proc_list("/proc");
    }

    const char *sub = argv[1];

    if (strcmp(sub, "help") == 0 || strcmp(sub, "--help") == 0) {
        cmd_proc_usage();
        return 0;
    }

    if (strcmp(sub, "list") == 0) {
        const char *path = (argc >= 3) ? argv[2] : "/proc";
        return cmd_proc_list(path);
    }

    if (strcmp(sub, "read") == 0) {
        if (argc < 3) {
            printf("Usage: proc read <path>\r\n");
            return -1;
        }
        return cmd_proc_read(argv[2]);
    }

    /* Treat unknown subcommand as a path to read (cat-style shortcut) */
    if (sub[0] == '/') {
        return cmd_proc_read(sub);
    }

    printf("Unknown subcommand: %s\r\n", sub);
    cmd_proc_usage();
    return -1;
}
