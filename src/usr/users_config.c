#include "users_config.h"
#include <string.h>
#include <stddef.h>

/*
 * littleOS User Account Database
 * 
 * Compile-time generated user accounts stored in ROM.
 * Minimal runtime overhead - just lookups and context creation.
 */

/* ============================================================================
 * User Account Database (Generated at Compile Time)
 * ============================================================================
 */

/* Unconditionally include root user */
static const user_account_t root_account = {
    .uid = UID_ROOT,
    .gid = GID_ROOT,
    .username = "root",
    .umask = 0022,
    .capabilities = CAP_ALL  /* Root has all capabilities */
};

/* Conditionally include custom user account if enabled */
#if LITTLEOS_ENABLE_USER_ACCOUNT == 1

static const user_account_t custom_user_account = {
    .uid = LITTLEOS_USER_UID,
    .gid = GID_USERS,
    .username = LITTLEOS_USER_NAME,
    .umask = LITTLEOS_USER_UMASK,
    .capabilities = LITTLEOS_USER_CAPABILITIES
};

static const user_account_t *user_accounts[] = {
    &root_account,
    &custom_user_account
};

static const uint16_t user_accounts_count = 2;

#else

static const user_account_t *user_accounts[] = {
    &root_account
};

static const uint16_t user_accounts_count = 1;

#endif /* LITTLEOS_ENABLE_USER_ACCOUNT */

/* ============================================================================
 * User Account Lookup Functions
 * ============================================================================
 */

const user_account_t *users_get_by_uid(uid_t uid)
{
    for (uint16_t i = 0; i < user_accounts_count; i++) {
        if (user_accounts[i]->uid == uid) {
            return user_accounts[i];
        }
    }
    return NULL;
}

const user_account_t *users_get_by_name(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    
    for (uint16_t i = 0; i < user_accounts_count; i++) {
        if (strcmp(user_accounts[i]->username, name) == 0) {
            return user_accounts[i];
        }
    }
    return NULL;
}

void users_init(void)
{
    /* Database is statically initialized - nothing to do at runtime */
    /* Could add validation or logging here if needed */
}

uint16_t users_get_count(void)
{
    return user_accounts_count;
}

const user_account_t *users_get_by_index(uint16_t index)
{
    if (index >= user_accounts_count) {
        return NULL;
    }
    return user_accounts[index];
}

bool users_uid_exists(uid_t uid)
{
    return users_get_by_uid(uid) != NULL;
}

bool users_name_exists(const char *name)
{
    return users_get_by_name(name) != NULL;
}

/* ============================================================================
 * Security Context Creation from User Account
 * ============================================================================
 */

task_sec_ctx_t users_account_to_context(const user_account_t *account)
{
    task_sec_ctx_t ctx = {
        .uid = account->uid,
        .euid = account->uid,
        .gid = account->gid,
        .egid = account->gid,
        .umask = account->umask,
        .capabilities = account->capabilities
    };
    return ctx;
}

/* ============================================================================
 * Helpers for Common Operations
 * ============================================================================
 */

/**
 * Get root user account
 */
const user_account_t *users_get_root(void)
{
    return users_get_by_uid(UID_ROOT);
}

/**
 * Create root security context
 */
task_sec_ctx_t users_root_context(void)
{
    const user_account_t *root = users_get_root();
    return users_account_to_context(root);
}

/**
 * Get default user account (non-root)
 * Returns NULL if only root is configured
 */
const user_account_t *users_get_default_user(void)
{
#if LITTLEOS_ENABLE_USER_ACCOUNT == 1
    return users_get_by_uid(LITTLEOS_USER_UID);
#else
    return NULL;
#endif
}

/**
 * Print user account database (for debugging)
 * Only available if debug output is enabled
 */
#ifdef LITTLEOS_DEBUG_USERS

#include <stdio.h>

void users_print_database(void)
{
    printf("=== littleOS User Database ===\n");
    printf("Total users: %d\n\n", users_get_count());
    
    for (uint16_t i = 0; i < users_get_count(); i++) {
        const user_account_t *user = users_get_by_index(i);
        printf("User %d:\n", i);
        printf("  UID: %d\n", user->uid);
        printf("  GID: %d\n", user->gid);
        printf("  Name: %s\n", user->username);
        printf("  Umask: 0%03o\n", user->umask);
        printf("  Capabilities: 0x%08x\n", user->capabilities);
        printf("\n");
    }
}

#else

void users_print_database(void)
{
    /* Debug output disabled */
}

#endif /* LITTLEOS_DEBUG_USERS */
