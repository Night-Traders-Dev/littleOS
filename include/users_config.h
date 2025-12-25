#ifndef LITTLEOS_USERS_CONFIG_H
#define LITTLEOS_USERS_CONFIG_H

/*
 * littleOS User Configuration System
 * 
 * Compile-time configuration for user accounts and privilege levels.
 * Configure users at build time via CMake or preprocessor defines.
 * 
 * Default: Root only (UID 0)
 * With config: Root + custom user account (UID 1000+)
 */

#include <stdint.h>
#include "permissions.h"

/* ============================================================================
 * Build-Time Configuration
 * ============================================================================
 * 
 * Define these before including this header, or set via CMake:
 * 
 * LITTLEOS_ENABLE_USER_ACCOUNT    - Enable non-root user account (default: 0)
 * LITTLEOS_USER_UID               - UID of custom user (default: 1000)
 * LITTLEOS_USER_NAME              - Username string (default: "user")
 * LITTLEOS_USER_UMASK             - Default umask for user (default: 0022)
 * LITTLEOS_USER_CAPABILITIES      - Capability flags (default: 0)
 * LITTLEOS_STARTUP_TASK_UID       - UID to run startup app as (default: 0)
 */

/* ============================================================================
 * Default Configuration Values
 * ============================================================================
 */

#ifndef LITTLEOS_ENABLE_USER_ACCOUNT
#define LITTLEOS_ENABLE_USER_ACCOUNT  0
#endif

#ifndef LITTLEOS_USER_UID
#define LITTLEOS_USER_UID             1000
#endif

#ifndef LITTLEOS_USER_NAME
#define LITTLEOS_USER_NAME            "user"
#endif

#ifndef LITTLEOS_USER_UMASK
#define LITTLEOS_USER_UMASK           0022
#endif

#ifndef LITTLEOS_USER_CAPABILITIES
#define LITTLEOS_USER_CAPABILITIES    0
#endif

#ifndef LITTLEOS_STARTUP_TASK_UID
#define LITTLEOS_STARTUP_TASK_UID     0  /* Root */
#endif

/* ============================================================================
 * User Account Database
 * ============================================================================
 */

typedef struct {
    uid_t uid;                      /* User ID */
    gid_t gid;                      /* Group ID */
    const char *username;           /* Username (informational) */
    uint16_t umask;                 /* Default file creation mask */
    uint32_t capabilities;          /* Linux-style capability flags */
} user_account_t;

/**
 * Get user account by UID
 * 
 * @param uid   User ID to look up
 * @return      Pointer to user account, or NULL if not found
 */
const user_account_t *users_get_by_uid(uid_t uid);

/**
 * Get user account by username
 * 
 * @param name  Username to look up
 * @return      Pointer to user account, or NULL if not found
 */
const user_account_t *users_get_by_name(const char *name);

/**
 * Initialize user database (call during boot)
 */
void users_init(void);

/**
 * Get total number of configured users
 */
uint16_t users_get_count(void);

/**
 * Get user account by index
 */
const user_account_t *users_get_by_index(uint16_t index);
const user_account_t *users_get_default_user(void);


/**
 * Check if a UID is valid (exists in user database)
 */
bool users_uid_exists(uid_t uid);

/**
 * Check if a username exists
 */
bool users_name_exists(const char *name);

/**
 * Create a security context from a user account
 * 
 * @param account   User account
 * @return          Initialized security context
 */
task_sec_ctx_t users_account_to_context(const user_account_t *account);
task_sec_ctx_t users_root_context(void);

void users_print_database(void);



#endif /* LITTLEOS_USERS_CONFIG_H */
