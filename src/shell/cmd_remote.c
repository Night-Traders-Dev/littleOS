/* cmd_remote.c - Remote shell management commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "remote_shell.h"

static void cmd_remote_usage(void) {
    printf("Remote shell commands:\r\n");
    printf("  remote status          - Show listener state and connected clients\r\n");
    printf("  remote start [port]    - Start listener (default port %d)\r\n",
           REMOTE_SHELL_DEFAULT_PORT);
    printf("  remote stop            - Stop listener and disconnect all clients\r\n");
    printf("  remote kick <id>       - Disconnect a specific client\r\n");
    printf("  remote send <text>     - Broadcast text to all connected clients\r\n");
}

static void cmd_remote_status(void) {
    remote_shell_status_t status;
    int r = remote_shell_get_status(&status);

    if (r != 0) {
        printf("Remote shell not available\r\n");
        return;
    }

    printf("Remote Shell Status:\r\n");
    printf("  Listener:     %s\r\n", status.listening ? "ACTIVE" : "STOPPED");

    if (status.listening) {
        printf("  Port:         %u\r\n", status.port);
    }

    printf("  Clients:      %u / %d\r\n", status.active_clients, REMOTE_SHELL_MAX_CLIENTS);
    printf("  Total conn:   %lu\r\n", (unsigned long)status.total_connections);

    if (status.active_clients > 0) {
        printf("\r\n  Connected clients:\r\n");
        for (int i = 0; i < REMOTE_SHELL_MAX_CLIENTS; i++) {
            if (!status.clients[i].active) continue;

            uint32_t ip = status.clients[i].client_ip;
            uint32_t uptime_ms = 0;

            /* Calculate uptime if we have a connected_at timestamp */
            if (status.clients[i].connected_at_ms > 0) {
#ifdef PICO_W
                extern uint32_t to_ms_since_boot(absolute_time_t t);
                extern absolute_time_t get_absolute_time(void);
                uint32_t now = to_ms_since_boot(get_absolute_time());
                uptime_ms = now - status.clients[i].connected_at_ms;
#endif
            }

            uint32_t uptime_s = uptime_ms / 1000;
            uint32_t mins = uptime_s / 60;
            uint32_t secs = uptime_s % 60;

            printf("  [%d] %d.%d.%d.%d:%u  up %lum%lus  rx:%lu tx:%lu\r\n",
                   i,
                   (ip >>  0) & 0xFF,
                   (ip >>  8) & 0xFF,
                   (ip >> 16) & 0xFF,
                   (ip >> 24) & 0xFF,
                   status.clients[i].client_port,
                   (unsigned long)mins, (unsigned long)secs,
                   (unsigned long)status.clients[i].bytes_rx,
                   (unsigned long)status.clients[i].bytes_tx);
        }
    }
}

int cmd_remote(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_remote_usage();
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) {
        cmd_remote_status();
        return 0;
    }

    if (strcmp(argv[1], "start") == 0) {
        uint16_t port = REMOTE_SHELL_DEFAULT_PORT;
        if (argc >= 3) {
            int p = atoi(argv[2]);
            if (p > 0 && p <= 65535) {
                port = (uint16_t)p;
            } else {
                printf("Invalid port: %s\r\n", argv[2]);
                return -1;
            }
        }

        printf("Starting remote shell on port %u...\r\n", port);
        int r = remote_shell_start(port);
        if (r == 0) {
            printf("Remote shell listening on port %u\r\n", port);
        } else {
            printf("Failed to start remote shell\r\n");
        }
        return r;
    }

    if (strcmp(argv[1], "stop") == 0) {
        printf("Stopping remote shell...\r\n");
        int r = remote_shell_stop();
        if (r == 0) {
            printf("Remote shell stopped\r\n");
        } else {
            printf("Remote shell is not running\r\n");
        }
        return r;
    }

    if (strcmp(argv[1], "kick") == 0) {
        if (argc < 3) {
            printf("Usage: remote kick <client_id>\r\n");
            return -1;
        }
        int id = atoi(argv[2]);
        if (id < 0 || id >= REMOTE_SHELL_MAX_CLIENTS) {
            printf("Invalid client ID (0-%d)\r\n", REMOTE_SHELL_MAX_CLIENTS - 1);
            return -1;
        }
        int r = remote_shell_kick(id);
        if (r == 0) {
            printf("Client %d disconnected\r\n", id);
        } else {
            printf("Client %d not connected\r\n", id);
        }
        return r;
    }

    if (strcmp(argv[1], "send") == 0) {
        if (argc < 3) {
            printf("Usage: remote send <text>\r\n");
            return -1;
        }

        /* Reconstruct the message from remaining args */
        char msg[REMOTE_SHELL_BUF_SIZE];
        int pos = 0;
        for (int i = 2; i < argc && pos < (int)sizeof(msg) - 2; i++) {
            if (i > 2 && pos < (int)sizeof(msg) - 1) {
                msg[pos++] = ' ';
            }
            size_t arglen = strlen(argv[i]);
            size_t space = (size_t)(sizeof(msg) - 1 - (size_t)pos);
            size_t to_copy = (arglen < space) ? arglen : space;
            memcpy(&msg[pos], argv[i], to_copy);
            pos += (int)to_copy;
        }
        /* Append newline */
        if (pos < (int)sizeof(msg) - 2) {
            msg[pos++] = '\r';
            msg[pos++] = '\n';
        }
        msg[pos] = '\0';

        int sent = remote_shell_broadcast(msg, (size_t)pos);
        if (sent > 0) {
            printf("Sent to %d client(s)\r\n", sent);
        } else if (sent == 0) {
            printf("No clients connected\r\n");
        } else {
            printf("Failed to broadcast\r\n");
        }
        return (sent >= 0) ? 0 : -1;
    }

    cmd_remote_usage();
    return -1;
}
