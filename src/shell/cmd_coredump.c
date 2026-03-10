/* cmd_coredump.c - Coredump shell command */
#include <stdio.h>
#include <string.h>
#include "coredump.h"

int cmd_coredump(int argc, char *argv[]) {
    if (argc < 2) {
        if (coredump_exists()) {
            printf("Coredump available. Use 'coredump show' to view.\r\n");
        } else {
            printf("No coredump stored.\r\n");
        }
        printf("\r\nUsage: coredump <show|clear|test>\r\n");
        return 0;
    }

    if (strcmp(argv[1], "show") == 0) {
        coredump_t dump;
        if (coredump_load(&dump)) {
            coredump_print(&dump);
        } else {
            printf("No valid coredump found.\r\n");
        }
        return 0;
    }

    if (strcmp(argv[1], "clear") == 0) {
        coredump_clear();
        printf("Coredump cleared.\r\n");
        return 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        printf("WARNING: This will trigger a panic and reboot!\r\n");
        printf("Saving test coredump...\r\n");
        coredump_panic("test panic from shell");
        return 0; /* Won't reach here on real hardware */
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
