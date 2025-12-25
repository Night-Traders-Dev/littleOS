#ifndef LITTLEOS_PERMISSIONS_H
#define LITTLEOS_PERMISSIONS_H

#include <stdint.h>
#include <stdbool.h>

/*
 * littleOS Linux-Inspired Permissions System
 * 
 * Implements UID/GID, permission bits, and access control
 * for embedded systems without traditional filesystems.
 * 
 * Permission model applies to:
 * - Peripheral devices (UART, GPIO, SPI, Timer, etc.)
 * - Memory regions (shared buffers, heaps)
 * - Inter-task communication (message queues, pipes)
 * - System capabilities (task creation, reboot, etc.)
 */

/* ============================================================================
 * User and Group IDs
 * ============================================================================
 */

typedef uint16_t uid_t;  /* User ID */
typedef uint16_t gid_t;  /* Group ID */
typedef uint16_t pid_t;  /* Process ID */

/* Standard UID ranges (matching Linux conventions) */
#define UID_ROOT        0       /* Root user - full privileges */
#define UID_SYSTEM_MIN  1       /* System services start here */
#define UID_SYSTEM_MAX  999     /* System services end here */
#define UID_USER_MIN    1000    /* User applications start here */
#define UID_INVALID     0xFFFF  /* Invalid UID marker */

/* Standard GID ranges */
#define GID_ROOT        0       /* Root group */
#define GID_SYSTEM      1       /* System group */
#define GID_DRIVERS     2       /* Drivers group */
#define GID_USERS       100     /* User group */
#define GID_INVALID     0xFFFF  /* Invalid GID marker */

/* ============================================================================
 * Permission Bits (Unix-Style)
 * ============================================================================
 */

typedef uint16_t perm_bits_t;

/* Permission bit masks */
#define PERM_READ       4       /* r: read access (0400) */
#define PERM_WRITE      2       /* w: write access (0200) */
#define PERM_EXEC       1       /* x: execute access (0100) */

/* Shift amounts for owner/group/other positions */
#define PERM_OWNER_SHIFT    6   /* Bits 8-6: owner permissions */
#define PERM_GROUP_SHIFT    3   /* Bits 5-3: group permissions */
#define PERM_OTHER_SHIFT    0   /* Bits 2-0: other permissions */

/* Permission macros to construct permission bits */
#define PERM_MAKE(owner, group, other) \
    ((owner << PERM_OWNER_SHIFT) | (group << PERM_GROUP_SHIFT) | other)

/* Extract permission component */
#define PERM_GET_OWNER(bits)  ((bits >> PERM_OWNER_SHIFT) & 0x7)
#define PERM_GET_GROUP(bits)  ((bits >> PERM_GROUP_SHIFT) & 0x7)
#define PERM_GET_OTHER(bits)  ((bits >> PERM_OTHER_SHIFT) & 0x7)

/* Common permission presets */
#define PERM_0644  PERM_MAKE(PERM_READ|PERM_WRITE, PERM_READ, PERM_READ)  /* rw-r--r-- */
#define PERM_0640  PERM_MAKE(PERM_READ|PERM_WRITE, PERM_READ, 0)          /* rw-r----- */
#define PERM_0660  PERM_MAKE(PERM_READ|PERM_WRITE, PERM_READ|PERM_WRITE, 0)
#define PERM_0600  PERM_MAKE(PERM_READ|PERM_WRITE, 0, 0)                  /* rw------- */
#define PERM_0755  PERM_MAKE(7, 5, 5)                                      /* rwxr-xr-x */
#define PERM_0700  PERM_MAKE(7, 0, 0)                                      /* rwx------ */
#define PERM_0770  PERM_MAKE(7, 7, 0)                                      /* rwxrwx--- */


/* ============================================================================
 * Task Permissions & Security Context
 * ============================================================================
 */

typedef struct {
    uid_t uid;              /* Real UID - who created this task */
    gid_t gid;              /* Real GID - primary group */
    uid_t euid;             /* Effective UID - for privilege escalation */
    gid_t egid;             /* Effective GID - for privilege escalation */
    uint16_t umask;         /* Default permissions for new resources */
    uint32_t capabilities;  /* Capability flags (Linux-style, advanced) */
} task_sec_ctx_t;

/* ============================================================================
 * Resource Permissions & Ownership
 * ============================================================================
 */

typedef struct {
    perm_bits_t perms;      /* Permission bits (owner|group|other) */
    uid_t owner_uid;        /* Resource owner UID */
    gid_t owner_gid;        /* Resource owner GID */
    uint8_t type;           /* Resource type (device, memory, ipc, etc.) */
    uint16_t flags;         /* Resource-specific flags */
} resource_perm_t;

/* Resource types */
#define RESOURCE_DEVICE     1   /* Peripheral (UART, GPIO, Timer, etc.) */
#define RESOURCE_MEMORY     2   /* Memory region (heap, shared buffer) */
#define RESOURCE_IPC        3   /* IPC channel (queue, pipe, socket) */
#define RESOURCE_SYSCALL    4   /* System call capability */
#define RESOURCE_TASK       5   /* Task/process */

/* ============================================================================
 * Permission Checking
 * ============================================================================
 */

/**
 * Check if a task has specific permission on a resource
 * 
 * @param task_ctx      Security context of the requesting task
 * @param resource_perm Permission metadata of the resource
 * @param required_perm Permission bits needed (PERM_READ, PERM_WRITE, PERM_EXEC)
 * 
 * @return true if access is allowed, false if denied
 * 
 * Algorithm:
 * 1. If task UID == 0 (root), allow (with caution flags)
 * 2. If task owns resource, check owner permissions
 * 3. If task in same group, check group permissions
 * 4. Otherwise, check other permissions
 */
bool perm_check(const task_sec_ctx_t *task_ctx,
                const resource_perm_t *resource_perm,
                uint8_t required_perm);

/**
 * Check if a specific permission bit is set in a permission field
 * 
 * @param perms         Permission bits (0644 style)
 * @param position      Which position to check (PERM_OWNER_SHIFT, etc.)
 * @param required_perm Permission to check for (PERM_READ, PERM_WRITE, PERM_EXEC)
 * 
 * @return true if permission is granted
 */
static inline bool perm_has_bit(perm_bits_t perms, 
                               int position,
                               uint8_t required_perm)
{
    uint8_t perm_set = (perms >> position) & 0x7;
    return (perm_set & required_perm) != 0;
}

/* ============================================================================
 * Privilege Escalation
 * ============================================================================
 */

/**
 * Change effective UID (like setuid in Unix)
 * 
 * Can only escalate to UID 0 if already UID 0 (or via capability)
 * Can always drop back to real UID
 * 
 * @param task_ctx      Task's security context
 * @param new_euid      New effective UID
 * 
 * @return true if successful, false if denied
 */
bool perm_seteuid(task_sec_ctx_t *task_ctx, uid_t new_euid);

/**
 * Change effective GID (like setgid in Unix)
 */
bool perm_setegid(task_sec_ctx_t *task_ctx, gid_t new_egid);

/**
 * Get current effective UID for permission checks
 */
static inline uid_t perm_geteuid(const task_sec_ctx_t *task_ctx)
{
    return task_ctx->euid;
}

/* ============================================================================
 * Capability System (Advanced, Optional)
 * ============================================================================
 */

/* Linux-style capability flags (subset for embedded systems) */
#define CAP_SYS_ADMIN       (1U << 0)   /* Can administer system */
#define CAP_SYS_BOOT        (1U << 1)   /* Can reboot/shutdown */
#define CAP_GPIO_WRITE      (1U << 2)   /* Can write GPIO */
#define CAP_UART_CONFIG     (1U << 3)   /* Can configure UART */
#define CAP_TASK_SPAWN      (1U << 4)   /* Can spawn tasks */
#define CAP_TASK_KILL       (1U << 5)   /* Can kill tasks */
#define CAP_MEM_LOCK        (1U << 6)   /* Can lock memory */
#define CAP_NET_ADMIN       (1U << 7)   /* Can configure networking */
#define CAP_ALL             0xFFFFFFFF  /* All capabilities */

/**
 * Check if a task has a specific capability
 */
static inline bool perm_has_capability(const task_sec_ctx_t *task_ctx,
                                       uint32_t capability)
{
    /* Root always has all capabilities */
    if (perm_geteuid(task_ctx) == UID_ROOT)
        return true;
    
    return (task_ctx->capabilities & capability) != 0;
}

/**
 * Grant a capability to a task
 */
void perm_grant_capability(task_sec_ctx_t *task_ctx, uint32_t capability);

/**
 * Revoke a capability from a task
 */
void perm_revoke_capability(task_sec_ctx_t *task_ctx, uint32_t capability);

/* ============================================================================
 * Resource Management
 * ============================================================================
 */

/**
 * Create a new resource with owner and permissions
 * 
 * @param owner_uid      UID of resource owner
 * @param owner_gid      GID of resource owner
 * @param perms          Permission bits (e.g., PERM_0644)
 * @param type           Resource type (RESOURCE_DEVICE, etc.)
 * 
 * @return Initialized resource permission structure
 */
resource_perm_t perm_resource_create(uid_t owner_uid, 
                                     gid_t owner_gid,
                                     perm_bits_t perms,
                                     uint8_t type);

/**
 * Change resource ownership
 * 
 * Only root can do this
 * 
 * @param task_ctx       Task requesting the change
 * @param resource       Resource to modify
 * @param new_uid        New owner UID
 * @param new_gid        New owner GID
 * 
 * @return true if successful
 */
bool perm_chown(const task_sec_ctx_t *task_ctx,
                resource_perm_t *resource,
                uid_t new_uid,
                gid_t new_gid);

/**
 * Change resource permissions
 * 
 * Owner can change their own resource permissions
 * Root can change any resource
 * 
 * @param task_ctx       Task requesting the change
 * @param resource       Resource to modify
 * @param new_perms      New permission bits
 * 
 * @return true if successful
 */
bool perm_chmod(const task_sec_ctx_t *task_ctx,
                resource_perm_t *resource,
                perm_bits_t new_perms);

/* ============================================================================
 * Audit Logging (Optional)
 * ============================================================================
 */

typedef enum {
    AUDIT_PERM_GRANTED,
    AUDIT_PERM_DENIED,
    AUDIT_PRIVILEGE_ESCALATION,
    AUDIT_PRIVILEGE_DROP,
    AUDIT_RESOURCE_CREATED,
    AUDIT_RESOURCE_DESTROYED,
    AUDIT_OWNERSHIP_CHANGED,
} audit_event_t;

/**
 * Log a security event (for debugging and security auditing)
 * 
 * Only enabled if LITTLEOS_AUDIT_ENABLED is defined
 */
void perm_audit_log(audit_event_t event, 
                    pid_t pid,
                    uid_t uid,
                    const char *description);

#endif /* LITTLEOS_PERMISSIONS_H */
