#ifndef LITTLEOS_SHELL_ENV_H
#define LITTLEOS_SHELL_ENV_H

/*
 * littleOS Shell Environment System
 *
 * Environment variables, aliases, and prompt customization
 * for the littleOS shell on RP2040 (264KB SRAM).
 *
 * Lightweight design:
 * - Static allocation only (no malloc)
 * - Bounded storage with compile-time limits
 * - $VAR and ${VAR} expansion in strings
 * - Recursive alias expansion with loop protection
 * - Customizable PS1 prompt with escape sequences
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration Limits
 * ============================================================================
 */

#define SHELL_ENV_MAX_VARS      16      /* Maximum environment variables */
#define SHELL_ENV_KEY_MAX       32      /* Max key length (including NUL) */
#define SHELL_ENV_VAL_MAX       64      /* Max value length (including NUL) */

#define SHELL_ALIAS_MAX         8       /* Maximum aliases */
#define SHELL_ALIAS_NAME_MAX    16      /* Max alias name length (including NUL) */
#define SHELL_ALIAS_EXPAND_MAX  128     /* Max alias expansion length (including NUL) */

#define SHELL_ALIAS_RECURSE_MAX 5       /* Max alias expansion depth */

#define SHELL_PS1_MAX           64      /* Max PS1 format string length */
#define SHELL_PROMPT_MAX        128     /* Max rendered prompt length */

/* ============================================================================
 * Default Environment Variables
 * ============================================================================
 */

#define SHELL_ENV_DEFAULT_USER      "root"
#define SHELL_ENV_DEFAULT_HOME      "/"
#define SHELL_ENV_DEFAULT_HOSTNAME  "littleos"
#define SHELL_ENV_DEFAULT_PS1       "\\u@\\h:\\w\\$ "
#define SHELL_ENV_DEFAULT_PATH      "/bin"

/* ============================================================================
 * Environment Variable API
 * ============================================================================
 */

/**
 * Initialize the environment subsystem.
 * Sets default variables: USER, HOME, HOSTNAME, PS1, PATH.
 */
void shell_env_init(void);

/**
 * Set an environment variable.
 *
 * @param key   Variable name (max SHELL_ENV_KEY_MAX-1 chars)
 * @param val   Variable value (max SHELL_ENV_VAL_MAX-1 chars)
 * @return true if set successfully, false if storage full or key/val too long
 */
bool shell_env_set(const char *key, const char *val);

/**
 * Get the value of an environment variable.
 *
 * @param key   Variable name to look up
 * @return Pointer to value string, or NULL if not found
 */
const char *shell_env_get(const char *key);

/**
 * Remove an environment variable.
 *
 * @param key   Variable name to remove
 * @return true if removed, false if not found
 */
bool shell_env_unset(const char *key);

/**
 * List all environment variables to stdout.
 * Format: KEY=VALUE (one per line, \r\n terminated)
 */
void shell_env_list(void);

/**
 * Expand environment variables in a string.
 * Supports $VAR and ${VAR} syntax.
 * Undefined variables expand to empty string.
 *
 * @param input     Input string with $VAR references
 * @param output    Output buffer for expanded string
 * @param outlen    Size of output buffer
 * @return true if expansion succeeded, false if output truncated
 */
bool shell_env_expand(const char *input, char *output, size_t outlen);

/* ============================================================================
 * Alias API
 * ============================================================================
 */

/**
 * Set a shell alias.
 *
 * @param name      Alias name (max SHELL_ALIAS_NAME_MAX-1 chars)
 * @param expansion Command string to expand to (max SHELL_ALIAS_EXPAND_MAX-1 chars)
 * @return true if set successfully, false if storage full or name/expansion too long
 */
bool shell_alias_set(const char *name, const char *expansion);

/**
 * Get the expansion for an alias.
 *
 * @param name  Alias name to look up
 * @return Pointer to expansion string, or NULL if not found
 */
const char *shell_alias_get(const char *name);

/**
 * Remove a shell alias.
 *
 * @param name  Alias name to remove
 * @return true if removed, false if not found
 */
bool shell_alias_unset(const char *name);

/**
 * List all aliases to stdout.
 * Format: alias NAME='EXPANSION' (one per line, \r\n terminated)
 */
void shell_alias_list(void);

/**
 * Expand aliases in a command line.
 * Only the first token is checked for alias expansion.
 * Recursive expansion up to SHELL_ALIAS_RECURSE_MAX depth.
 *
 * @param input     Input command line
 * @param output    Output buffer for expanded command
 * @param outlen    Size of output buffer
 * @return true if expansion succeeded, false if output truncated or loop detected
 */
bool shell_alias_expand(const char *input, char *output, size_t outlen);

/* ============================================================================
 * Prompt API
 * ============================================================================
 *
 * Prompt format escape sequences (PS1):
 *   \u  - Current username (from USER env var)
 *   \h  - Hostname (from HOSTNAME env var)
 *   \w  - Current working directory (from PWD or HOME env var)
 *   \$  - '#' if root (UID 0), '$' for normal users
 *
 * ANSI color codes can be used directly in PS1, e.g.:
 *   \033[32m\u@\h\033[0m:\033[34m\w\033[0m\$
 */

/**
 * Set the prompt format string (PS1).
 *
 * @param format    PS1 format string (max SHELL_PS1_MAX-1 chars)
 * @return true if set, false if too long
 */
bool shell_prompt_set(const char *format);

/**
 * Render the current prompt into a buffer.
 * Expands \u, \h, \w, \$ escape sequences.
 *
 * @param output    Output buffer for rendered prompt
 * @param outlen    Size of output buffer
 * @return true if rendered successfully
 */
bool shell_prompt_render(char *output, size_t outlen);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_SHELL_ENV_H */
