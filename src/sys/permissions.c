#include "permissions.h"
#include <string.h>

/*
 * littleOS Permissions System Implementation
 * 
 * Core permission checking, privilege escalation, and resource management
 */

/* ============================================================================
 * Permission Checking Implementation
 * ============================================================================
 */

bool perm_check(const task_sec_ctx_t *task_ctx,
                const resource_perm_t *resource_perm,
                uint8_t required_perm)
{
    uid_t effective_uid = task_ctx->euid;
    gid_t effective_gid = task_ctx->egid;
    
    /* Root bypass - always allow (but could log for audit) */
    if (effective_uid == UID_ROOT) {
        return true;
    }
    
    /* Owner permissions - higher priority */
    if (effective_uid == resource_perm->owner_uid) {
        uint8_t owner_perms = PERM_GET_OWNER(resource_perm->perms);
        bool allowed = (owner_perms & required_perm) != 0;
        return allowed;
    }
    
    /* Group permissions */
    if (effective_gid == resource_perm->owner_gid) {
        uint8_t group_perms = PERM_GET_GROUP(resource_perm->perms);
        bool allowed = (group_perms & required_perm) != 0;
        return allowed;
    }
    
    /* Other permissions - default case */
    {
        uint8_t other_perms = PERM_GET_OTHER(resource_perm->perms);
        bool allowed = (other_perms & required_perm) != 0;
        return allowed;
    }
}

/* ============================================================================
 * Privilege Escalation
 * ============================================================================
 */

bool perm_seteuid(task_sec_ctx_t *task_ctx, uid_t new_euid)
{
    /* Rules for setuid:
     * 1. Can always set to real UID (drop privilege)
     * 2. If already root, can set to any UID
     * 3. With CAP_SETUID capability, can set to any UID
     * 4. Otherwise, cannot escalate
     */
    
    uid_t real_uid = task_ctx->uid;
    uid_t current_euid = task_ctx->euid;
    
    /* Can always drop back to real UID */
    if (new_euid == real_uid) {
        task_ctx->euid = new_euid;
        return true;
    }
    
    /* If already root, can set to anything */
    if (current_euid == UID_ROOT) {
        task_ctx->euid = new_euid;
        return true;
    }
    
    /* Otherwise deny (no capability system in basic version) */
    return false;
}

bool perm_setegid(task_sec_ctx_t *task_ctx, gid_t new_egid)
{
    gid_t real_gid = task_ctx->gid;
    gid_t current_egid = task_ctx->egid;
    
    /* Can always drop back to real GID */
    if (new_egid == real_gid) {
        task_ctx->egid = new_egid;
        return true;
    }
    
    /* If already in group with privilege, allow */
    if (current_egid == GID_ROOT) {
        task_ctx->egid = new_egid;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Capability System
 * ============================================================================
 */

void perm_grant_capability(task_sec_ctx_t *task_ctx, uint32_t capability)
{
    task_ctx->capabilities |= capability;
}

void perm_revoke_capability(task_sec_ctx_t *task_ctx, uint32_t capability)
{
    task_ctx->capabilities &= ~capability;
}

/* ============================================================================
 * Resource Management
 * ============================================================================
 */

resource_perm_t perm_resource_create(uid_t owner_uid,
                                     gid_t owner_gid,
                                     perm_bits_t perms,
                                     uint8_t type)
{
    resource_perm_t resource = {
        .perms = perms,
        .owner_uid = owner_uid,
        .owner_gid = owner_gid,
        .type = type,
        .flags = 0
    };
    return resource;
}

bool perm_chown(const task_sec_ctx_t *task_ctx,
                resource_perm_t *resource,
                uid_t new_uid,
                gid_t new_gid)
{
    /* Only root can change ownership */
    if (task_ctx->euid != UID_ROOT) {
        return false;
    }
    
    resource->owner_uid = new_uid;
    resource->owner_gid = new_gid;
    return true;
}

bool perm_chmod(const task_sec_ctx_t *task_ctx,
                resource_perm_t *resource,
                perm_bits_t new_perms)
{
    /* Owner can change their own resource */
    if (task_ctx->uid == resource->owner_uid) {
        resource->perms = new_perms;
        return true;
    }
    
    /* Root can change any resource */
    if (task_ctx->euid == UID_ROOT) {
        resource->perms = new_perms;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Audit Logging (Minimal Implementation)
 * ============================================================================
 */

/* Simple circular buffer for audit log (optional feature) */
#ifdef LITTLEOS_AUDIT_ENABLED

#define AUDIT_LOG_SIZE 256

typedef struct {
    audit_event_t event;
    pid_t pid;
    uid_t uid;
    uint32_t timestamp;
    char description[64];
} audit_entry_t;

static audit_entry_t audit_log[AUDIT_LOG_SIZE];
static uint16_t audit_log_index = 0;

void perm_audit_log(audit_event_t event,
                    pid_t pid,
                    uid_t uid,
                    const char *description)
{
    audit_entry_t *entry = &audit_log[audit_log_index];
    entry->event = event;
    entry->pid = pid;
    entry->uid = uid;
    entry->timestamp = 0;  /* Would use timer_get_ticks() in real implementation */
    
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
        entry->description[sizeof(entry->description) - 1] = '\0';
    } else {
        entry->description[0] = '\0';
    }
    
    audit_log_index = (audit_log_index + 1) % AUDIT_LOG_SIZE;
}

#else

void perm_audit_log(audit_event_t event,
                    pid_t pid,
                    uid_t uid,
                    const char *description)
{
    /* Audit disabled - no-op */
    (void)event;
    (void)pid;
    (void)uid;
    (void)description;
}

#endif /* LITTLEOS_AUDIT_ENABLED */

/* ============================================================================
 * Helper Functions for Integration
 * ============================================================================
 */

/**
 * Initialize default security context for a task
 * 
 * @param uid       User ID for the task
 * @param is_system Whether this is a system task (gets system UID/GID)
 */
task_sec_ctx_t perm_init_context(uid_t uid, bool is_system)
{
    task_sec_ctx_t ctx = {
        .uid = uid,
        .euid = uid,
        .gid = is_system ? GID_SYSTEM : GID_USERS,
        .egid = is_system ? GID_SYSTEM : GID_USERS,
        .umask = 0022,  /* Default: owner rw, others r */
        .capabilities = 0
    };
    
    /* Root task gets all capabilities */
    if (uid == UID_ROOT) {
        ctx.capabilities = CAP_ALL;
    }
    
    return ctx;
}

/**
 * Verify a task can access a resource for a given operation
 * 
 * Combines permission check with capability check
 */
bool perm_task_can_access(const task_sec_ctx_t *task_ctx,
                          const resource_perm_t *resource,
                          uint8_t required_perm)
{
    /* First check standard permissions */
    if (!perm_check(task_ctx, resource, required_perm)) {
        return false;
    }
    
    /* Then check capability requirements based on resource type */
    switch (resource->type) {
        case RESOURCE_DEVICE:
            /* Writing to devices might require specific capability */
            if (required_perm & PERM_WRITE) {
                /* Specific device types could check specific caps */
                /* For now, permission bits are sufficient */
            }
            break;
        
        case RESOURCE_TASK:
            /* Killing tasks requires capability */
            if (required_perm & PERM_EXEC) {
                if (!perm_has_capability(task_ctx, CAP_TASK_KILL)) {
                    return false;
                }
            }
            break;
        
        default:
            break;
    }
    
    return true;
}
