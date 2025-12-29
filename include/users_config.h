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
 *
 * ==== CRITICAL: CMake-generated configuration ====
 * These MUST be defined by CMakeLists.txt via add_compile_definitions()
 * OR passed via -D flags to ensure they propagate to all compilation units.
 *
 * ==== INCLUDE ORDER (IMPORTANT!) ====
 * permissions.h MUST be included FIRST to define our types.
 * Then we include standard headers.
 * Do NOT include <sys/types.h> directly - permissions.h does that with proper guards.
 */

#include "permissions.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================================
 * Build-Time Configuration (CMake-provided)
 * ============================================================================
 *
 * CMakeLists.txt MUST set these via add_compile_definitions():
 *
 * if(LITTLEOS_ENABLE_USER_ACCOUNT)
 *   add_compile_definitions(
 *     LITTLEOS_ENABLE_USER_ACCOUNT=1
 *     LITTLEOS_USER_UID=${LITTLEOS_USER_UID}
 *     LITTLEOS_USER_NAME="${LITTLEOS_USER_NAME}"
 *     LITTLEOS_USER_UMASK=${LITTLEOS_USER_UMASK}
 *     LITTLEOS_USER_CAPABILITIES=${LITTLEOS_USER_CAPABILITIES}
 *   )
 * else()
 *   add_compile_definitions(LITTLEOS_ENABLE_USER_ACCOUNT=0)
 * endif()
 */

/* ============================================================================
 * Fallback Defaults (only if NOT set by CMake)
 * ============================================================================
 */

#ifndef LITTLEOS_ENABLE_USER_ACCOUNT
#define LITTLEOS_ENABLE_USER_ACCOUNT 0
#endif

#ifndef LITTLEOS_USER_UID
#define LITTLEOS_USER_UID 1000
#endif

#ifndef LITTLEOS_USER_NAME
#define LITTLEOS_USER_NAME "user"
#endif

#ifndef LITTLEOS_USER_UMASK
#define LITTLEOS_USER_UMASK 0022
#endif

#ifndef LITTLEOS_USER_CAPABILITIES
#define LITTLEOS_USER_CAPABILITIES 0
#endif

#ifndef LITTLEOS_STARTUP_TASK_UID
#define LITTLEOS_STARTUP_TASK_UID 0 /* Root */
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
 * @param uid User ID to look up
 * @return Pointer to user account, or NULL if not found
 */
const user_account_t *users_get_by_uid(uid_t uid);

/**
 * Get user account by username
 *
 * @param name Username to look up
 * @return Pointer to user account, or NULL if not found
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

/**
 * Get default non-root user account (if configured)
 * Returns NULL if only root is configured
 */
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
 * @param account User account
 * @return Initialized security context
 */
task_sec_ctx_t users_account_to_context(const user_account_t *account);

/**
 * Create root security context
 */
task_sec_ctx_t users_root_context(void);

/**
 * Get root user account
 */
const user_account_t *users_get_root(void);

/**
 * Print user account database (for debugging)
 * Only available if LITTLEOS_DEBUG_USERS is defined
 */
void users_print_database(void);

/**
 * Print configured build-time settings (debugging)
 */
void users_print_build_config(void);

#endif /* LITTLEOS_USERS_CONFIG_H */
