/* ipc.h - Inter-Process Communication for littleOS */
#ifndef LITTLEOS_IPC_H
#define LITTLEOS_IPC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Message Passing
 * ============================================================================ */

#define IPC_MAX_MSG_SIZE        128     /* Max payload per message */
#define IPC_MAX_CHANNELS        8       /* Max concurrent channels */
#define IPC_CHANNEL_DEPTH       16      /* Messages per channel queue */
#define IPC_CHANNEL_NAME_LEN    16      /* Max channel name length */

/* Message priority */
typedef enum {
    IPC_PRIORITY_LOW    = 0,
    IPC_PRIORITY_NORMAL = 1,
    IPC_PRIORITY_HIGH   = 2,
    IPC_PRIORITY_URGENT = 3,
} ipc_priority_t;

/* Message header */
typedef struct {
    uint16_t    sender_task;            /* Sender task ID */
    uint16_t    receiver_task;          /* Receiver task ID (0 = broadcast) */
    uint16_t    msg_type;               /* Application-defined type */
    uint16_t    payload_len;            /* Payload length */
    uint32_t    timestamp_ms;           /* Send timestamp */
    uint8_t     priority;               /* Message priority */
    uint8_t     flags;                  /* Flags (IPC_FLAG_*) */
    uint8_t     _pad[2];
} ipc_msg_header_t;

/* Flags */
#define IPC_FLAG_NONE       0x00
#define IPC_FLAG_URGENT     0x01
#define IPC_FLAG_REPLY      0x02        /* This is a reply to a request */
#define IPC_FLAG_BROADCAST  0x04        /* Broadcast to all listeners */

/* Complete message */
typedef struct {
    ipc_msg_header_t header;
    uint8_t          payload[IPC_MAX_MSG_SIZE];
} ipc_message_t;

/* Channel statistics */
typedef struct {
    uint32_t    messages_sent;
    uint32_t    messages_received;
    uint32_t    messages_dropped;       /* Queue full drops */
    uint32_t    bytes_transferred;
} ipc_channel_stats_t;

/* ============================================================================
 * Semaphore
 * ============================================================================ */

#define IPC_MAX_SEMAPHORES  8

#ifndef LITTLEOS_MAX_TASKS
#define LITTLEOS_MAX_TASKS 16
#endif

typedef struct {
    volatile int32_t count;
    int32_t          max_count;
    char             name[IPC_CHANNEL_NAME_LEN];
    bool             initialized;
    uint16_t         waiting_tasks[LITTLEOS_MAX_TASKS];
    uint16_t         waiting_count;
} ipc_semaphore_t;

/* ============================================================================
 * Shared Memory
 * ============================================================================ */

#define IPC_MAX_SHMEM_REGIONS   4
#define IPC_SHMEM_MAX_SIZE      1024

typedef struct {
    uint8_t     data[IPC_SHMEM_MAX_SIZE];
    uint32_t    size;
    char        name[IPC_CHANNEL_NAME_LEN];
    uint16_t    owner_task;
    bool        initialized;
    bool        locked;
    uint16_t    lock_holder;
} ipc_shmem_t;

/* ============================================================================
 * Public API - System
 * ============================================================================ */

void ipc_init(void);
void ipc_print_stats(void);

/* ============================================================================
 * Public API - Message Passing
 * ============================================================================ */

/* Create a named message channel */
int ipc_channel_create(const char *name);

/* Destroy a channel */
int ipc_channel_destroy(int channel_id);

/* Send message to a channel */
int ipc_send(int channel_id, uint16_t sender_task, uint16_t msg_type,
             const void *payload, uint16_t payload_len, ipc_priority_t priority);

/* Receive message from a channel (non-blocking, returns -1 if empty) */
int ipc_recv(int channel_id, ipc_message_t *msg);

/* Peek at next message without removing (non-blocking) */
int ipc_peek(int channel_id, ipc_message_t *msg);

/* Get number of pending messages in channel */
int ipc_pending(int channel_id);

/* Find channel by name */
int ipc_channel_find(const char *name);

/* Get channel statistics */
int ipc_channel_stats(int channel_id, ipc_channel_stats_t *stats);

/* ============================================================================
 * Public API - Semaphores
 * ============================================================================ */

int  ipc_sem_create(const char *name, int32_t initial_count, int32_t max_count);
int  ipc_sem_destroy(int sem_id);
int  ipc_sem_wait(int sem_id, uint16_t task_id);       /* Decrement (blocks conceptually) */
int  ipc_sem_post(int sem_id);                          /* Increment */
int  ipc_sem_trywait(int sem_id, uint16_t task_id);    /* Non-blocking wait */
int  ipc_sem_getvalue(int sem_id);

/* ============================================================================
 * Public API - Shared Memory
 * ============================================================================ */

int  ipc_shmem_create(const char *name, uint32_t size, uint16_t owner_task);
int  ipc_shmem_destroy(int shm_id);
void *ipc_shmem_attach(int shm_id);
int  ipc_shmem_lock(int shm_id, uint16_t task_id);
int  ipc_shmem_unlock(int shm_id, uint16_t task_id);
int  ipc_shmem_write(int shm_id, uint32_t offset, const void *data, uint32_t len);
int  ipc_shmem_read(int shm_id, uint32_t offset, void *data, uint32_t len);

/* ============================================================================
 * Error codes
 * ============================================================================ */

#define IPC_OK              0
#define IPC_ERR_FULL        (-1)
#define IPC_ERR_EMPTY       (-2)
#define IPC_ERR_NOT_FOUND   (-3)
#define IPC_ERR_INVALID     (-4)
#define IPC_ERR_LOCKED      (-5)
#define IPC_ERR_NO_RESOURCE (-6)
#define IPC_ERR_PERMISSION  (-7)

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_IPC_H */
