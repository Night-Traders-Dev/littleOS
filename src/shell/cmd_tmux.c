/* cmd_tmux.c - Terminal multiplexer shell commands */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tmux.h"

int cmd_screen(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: screen <command> [args]\r\n");
        printf("Commands:\r\n");
        printf("  list              - List all windows\r\n");
        printf("  new [NAME]        - Create new window\r\n");
        printf("  kill INDEX        - Destroy window\r\n");
        printf("  switch INDEX      - Switch to window\r\n");
        printf("  next              - Next window\r\n");
        printf("  prev              - Previous window\r\n");
        printf("  rename INDEX NAME - Rename window\r\n");
        printf("  status            - Show status bar\r\n");
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        printf("=== Windows (%d/%d) ===\r\n", tmux_get_window_count(), TMUX_MAX_WINDOWS);
        int active = tmux_get_active();
        for (int i = 0; i < TMUX_MAX_WINDOWS; i++) {
            char buf[16];
            /* Get output to check if window is active */
            int n = tmux_get_output(i, buf, 1);
            if (n >= 0 || i == 0) {
                /* Window exists - try rename to get name (hacky but works) */
                tmux_get_output(i, buf, sizeof(buf));
                printf("  [%d] %s%s\r\n", i,
                    i == 0 ? "shell" : "window",
                    i == active ? " (active)" : "");
            }
        }
        /* Better approach: show status bar */
        char bar[128];
        tmux_status_bar(bar, sizeof(bar));
        printf("\r\n%s\r\n", bar);
        return 0;
    }

    if (strcmp(argv[1], "new") == 0) {
        const char *name = (argc >= 3) ? argv[2] : "window";
        int idx = tmux_create_window(name);
        if (idx >= 0)
            printf("Created window %d: %s\r\n", idx, name);
        else
            printf("Failed to create window (max %d)\r\n", TMUX_MAX_WINDOWS);
        return (idx >= 0) ? 0 : 1;
    }

    if (strcmp(argv[1], "kill") == 0) {
        if (argc < 3) {
            printf("Usage: screen kill <index>\r\n");
            return 1;
        }
        int idx = atoi(argv[2]);
        int rc = tmux_destroy_window(idx);
        switch (rc) {
            case 0:  printf("Window %d destroyed\r\n", idx); break;
            case -2: printf("Cannot destroy window 0 (shell)\r\n"); break;
            default: printf("Failed to destroy window %d\r\n", idx); break;
        }
        return rc;
    }

    if (strcmp(argv[1], "switch") == 0 ||
        (argv[1][0] >= '0' && argv[1][0] <= '9' && strlen(argv[1]) == 1)) {
        int idx;
        if (strcmp(argv[1], "switch") == 0) {
            if (argc < 3) {
                printf("Usage: screen switch <index>\r\n");
                return 1;
            }
            idx = atoi(argv[2]);
        } else {
            idx = argv[1][0] - '0';
        }
        int rc = tmux_switch(idx);
        if (rc < 0) printf("Window %d not found\r\n", idx);
        return rc;
    }

    if (strcmp(argv[1], "next") == 0) {
        return tmux_next();
    }

    if (strcmp(argv[1], "prev") == 0) {
        return tmux_prev();
    }

    if (strcmp(argv[1], "rename") == 0) {
        if (argc < 4) {
            printf("Usage: screen rename <index> <name>\r\n");
            return 1;
        }
        int rc = tmux_rename_window(atoi(argv[2]), argv[3]);
        if (rc == 0)
            printf("Renamed window %s to '%s'\r\n", argv[2], argv[3]);
        else
            printf("Failed to rename window\r\n");
        return rc;
    }

    if (strcmp(argv[1], "status") == 0) {
        char bar[128];
        tmux_status_bar(bar, sizeof(bar));
        printf("%s\r\n", bar);
        return 0;
    }

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
