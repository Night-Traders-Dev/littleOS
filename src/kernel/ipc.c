/**
 * littleOS IPC - Inter-Process Communication
 *
 * Message passing, counting semaphores, and shared memory regions
 * for cooperative multitasking on RP2040.
 */

#include "ipc.h"
#include "dmesg.h"
#include <stdio.h>
#include <string.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#define IPC_TIMESTAMP_MS()  to_ms_since_boot(get_absolute_time())
#else
#define IPC_TIMESTAMP_MS()  0
#endif

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct {
    char              name[IPC_CHANNEL_NAME_LEN];
    ipc_message_t     queue[IPC_CHANNEL_DEPTH];
    uint16_t          head;
    uint16_t          tail;
    uint16_t          count;
    bool              used;
    ipc_channel_stats_t stats;
} ipc_channel_t;

static ipc_channel_t  channels[IPC_MAX_CHANNELS];
static ipc_semaphore_t semaphores[IPC_MAX_SEMAPHORES];
static ipc_shmem_t     shmem_regions[IPC_MAX_SHMEM_REGIONS];

/* ============================================================================
 * System Init
 * ============================================================================ */

void ipc_init(void)
{
    memset(channels, 0, sizeof(channels));
    memset(semaphores, 0, sizeof(semaphores));
    memset(shmem_regions, 0, sizeof(shmem_regions));
    dmesg_info("IPC: initialized (%d channels, %d semaphores, %d shmem regions)",
               IPC_MAX_CHANNELS, IPC_MAX_SEMAPHORES, IPC_MAX_SHMEM_REGIONS);
}

/* ============================================================================
 * Message Passing
 * ============================================================================ */

int ipc_channel_create(const char *name)
{
    if (!name) return IPC_ERR_INVALID;

    for (int i = 0; i < IPC_MAX_CHANNELS; i++) {
        if (!channels[i].used) {
            memset(&channels[i], 0, sizeof(ipc_channel_t));
            strncpy(channels[i].name, name, IPC_CHANNEL_NAME_LEN - 1);
            channels[i].name[IPC_CHANNEL_NAME_LEN - 1] = '\0';
            channels[i].used = true;
            dmesg_info("IPC: channel '%s' created (id=%d)", channels[i].name, i);
            return i;
        }
    }
    return IPC_ERR_NO_RESOURCE;
}

int ipc_channel_destroy(int channel_id)
{
    if (channel_id < 0 || channel_id >= IPC_MAX_CHANNELS) return IPC_ERR_INVALID;
    if (!channels[channel_id].used) return IPC_ERR_NOT_FOUND;

    dmesg_info("IPC: channel '%s' destroyed (id=%d)", channels[channel_id].name, channel_id);
    memset(&channels[channel_id], 0, sizeof(ipc_channel_t));
    return IPC_OK;
}

int ipc_send(int channel_id, uint16_t sender_task, uint16_t msg_type,
             const void *payload, uint16_t payload_len, ipc_priority_t priority)
{
    if (channel_id < 0 || channel_id >= IPC_MAX_CHANNELS) return IPC_ERR_INVALID;

    ipc_channel_t *ch = &channels[channel_id];
    if (!ch->used) return IPC_ERR_NOT_FOUND;

    if (payload_len > IPC_MAX_MSG_SIZE) return IPC_ERR_INVALID;

    if (ch->count >= IPC_CHANNEL_DEPTH) {
        ch->stats.messages_dropped++;
        return IPC_ERR_FULL;
    }

    /* Build the message */
    ipc_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.sender_task  = sender_task;
    msg.header.receiver_task = 0;
    msg.header.msg_type     = msg_type;
    msg.header.payload_len  = payload_len;
    msg.header.timestamp_ms = IPC_TIMESTAMP_MS();
    msg.header.priority     = (uint8_t)priority;
    msg.header.flags        = (priority == IPC_PRIORITY_URGENT) ? IPC_FLAG_URGENT : IPC_FLAG_NONE;

    if (payload && payload_len > 0) {
        memcpy(msg.payload, payload, payload_len);
    }

    if (priority == IPC_PRIORITY_URGENT && ch->count > 0) {
        /*
         * Urgent: insert at the front of the circular buffer.
         * Move head backward by one slot.
         */
        if (ch->head == 0) {
            ch->head = IPC_CHANNEL_DEPTH - 1;
        } else {
            ch->head--;
        }
        ch->queue[ch->head] = msg;
    } else {
        /* Normal insertion at the tail */
        ch->queue[ch->tail] = msg;
        ch->tail = (ch->tail + 1) % IPC_CHANNEL_DEPTH;
    }

    ch->count++;
    ch->stats.messages_sent++;
    ch->stats.bytes_transferred += payload_len;

    return IPC_OK;
}

int ipc_recv(int channel_id, ipc_message_t *msg)
{
    if (channel_id < 0 || channel_id >= IPC_MAX_CHANNELS) return IPC_ERR_INVALID;
    if (!msg) return IPC_ERR_INVALID;

    ipc_channel_t *ch = &channels[channel_id];
    if (!ch->used) return IPC_ERR_NOT_FOUND;

    if (ch->count == 0) return IPC_ERR_EMPTY;

    *msg = ch->queue[ch->head];
    ch->head = (ch->head + 1) % IPC_CHANNEL_DEPTH;
    ch->count--;
    ch->stats.messages_received++;

    return IPC_OK;
}

int ipc_peek(int channel_id, ipc_message_t *msg)
{
    if (channel_id < 0 || channel_id >= IPC_MAX_CHANNELS) return IPC_ERR_INVALID;
    if (!msg) return IPC_ERR_INVALID;

    ipc_channel_t *ch = &channels[channel_id];
    if (!ch->used) return IPC_ERR_NOT_FOUND;

    if (ch->count == 0) return IPC_ERR_EMPTY;

    *msg = ch->queue[ch->head];
    return IPC_OK;
}

int ipc_pending(int channel_id)
{
    if (channel_id < 0 || channel_id >= IPC_MAX_CHANNELS) return IPC_ERR_INVALID;
    if (!channels[channel_id].used) return IPC_ERR_NOT_FOUND;
    return (int)channels[channel_id].count;
}

int ipc_channel_find(const char *name)
{
    if (!name) return IPC_ERR_INVALID;

    for (int i = 0; i < IPC_MAX_CHANNELS; i++) {
        if (channels[i].used && strncmp(channels[i].name, name, IPC_CHANNEL_NAME_LEN) == 0) {
            return i;
        }
    }
    return IPC_ERR_NOT_FOUND;
}

int ipc_channel_stats(int channel_id, ipc_channel_stats_t *stats)
{
    if (channel_id < 0 || channel_id >= IPC_MAX_CHANNELS) return IPC_ERR_INVALID;
    if (!stats) return IPC_ERR_INVALID;
    if (!channels[channel_id].used) return IPC_ERR_NOT_FOUND;

    *stats = channels[channel_id].stats;
    return IPC_OK;
}

/* ============================================================================
 * Semaphores
 * ============================================================================ */

int ipc_sem_create(const char *name, int32_t initial_count, int32_t max_count)
{
    if (!name || initial_count < 0 || max_count < 1 || initial_count > max_count) {
        return IPC_ERR_INVALID;
    }

    for (int i = 0; i < IPC_MAX_SEMAPHORES; i++) {
        if (!semaphores[i].initialized) {
            memset(&semaphores[i], 0, sizeof(ipc_semaphore_t));
            strncpy(semaphores[i].name, name, IPC_CHANNEL_NAME_LEN - 1);
            semaphores[i].name[IPC_CHANNEL_NAME_LEN - 1] = '\0';
            semaphores[i].count = initial_count;
            semaphores[i].max_count = max_count;
            semaphores[i].initialized = true;
            dmesg_info("IPC: semaphore '%s' created (id=%d, count=%d, max=%d)",
                       semaphores[i].name, i, (int)initial_count, (int)max_count);
            return i;
        }
    }
    return IPC_ERR_NO_RESOURCE;
}

int ipc_sem_destroy(int sem_id)
{
    if (sem_id < 0 || sem_id >= IPC_MAX_SEMAPHORES) return IPC_ERR_INVALID;
    if (!semaphores[sem_id].initialized) return IPC_ERR_NOT_FOUND;

    dmesg_info("IPC: semaphore '%s' destroyed (id=%d)", semaphores[sem_id].name, sem_id);
    memset(&semaphores[sem_id], 0, sizeof(ipc_semaphore_t));
    return IPC_OK;
}

int ipc_sem_wait(int sem_id, uint16_t task_id)
{
    if (sem_id < 0 || sem_id >= IPC_MAX_SEMAPHORES) return IPC_ERR_INVALID;
    if (!semaphores[sem_id].initialized) return IPC_ERR_NOT_FOUND;

    ipc_semaphore_t *sem = &semaphores[sem_id];

    if (sem->count > 0) {
        sem->count--;
        return IPC_OK;
    }

    /*
     * Count is 0 - in a cooperative system we cannot truly block.
     * Record the task as waiting and return IPC_ERR_LOCKED so the
     * caller can yield and retry.
     */
    if (sem->waiting_count < LITTLEOS_MAX_TASKS) {
        sem->waiting_tasks[sem->waiting_count++] = task_id;
    }
    return IPC_ERR_LOCKED;
}

int ipc_sem_post(int sem_id)
{
    if (sem_id < 0 || sem_id >= IPC_MAX_SEMAPHORES) return IPC_ERR_INVALID;
    if (!semaphores[sem_id].initialized) return IPC_ERR_NOT_FOUND;

    ipc_semaphore_t *sem = &semaphores[sem_id];

    if (sem->count >= sem->max_count) {
        return IPC_ERR_FULL;
    }

    sem->count++;

    /* Clear first waiting task if any */
    if (sem->waiting_count > 0) {
        /* Shift waiting list */
        for (uint16_t i = 1; i < sem->waiting_count; i++) {
            sem->waiting_tasks[i - 1] = sem->waiting_tasks[i];
        }
        sem->waiting_count--;
    }

    return IPC_OK;
}

int ipc_sem_trywait(int sem_id, uint16_t task_id)
{
    (void)task_id;

    if (sem_id < 0 || sem_id >= IPC_MAX_SEMAPHORES) return IPC_ERR_INVALID;
    if (!semaphores[sem_id].initialized) return IPC_ERR_NOT_FOUND;

    if (semaphores[sem_id].count > 0) {
        semaphores[sem_id].count--;
        return IPC_OK;
    }

    return IPC_ERR_LOCKED;
}

int ipc_sem_getvalue(int sem_id)
{
    if (sem_id < 0 || sem_id >= IPC_MAX_SEMAPHORES) return IPC_ERR_INVALID;
    if (!semaphores[sem_id].initialized) return IPC_ERR_NOT_FOUND;

    return (int)semaphores[sem_id].count;
}

/* ============================================================================
 * Shared Memory
 * ============================================================================ */

int ipc_shmem_create(const char *name, uint32_t size, uint16_t owner_task)
{
    if (!name || size == 0 || size > IPC_SHMEM_MAX_SIZE) return IPC_ERR_INVALID;

    for (int i = 0; i < IPC_MAX_SHMEM_REGIONS; i++) {
        if (!shmem_regions[i].initialized) {
            memset(&shmem_regions[i], 0, sizeof(ipc_shmem_t));
            strncpy(shmem_regions[i].name, name, IPC_CHANNEL_NAME_LEN - 1);
            shmem_regions[i].name[IPC_CHANNEL_NAME_LEN - 1] = '\0';
            shmem_regions[i].size = size;
            shmem_regions[i].owner_task = owner_task;
            shmem_regions[i].initialized = true;
            shmem_regions[i].locked = false;
            dmesg_info("IPC: shmem '%s' created (id=%d, size=%u, owner=%u)",
                       shmem_regions[i].name, i, (unsigned)size, (unsigned)owner_task);
            return i;
        }
    }
    return IPC_ERR_NO_RESOURCE;
}

int ipc_shmem_destroy(int shm_id)
{
    if (shm_id < 0 || shm_id >= IPC_MAX_SHMEM_REGIONS) return IPC_ERR_INVALID;
    if (!shmem_regions[shm_id].initialized) return IPC_ERR_NOT_FOUND;
    if (shmem_regions[shm_id].locked) return IPC_ERR_LOCKED;

    dmesg_info("IPC: shmem '%s' destroyed (id=%d)", shmem_regions[shm_id].name, shm_id);
    memset(&shmem_regions[shm_id], 0, sizeof(ipc_shmem_t));
    return IPC_OK;
}

void *ipc_shmem_attach(int shm_id)
{
    if (shm_id < 0 || shm_id >= IPC_MAX_SHMEM_REGIONS) return NULL;
    if (!shmem_regions[shm_id].initialized) return NULL;

    return (void *)shmem_regions[shm_id].data;
}

int ipc_shmem_lock(int shm_id, uint16_t task_id)
{
    if (shm_id < 0 || shm_id >= IPC_MAX_SHMEM_REGIONS) return IPC_ERR_INVALID;
    if (!shmem_regions[shm_id].initialized) return IPC_ERR_NOT_FOUND;

    ipc_shmem_t *shm = &shmem_regions[shm_id];

    if (shm->locked) {
        if (shm->lock_holder == task_id) {
            return IPC_OK;  /* Already held by this task */
        }
        return IPC_ERR_LOCKED;
    }

    shm->locked = true;
    shm->lock_holder = task_id;
    return IPC_OK;
}

int ipc_shmem_unlock(int shm_id, uint16_t task_id)
{
    if (shm_id < 0 || shm_id >= IPC_MAX_SHMEM_REGIONS) return IPC_ERR_INVALID;
    if (!shmem_regions[shm_id].initialized) return IPC_ERR_NOT_FOUND;

    ipc_shmem_t *shm = &shmem_regions[shm_id];

    if (!shm->locked) return IPC_OK;

    if (shm->lock_holder != task_id) {
        return IPC_ERR_PERMISSION;
    }

    shm->locked = false;
    shm->lock_holder = 0;
    return IPC_OK;
}

int ipc_shmem_write(int shm_id, uint32_t offset, const void *data, uint32_t len)
{
    if (shm_id < 0 || shm_id >= IPC_MAX_SHMEM_REGIONS) return IPC_ERR_INVALID;
    if (!data) return IPC_ERR_INVALID;

    ipc_shmem_t *shm = &shmem_regions[shm_id];
    if (!shm->initialized) return IPC_ERR_NOT_FOUND;

    if (offset + len > shm->size) return IPC_ERR_INVALID;

    memcpy(shm->data + offset, data, len);
    return IPC_OK;
}

int ipc_shmem_read(int shm_id, uint32_t offset, void *data, uint32_t len)
{
    if (shm_id < 0 || shm_id >= IPC_MAX_SHMEM_REGIONS) return IPC_ERR_INVALID;
    if (!data) return IPC_ERR_INVALID;

    ipc_shmem_t *shm = &shmem_regions[shm_id];
    if (!shm->initialized) return IPC_ERR_NOT_FOUND;

    if (offset + len > shm->size) return IPC_ERR_INVALID;

    memcpy(data, shm->data + offset, len);
    return IPC_OK;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void ipc_print_stats(void)
{
    printf("\n=== IPC Status ===\n");

    printf("Channels:\n");
    for (int i = 0; i < IPC_MAX_CHANNELS; i++) {
        if (channels[i].used) {
            ipc_channel_t *ch = &channels[i];
            printf("  [%d] '%s': pending=%u sent=%u recv=%u dropped=%u bytes=%u\n",
                   i, ch->name, (unsigned)ch->count,
                   (unsigned)ch->stats.messages_sent,
                   (unsigned)ch->stats.messages_received,
                   (unsigned)ch->stats.messages_dropped,
                   (unsigned)ch->stats.bytes_transferred);
        }
    }

    printf("Semaphores:\n");
    for (int i = 0; i < IPC_MAX_SEMAPHORES; i++) {
        if (semaphores[i].initialized) {
            printf("  [%d] '%s': count=%d/%d waiting=%u\n",
                   i, semaphores[i].name,
                   (int)semaphores[i].count,
                   (int)semaphores[i].max_count,
                   (unsigned)semaphores[i].waiting_count);
        }
    }

    printf("Shared Memory:\n");
    for (int i = 0; i < IPC_MAX_SHMEM_REGIONS; i++) {
        if (shmem_regions[i].initialized) {
            printf("  [%d] '%s': size=%u owner=%u locked=%s",
                   i, shmem_regions[i].name,
                   (unsigned)shmem_regions[i].size,
                   (unsigned)shmem_regions[i].owner_task,
                   shmem_regions[i].locked ? "yes" : "no");
            if (shmem_regions[i].locked) {
                printf(" (holder=%u)", (unsigned)shmem_regions[i].lock_holder);
            }
            printf("\n");
        }
    }
}
