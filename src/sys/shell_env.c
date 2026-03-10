#include "shell_env.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/*
 * littleOS Shell Environment Implementation
 *
 * Static storage for environment variables, aliases, and prompt rendering.
 * All memory is statically allocated to avoid heap usage on RP2040.
 */

/* ============================================================================
 * Environment Variable Storage
 * ============================================================================
 */

typedef struct {
    char key[SHELL_ENV_KEY_MAX];
    char val[SHELL_ENV_VAL_MAX];
    bool active;
} env_var_t;

static env_var_t env_vars[SHELL_ENV_MAX_VARS];
static int env_count = 0;

/* ============================================================================
 * Alias Storage
 * ============================================================================
 */

typedef struct {
    char name[SHELL_ALIAS_NAME_MAX];
    char expansion[SHELL_ALIAS_EXPAND_MAX];
    bool active;
} alias_entry_t;

static alias_entry_t aliases[SHELL_ALIAS_MAX];
static int alias_count = 0;

/* ============================================================================
 * Environment Variable Implementation
 * ============================================================================
 */

/* Find index of env var by key, returns -1 if not found */
static int env_find(const char *key) {
    if (!key) return -1;
    for (int i = 0; i < SHELL_ENV_MAX_VARS; i++) {
        if (env_vars[i].active && strcmp(env_vars[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

void shell_env_init(void) {
    /* Clear all variables */
    memset(env_vars, 0, sizeof(env_vars));
    env_count = 0;

    /* Clear all aliases */
    memset(aliases, 0, sizeof(aliases));
    alias_count = 0;

    /* Set default environment variables */
    shell_env_set("USER",     SHELL_ENV_DEFAULT_USER);
    shell_env_set("HOME",     SHELL_ENV_DEFAULT_HOME);
    shell_env_set("HOSTNAME", SHELL_ENV_DEFAULT_HOSTNAME);
    shell_env_set("PS1",      SHELL_ENV_DEFAULT_PS1);
    shell_env_set("PATH",     SHELL_ENV_DEFAULT_PATH);
    shell_env_set("PWD",      "/");
}

bool shell_env_set(const char *key, const char *val) {
    if (!key || !val) return false;
    if (strlen(key) >= SHELL_ENV_KEY_MAX) return false;
    if (strlen(val) >= SHELL_ENV_VAL_MAX) return false;

    /* Update existing variable if found */
    int idx = env_find(key);
    if (idx >= 0) {
        strncpy(env_vars[idx].val, val, SHELL_ENV_VAL_MAX - 1);
        env_vars[idx].val[SHELL_ENV_VAL_MAX - 1] = '\0';
        return true;
    }

    /* Find a free slot */
    for (int i = 0; i < SHELL_ENV_MAX_VARS; i++) {
        if (!env_vars[i].active) {
            strncpy(env_vars[i].key, key, SHELL_ENV_KEY_MAX - 1);
            env_vars[i].key[SHELL_ENV_KEY_MAX - 1] = '\0';
            strncpy(env_vars[i].val, val, SHELL_ENV_VAL_MAX - 1);
            env_vars[i].val[SHELL_ENV_VAL_MAX - 1] = '\0';
            env_vars[i].active = true;
            env_count++;
            return true;
        }
    }

    return false; /* Storage full */
}

const char *shell_env_get(const char *key) {
    int idx = env_find(key);
    if (idx >= 0) {
        return env_vars[idx].val;
    }
    return NULL;
}

bool shell_env_unset(const char *key) {
    int idx = env_find(key);
    if (idx >= 0) {
        env_vars[idx].active = false;
        env_vars[idx].key[0] = '\0';
        env_vars[idx].val[0] = '\0';
        env_count--;
        return true;
    }
    return false;
}

void shell_env_list(void) {
    for (int i = 0; i < SHELL_ENV_MAX_VARS; i++) {
        if (env_vars[i].active) {
            printf("%s=%s\r\n", env_vars[i].key, env_vars[i].val);
        }
    }
}

/* Helper: check if character is valid in a variable name */
static bool is_var_char(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

bool shell_env_expand(const char *input, char *output, size_t outlen) {
    if (!input || !output || outlen == 0) return false;

    size_t oi = 0;  /* output index */
    const char *p = input;

    while (*p && oi < outlen - 1) {
        if (*p == '$') {
            p++;
            char varname[SHELL_ENV_KEY_MAX];
            int vi = 0;

            if (*p == '{') {
                /* ${VAR} syntax */
                p++;
                while (*p && *p != '}' && vi < SHELL_ENV_KEY_MAX - 1) {
                    varname[vi++] = *p++;
                }
                if (*p == '}') p++;
            } else {
                /* $VAR syntax */
                while (*p && is_var_char(*p) && vi < SHELL_ENV_KEY_MAX - 1) {
                    varname[vi++] = *p++;
                }
            }
            varname[vi] = '\0';

            if (vi > 0) {
                const char *val = shell_env_get(varname);
                if (val) {
                    size_t vlen = strlen(val);
                    if (oi + vlen >= outlen) {
                        /* Truncate */
                        size_t remaining = outlen - 1 - oi;
                        memcpy(output + oi, val, remaining);
                        oi += remaining;
                        break;
                    }
                    memcpy(output + oi, val, vlen);
                    oi += vlen;
                }
                /* Undefined vars expand to empty string */
            } else {
                /* Bare '$' with no var name - copy literally */
                if (oi < outlen - 1) {
                    output[oi++] = '$';
                }
            }
        } else {
            output[oi++] = *p++;
        }
    }

    output[oi] = '\0';
    return (*p == '\0'); /* true if we consumed all input */
}

/* ============================================================================
 * Alias Implementation
 * ============================================================================
 */

/* Find index of alias by name, returns -1 if not found */
static int alias_find(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < SHELL_ALIAS_MAX; i++) {
        if (aliases[i].active && strcmp(aliases[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

bool shell_alias_set(const char *name, const char *expansion) {
    if (!name || !expansion) return false;
    if (strlen(name) >= SHELL_ALIAS_NAME_MAX) return false;
    if (strlen(expansion) >= SHELL_ALIAS_EXPAND_MAX) return false;

    /* Update existing alias if found */
    int idx = alias_find(name);
    if (idx >= 0) {
        strncpy(aliases[idx].expansion, expansion, SHELL_ALIAS_EXPAND_MAX - 1);
        aliases[idx].expansion[SHELL_ALIAS_EXPAND_MAX - 1] = '\0';
        return true;
    }

    /* Find a free slot */
    for (int i = 0; i < SHELL_ALIAS_MAX; i++) {
        if (!aliases[i].active) {
            strncpy(aliases[i].name, name, SHELL_ALIAS_NAME_MAX - 1);
            aliases[i].name[SHELL_ALIAS_NAME_MAX - 1] = '\0';
            strncpy(aliases[i].expansion, expansion, SHELL_ALIAS_EXPAND_MAX - 1);
            aliases[i].expansion[SHELL_ALIAS_EXPAND_MAX - 1] = '\0';
            aliases[i].active = true;
            alias_count++;
            return true;
        }
    }

    return false; /* Storage full */
}

const char *shell_alias_get(const char *name) {
    int idx = alias_find(name);
    if (idx >= 0) {
        return aliases[idx].expansion;
    }
    return NULL;
}

bool shell_alias_unset(const char *name) {
    int idx = alias_find(name);
    if (idx >= 0) {
        aliases[idx].active = false;
        aliases[idx].name[0] = '\0';
        aliases[idx].expansion[0] = '\0';
        alias_count--;
        return true;
    }
    return false;
}

void shell_alias_list(void) {
    for (int i = 0; i < SHELL_ALIAS_MAX; i++) {
        if (aliases[i].active) {
            printf("alias %s='%s'\r\n", aliases[i].name, aliases[i].expansion);
        }
    }
}

bool shell_alias_expand(const char *input, char *output, size_t outlen) {
    if (!input || !output || outlen == 0) return false;

    /* Work buffer for intermediate expansions */
    char buf_a[512];
    char buf_b[512];
    char *current = buf_a;
    char *next = buf_b;

    strncpy(current, input, sizeof(buf_a) - 1);
    current[sizeof(buf_a) - 1] = '\0';

    for (int depth = 0; depth < SHELL_ALIAS_RECURSE_MAX; depth++) {
        /* Extract first token from current */
        char first_token[SHELL_ALIAS_NAME_MAX];
        const char *rest = current;
        int ti = 0;

        /* Skip leading whitespace */
        while (*rest == ' ' || *rest == '\t') rest++;

        /* Extract first word */
        const char *word_start = rest;
        while (*rest && *rest != ' ' && *rest != '\t' && ti < SHELL_ALIAS_NAME_MAX - 1) {
            first_token[ti++] = *rest++;
        }
        first_token[ti] = '\0';

        if (ti == 0) break; /* Empty input */

        /* Look up alias */
        const char *expansion = shell_alias_get(first_token);
        if (!expansion) break; /* No alias match, done */

        /* Build expanded command: expansion + rest of input */
        int written = snprintf(next, 512, "%s%s", expansion, rest);
        if (written < 0 || written >= 512) {
            /* Expansion too long, use unexpanded */
            break;
        }

        /* Swap buffers */
        char *tmp = current;
        current = next;
        next = tmp;
    }

    /* Copy result to output */
    strncpy(output, current, outlen - 1);
    output[outlen - 1] = '\0';
    return true;
}

/* ============================================================================
 * Prompt Implementation
 * ============================================================================
 */

bool shell_prompt_set(const char *format) {
    if (!format) return false;
    if (strlen(format) >= SHELL_PS1_MAX) return false;
    return shell_env_set("PS1", format);
}

bool shell_prompt_render(char *output, size_t outlen) {
    if (!output || outlen == 0) return false;

    const char *ps1 = shell_env_get("PS1");
    if (!ps1) {
        ps1 = SHELL_ENV_DEFAULT_PS1;
    }

    const char *user = shell_env_get("USER");
    if (!user) user = "?";

    const char *hostname = shell_env_get("HOSTNAME");
    if (!hostname) hostname = "?";

    const char *cwd = shell_env_get("PWD");
    if (!cwd) {
        cwd = shell_env_get("HOME");
        if (!cwd) cwd = "/";
    }

    /* Determine if root: USER == "root" */
    bool is_root = (strcmp(user, "root") == 0);

    size_t oi = 0;
    const char *p = ps1;

    while (*p && oi < outlen - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case 'u': {
                /* Username */
                size_t len = strlen(user);
                if (oi + len < outlen) {
                    memcpy(output + oi, user, len);
                    oi += len;
                }
                break;
            }
            case 'h': {
                /* Hostname */
                size_t len = strlen(hostname);
                if (oi + len < outlen) {
                    memcpy(output + oi, hostname, len);
                    oi += len;
                }
                break;
            }
            case 'w': {
                /* Working directory */
                size_t len = strlen(cwd);
                if (oi + len < outlen) {
                    memcpy(output + oi, cwd, len);
                    oi += len;
                }
                break;
            }
            case '$': {
                /* Root indicator */
                if (oi < outlen - 1) {
                    output[oi++] = is_root ? '#' : '$';
                }
                break;
            }
            case '\\': {
                /* Literal backslash */
                if (oi < outlen - 1) {
                    output[oi++] = '\\';
                }
                break;
            }
            case '0': {
                /* ANSI escape: \033 -> ESC character */
                if (*(p + 1) == '3' && *(p + 2) == '3') {
                    if (oi < outlen - 1) {
                        output[oi++] = '\033';
                    }
                    p += 2; /* Skip past '33' */
                } else {
                    output[oi++] = '\\';
                    output[oi++] = *p;
                }
                break;
            }
            default:
                /* Unknown escape, copy literally */
                if (oi + 1 < outlen) {
                    output[oi++] = '\\';
                    output[oi++] = *p;
                }
                break;
            }
            p++;
        } else {
            output[oi++] = *p++;
        }
    }

    output[oi] = '\0';
    return true;
}
