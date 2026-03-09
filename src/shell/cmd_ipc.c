/* cmd_ipc.c - IPC shell commands */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"

static void cmd_ipc_usage(void) {
    printf("Usage:\r\n");
    printf("  ipc status              - Show IPC subsystem status\r\n");
    printf("  ipc channel create <n>  - Create named channel\r\n");
    printf("  ipc channel destroy <id>- Destroy channel\r\n");
    printf("  ipc channel list        - List all channels\r\n");
    printf("  ipc send <ch> <msg>     - Send message to channel\r\n");
    printf("  ipc recv <ch>           - Receive message from channel\r\n");
    printf("  ipc peek <ch>           - Peek at next message\r\n");
    printf("  ipc sem create <n> <c>  - Create semaphore (name, count)\r\n");
    printf("  ipc sem post <id>       - Post (increment) semaphore\r\n");
    printf("  ipc sem wait <id>       - Wait (decrement) semaphore\r\n");
    printf("  ipc sem value <id>      - Get semaphore value\r\n");
    printf("  ipc shm create <n> <sz> - Create shared memory region\r\n");
    printf("  ipc shm write <id> <d>  - Write data to shared memory\r\n");
    printf("  ipc shm read <id>       - Read from shared memory\r\n");
}

int cmd_ipc(int argc, char *argv[]) {
    if (argc < 2) {
        cmd_ipc_usage();
        return -1;
    }

    if (strcmp(argv[1], "status") == 0) {
        ipc_print_stats();
        return 0;
    }

    if (strcmp(argv[1], "channel") == 0) {
        if (argc < 3) {
            cmd_ipc_usage();
            return -1;
        }
        if (strcmp(argv[2], "create") == 0) {
            if (argc < 4) {
                printf("Usage: ipc channel create <name>\r\n");
                return -1;
            }
            int id = ipc_channel_create(argv[3]);
            if (id < 0) {
                printf("ipc: failed to create channel: %d\r\n", id);
                return id;
            }
            printf("ipc: created channel '%s' (id=%d)\r\n", argv[3], id);
            return 0;
        }
        if (strcmp(argv[2], "destroy") == 0) {
            if (argc < 4) {
                printf("Usage: ipc channel destroy <id>\r\n");
                return -1;
            }
            int id = atoi(argv[3]);
            int r = ipc_channel_destroy(id);
            if (r != IPC_OK) {
                printf("ipc: failed to destroy channel %d: %d\r\n", id, r);
                return r;
            }
            printf("ipc: destroyed channel %d\r\n", id);
            return 0;
        }
        if (strcmp(argv[2], "list") == 0) {
            printf("Channel listing:\r\n");
            for (int i = 0; i < IPC_MAX_CHANNELS; i++) {
                ipc_channel_stats_t stats;
                if (ipc_channel_stats(i, &stats) == IPC_OK) {
                    int pending = ipc_pending(i);
                    printf("  [%d] pending=%d sent=%lu recv=%lu dropped=%lu\r\n",
                           i, pending,
                           (unsigned long)stats.messages_sent,
                           (unsigned long)stats.messages_received,
                           (unsigned long)stats.messages_dropped);
                }
            }
            return 0;
        }
    }

    if (strcmp(argv[1], "send") == 0) {
        if (argc < 4) {
            printf("Usage: ipc send <channel_id> <message>\r\n");
            return -1;
        }
        int ch = atoi(argv[2]);
        const char *msg = argv[3];
        int r = ipc_send(ch, 0, 1, msg, (uint16_t)strlen(msg), IPC_PRIORITY_NORMAL);
        if (r != IPC_OK) {
            printf("ipc: send failed: %d\r\n", r);
            return r;
        }
        printf("ipc: sent %zu bytes to channel %d\r\n", strlen(msg), ch);
        return 0;
    }

    if (strcmp(argv[1], "recv") == 0) {
        if (argc < 3) {
            printf("Usage: ipc recv <channel_id>\r\n");
            return -1;
        }
        int ch = atoi(argv[2]);
        ipc_message_t msg;
        int r = ipc_recv(ch, &msg);
        if (r != IPC_OK) {
            printf("ipc: no message (channel %d empty)\r\n", ch);
            return r;
        }
        printf("ipc: from task %d, type %d, %d bytes: ",
               msg.header.sender_task, msg.header.msg_type, msg.header.payload_len);
        fwrite(msg.payload, 1, msg.header.payload_len, stdout);
        printf("\r\n");
        return 0;
    }

    if (strcmp(argv[1], "peek") == 0) {
        if (argc < 3) {
            printf("Usage: ipc peek <channel_id>\r\n");
            return -1;
        }
        int ch = atoi(argv[2]);
        ipc_message_t msg;
        int r = ipc_peek(ch, &msg);
        if (r != IPC_OK) {
            printf("ipc: no message to peek\r\n");
            return r;
        }
        printf("ipc: [peek] from task %d, type %d, %d bytes\r\n",
               msg.header.sender_task, msg.header.msg_type, msg.header.payload_len);
        return 0;
    }

    if (strcmp(argv[1], "sem") == 0) {
        if (argc < 3) {
            cmd_ipc_usage();
            return -1;
        }
        if (strcmp(argv[2], "create") == 0) {
            if (argc < 5) {
                printf("Usage: ipc sem create <name> <initial_count>\r\n");
                return -1;
            }
            int count = atoi(argv[4]);
            int id = ipc_sem_create(argv[3], count, count + 10);
            if (id < 0) {
                printf("ipc: failed to create semaphore: %d\r\n", id);
                return id;
            }
            printf("ipc: created semaphore '%s' (id=%d, count=%d)\r\n", argv[3], id, count);
            return 0;
        }
        if (strcmp(argv[2], "post") == 0) {
            if (argc < 4) { printf("Usage: ipc sem post <id>\r\n"); return -1; }
            int id = atoi(argv[3]);
            int r = ipc_sem_post(id);
            printf("ipc: sem post %d -> %s (value=%d)\r\n", id,
                   r == IPC_OK ? "ok" : "error", ipc_sem_getvalue(id));
            return r;
        }
        if (strcmp(argv[2], "wait") == 0) {
            if (argc < 4) { printf("Usage: ipc sem wait <id>\r\n"); return -1; }
            int id = atoi(argv[3]);
            int r = ipc_sem_trywait(id, 0);
            if (r == IPC_OK) {
                printf("ipc: sem wait %d -> acquired (value=%d)\r\n", id, ipc_sem_getvalue(id));
            } else {
                printf("ipc: sem wait %d -> would block (value=%d)\r\n", id, ipc_sem_getvalue(id));
            }
            return r;
        }
        if (strcmp(argv[2], "value") == 0) {
            if (argc < 4) { printf("Usage: ipc sem value <id>\r\n"); return -1; }
            int id = atoi(argv[3]);
            printf("ipc: semaphore %d value = %d\r\n", id, ipc_sem_getvalue(id));
            return 0;
        }
    }

    if (strcmp(argv[1], "shm") == 0) {
        if (argc < 3) {
            cmd_ipc_usage();
            return -1;
        }
        if (strcmp(argv[2], "create") == 0) {
            if (argc < 5) {
                printf("Usage: ipc shm create <name> <size>\r\n");
                return -1;
            }
            uint32_t size = (uint32_t)strtoul(argv[4], NULL, 0);
            int id = ipc_shmem_create(argv[3], size, 0);
            if (id < 0) {
                printf("ipc: failed to create shared memory: %d\r\n", id);
                return id;
            }
            printf("ipc: created shared memory '%s' (id=%d, size=%lu)\r\n",
                   argv[3], id, (unsigned long)size);
            return 0;
        }
        if (strcmp(argv[2], "write") == 0) {
            if (argc < 5) {
                printf("Usage: ipc shm write <id> <data>\r\n");
                return -1;
            }
            int id = atoi(argv[3]);
            const char *data = argv[4];
            int r = ipc_shmem_write(id, 0, data, (uint32_t)strlen(data));
            if (r != IPC_OK) {
                printf("ipc: shm write failed: %d\r\n", r);
                return r;
            }
            printf("ipc: wrote %zu bytes to shared memory %d\r\n", strlen(data), id);
            return 0;
        }
        if (strcmp(argv[2], "read") == 0) {
            if (argc < 4) {
                printf("Usage: ipc shm read <id>\r\n");
                return -1;
            }
            int id = atoi(argv[3]);
            char buf[128];
            memset(buf, 0, sizeof(buf));
            int r = ipc_shmem_read(id, 0, buf, sizeof(buf) - 1);
            if (r != IPC_OK) {
                printf("ipc: shm read failed: %d\r\n", r);
                return r;
            }
            printf("ipc: shm[%d] = \"%s\"\r\n", id, buf);
            return 0;
        }
    }

    cmd_ipc_usage();
    return -1;
}
