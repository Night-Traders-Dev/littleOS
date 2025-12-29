
// users_config.c - littleOS User Account Database
//
// Compile-time generated user accounts stored in ROM.
// All configuration comes from CMake via preprocessor defines.

#include "users_config.h"
#include <stdio.h>

/* ============================================================================
 * User Account Database (Generated at Compile Time)
 * ============================================================================
 *
 * All users are statically initialized based on CMake configuration.
 * Macro stringification ensures CMake values are properly converted.
 */

/* Macro stringification for converting CMake values to strings */
#define STRINGIFY(x) #x
#define STRINGIFY_VALUE(x) STRINGIFY(x)

/* ============================================================================
 * Root User Account (Always Present)
 * ============================================================================
 */

static const user_account_t root_account = {
    .uid = 0,                           /* Root UID */
    .gid = 0,                           /* Root GID */
    .username = "root",                 /* Root username */
    .umask = 0022,                      /* Standard umask */
    .capabilities = CAP_ALL             /* All capabilities */
};

/* ============================================================================
 * Custom User Account (Conditionally Included)
 * ============================================================================
 *
 * If LITTLEOS_ENABLE_USER_ACCOUNT is 1, include the custom user account
 * with values from CMake configuration.
 */

#if LITTLEOS_ENABLE_USER_ACCOUNT == 1

static const user_account_t custom_user_account = {
    .uid = LITTLEOS_USER_UID,                   /* From CMake */
    .gid = 100,                                 /* GID_USERS (standard) */
    .username = STRINGIFY_VALUE(LITTLEOS_USER_NAME),  /* From CMake (stringified) */
    .umask = LITTLEOS_USER_UMASK,               /* From CMake */
    .capabilities = LITTLEOS_USER_CAPABILITIES  /* From CMake */
};

/* User account array with both root and custom user */
static const user_account_t *user_accounts[] = {
    &root_account,
    &custom_user_account
};

static const uint16_t user_accounts_count = 2;

#else  /* LITTLEOS_ENABLE_USER_ACCOUNT == 0 */

/* User account array with only root */
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
    /* Database is statically initialized at compile-time */
    /* All configuration comes from CMake via preprocessor defines */
    /* Nothing dynamic to initialize at runtime */
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
    return users_get_by_uid(0);
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

bool users_uid_exists(uid_t uid)
{
    return users_get_by_uid(uid) != NULL;
}

bool users_name_exists(const char *name)
{
    return users_get_by_name(name) != NULL;
}

/* ============================================================================
 * Debug / Informational Functions
 * ============================================================================
 */

/**
 * Print user account database (for debugging)
 * Only available if LITTLEOS_DEBUG_USERS is defined
 */
#ifdef LITTLEOS_DEBUG_USERS

void users_print_database(void)
{
    printf("=== littleOS User Database ===\n");
    printf("Total users: %u\n\n", users_get_count());

    for (uint16_t i = 0; i < users_get_count(); i++) {
        const user_account_t *user = users_get_by_index(i);
        printf("User %u:\n", i);
        printf("  UID: %u\n", user->uid);
        printf("  GID: %u\n", user->gid);
        printf("  Name: %s\n", user->username);
        printf("  Umask: 0%03o\n", user->umask);
        printf("  Capabilities: 0x%08x\n", user->capabilities);
        printf("\n");
    }
}

#else  /* LITTLEOS_DEBUG_USERS not defined */

void users_print_database(void)
{
    /* Debug output disabled at compile time */
}

#endif /* LITTLEOS_DEBUG_USERS */

/**
 * Print build-time configuration (for debugging)
 * Shows what was actually compiled in
 */
void users_print_build_config(void)
{
    printf("=== littleOS User Configuration (Build-Time) ===\n");
    printf("LITTLEOS_ENABLE_USER_ACCOUNT=%d\n", LITTLEOS_ENABLE_USER_ACCOUNT);

#if LITTLEOS_ENABLE_USER_ACCOUNT == 1
    printf("LITTLEOS_USER_UID=%u\n", LITTLEOS_USER_UID);
    printf("LITTLEOS_USER_NAME=%s\n", STRINGIFY_VALUE(LITTLEOS_USER_NAME));
    printf("LITTLEOS_USER_UMASK=0%03o\n", LITTLEOS_USER_UMASK);
    printf("LITTLEOS_USER_CAPABILITIES=0x%08x\n", LITTLEOS_USER_CAPABILITIES);
    printf("\nStatus: Custom user account ENABLED\n");
#else
    printf("Status: Root-only (custom user DISABLED)\n");
#endif

    printf("LITTLEOS_STARTUP_TASK_UID=%u\n", LITTLEOS_STARTUP_TASK_UID);
    printf("\n");
}