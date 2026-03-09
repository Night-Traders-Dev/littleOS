/* remote_shell.h - Remote Shell over TCP for littleOS */
#ifndef LITTLEOS_REMOTE_SHELL_H
#define LITTLEOS_REMOTE_SHELL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REMOTE_SHELL_DEFAULT_PORT   2323
#define REMOTE_SHELL_MAX_CLIENTS    2
#define REMOTE_SHELL_BUF_SIZE       512

/* Client connection info */
typedef struct {
    uint32_t client_ip;
    uint16_t client_port;
    uint32_t connected_at_ms;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    bool     active;
} remote_shell_client_t;

/* Remote shell status */
typedef struct {
    bool     listening;
    uint16_t port;
    uint8_t  active_clients;
    uint32_t total_connections;
    remote_shell_client_t clients[REMOTE_SHELL_MAX_CLIENTS];
} remote_shell_status_t;

/* Start remote shell listener on a TCP port */
int remote_shell_start(uint16_t port);

/* Stop remote shell listener */
int remote_shell_stop(void);

/* Process remote shell (call from main loop or a task) */
void remote_shell_task(void);

/* Get remote shell status */
int remote_shell_get_status(remote_shell_status_t *status);

/* Send output to all connected remote clients (mirrors printf) */
int remote_shell_broadcast(const char *data, size_t len);

/* Disconnect a specific client */
int remote_shell_kick(int client_index);

/* Check if remote shell is active */
bool remote_shell_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_REMOTE_SHELL_H */
