/* cmd_syslog.c - Syslog shell command */
#include <stdio.h>
#include <string.h>
#include "syslog.h"

int cmd_syslog(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "show") == 0) {
        printf("=== System Log (boot #%lu, %d entries) ===\r\n",
               (unsigned long)syslog_get_boot_count(), syslog_get_count());
        syslog_print_all();
        return 0;
    }

    if (strcmp(argv[1], "clear") == 0) {
        syslog_clear();
        printf("Syslog cleared (boot count preserved)\r\n");
        return 0;
    }

    if (strcmp(argv[1], "write") == 0) {
        if (argc < 3) {
            printf("Usage: syslog write <message>\r\n");
            return 1;
        }
        /* Reassemble message from argv */
        char msg[SYSLOG_MAX_MSG_LEN];
        msg[0] = '\0';
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, argv[i], sizeof(msg) - strlen(msg) - 1);
        }
        syslog_write(SYSLOG_INFO, "%s", msg);
        printf("Logged: %s\r\n", msg);
        return 0;
    }

    if (strcmp(argv[1], "boot") == 0) {
        printf("Boot count: %lu\r\n", (unsigned long)syslog_get_boot_count());
        return 0;
    }

    printf("Usage: syslog <show|clear|write|boot>\r\n");
    return 1;
}
