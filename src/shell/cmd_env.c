// src/shell/cmd_env.c
#include <stdio.h>
#include <string.h>
#include "shell_env.h"

/*
 * Shell commands for environment variables, aliases, and prompt customization.
 *
 * Commands:
 *   env     - Manage environment variables
 *   alias   - Manage command aliases
 *   export  - Shorthand for setting environment variables
 *   prompt  - Configure PS1 prompt format
 */

/* ============================================================================
 * env command - Environment variable management
 * ============================================================================
 *
 * Usage:
 *   env                  - List all variables
 *   env list             - List all variables
 *   env get KEY          - Get value of KEY
 *   env set KEY=VALUE    - Set KEY to VALUE
 *   env unset KEY        - Remove KEY
 */
int cmd_env(int argc, char *argv[]) {
    if (argc < 2) {
        /* No subcommand: list all */
        shell_env_list();
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        shell_env_list();
        return 0;
    }

    if (strcmp(argv[1], "get") == 0) {
        if (argc < 3) {
            printf("Usage: env get <KEY>\r\n");
            return 1;
        }
        const char *val = shell_env_get(argv[2]);
        if (val) {
            printf("%s=%s\r\n", argv[2], val);
        } else {
            printf("env: '%s' not set\r\n", argv[2]);
        }
        return val ? 0 : 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) {
            printf("Usage: env set KEY=VALUE\r\n");
            return 1;
        }

        /* Parse KEY=VALUE from argv[2] (and possibly argv[3..] for values with spaces) */
        char combined[SHELL_ENV_KEY_MAX + SHELL_ENV_VAL_MAX + 2];
        combined[0] = '\0';

        /* Reassemble from argv[2] onward */
        for (int i = 2; i < argc; i++) {
            if (i > 2) {
                strncat(combined, " ", sizeof(combined) - strlen(combined) - 1);
            }
            strncat(combined, argv[i], sizeof(combined) - strlen(combined) - 1);
        }

        char *eq = strchr(combined, '=');
        if (!eq) {
            printf("Usage: env set KEY=VALUE\r\n");
            return 1;
        }

        *eq = '\0';
        const char *key = combined;
        const char *val = eq + 1;

        if (strlen(key) == 0) {
            printf("env: empty key\r\n");
            return 1;
        }

        if (shell_env_set(key, val)) {
            printf("%s=%s\r\n", key, val);
            return 0;
        } else {
            printf("env: failed to set '%s' (storage full or value too long)\r\n", key);
            return 1;
        }
    }

    if (strcmp(argv[1], "unset") == 0) {
        if (argc < 3) {
            printf("Usage: env unset <KEY>\r\n");
            return 1;
        }
        if (shell_env_unset(argv[2])) {
            printf("env: unset '%s'\r\n", argv[2]);
            return 0;
        } else {
            printf("env: '%s' not set\r\n", argv[2]);
            return 1;
        }
    }

    /* Unknown subcommand */
    printf("Usage: env <list|get|set|unset>\r\n");
    printf("\r\nSubcommands:\r\n");
    printf("  list            - List all environment variables\r\n");
    printf("  get KEY         - Get value of KEY\r\n");
    printf("  set KEY=VALUE   - Set KEY to VALUE\r\n");
    printf("  unset KEY       - Remove KEY\r\n");
    return 1;
}

/* ============================================================================
 * alias command - Alias management
 * ============================================================================
 *
 * Usage:
 *   alias                    - List all aliases
 *   alias list               - List all aliases
 *   alias set NAME=COMMAND   - Set alias
 *   alias unset NAME         - Remove alias
 */
int cmd_alias(int argc, char *argv[]) {
    if (argc < 2) {
        /* No subcommand: list all */
        shell_alias_list();
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        shell_alias_list();
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) {
            printf("Usage: alias set NAME=COMMAND\r\n");
            return 1;
        }

        /* Reassemble everything after "alias set " */
        char combined[SHELL_ALIAS_NAME_MAX + SHELL_ALIAS_EXPAND_MAX + 2];
        combined[0] = '\0';

        for (int i = 2; i < argc; i++) {
            if (i > 2) {
                strncat(combined, " ", sizeof(combined) - strlen(combined) - 1);
            }
            strncat(combined, argv[i], sizeof(combined) - strlen(combined) - 1);
        }

        char *eq = strchr(combined, '=');
        if (!eq) {
            printf("Usage: alias set NAME=COMMAND\r\n");
            return 1;
        }

        *eq = '\0';
        const char *name = combined;
        const char *expansion = eq + 1;

        if (strlen(name) == 0) {
            printf("alias: empty name\r\n");
            return 1;
        }

        if (shell_alias_set(name, expansion)) {
            printf("alias %s='%s'\r\n", name, expansion);
            return 0;
        } else {
            printf("alias: failed to set '%s' (storage full or value too long)\r\n", name);
            return 1;
        }
    }

    if (strcmp(argv[1], "unset") == 0) {
        if (argc < 3) {
            printf("Usage: alias unset <NAME>\r\n");
            return 1;
        }
        if (shell_alias_unset(argv[2])) {
            printf("alias: removed '%s'\r\n", argv[2]);
            return 0;
        } else {
            printf("alias: '%s' not defined\r\n", argv[2]);
            return 1;
        }
    }

    /* Unknown subcommand */
    printf("Usage: alias <list|set|unset>\r\n");
    printf("\r\nSubcommands:\r\n");
    printf("  list               - List all aliases\r\n");
    printf("  set NAME=COMMAND   - Set alias NAME to COMMAND\r\n");
    printf("  unset NAME         - Remove alias NAME\r\n");
    return 1;
}

/* ============================================================================
 * export command - Shorthand for env set
 * ============================================================================
 *
 * Usage:
 *   export KEY=VALUE    - Set environment variable (like bash export)
 *   export              - List all variables
 */
int cmd_export(int argc, char *argv[]) {
    if (argc < 2) {
        /* No args: list all, similar to bash export without args */
        shell_env_list();
        return 0;
    }

    /* Reassemble all arguments after "export" */
    char combined[SHELL_ENV_KEY_MAX + SHELL_ENV_VAL_MAX + 2];
    combined[0] = '\0';

    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strncat(combined, " ", sizeof(combined) - strlen(combined) - 1);
        }
        strncat(combined, argv[i], sizeof(combined) - strlen(combined) - 1);
    }

    char *eq = strchr(combined, '=');
    if (!eq) {
        /* No '=': try to display the variable */
        const char *val = shell_env_get(combined);
        if (val) {
            printf("%s=%s\r\n", combined, val);
            return 0;
        } else {
            printf("export: '%s' not set\r\n", combined);
            return 1;
        }
    }

    *eq = '\0';
    const char *key = combined;
    const char *val = eq + 1;

    if (strlen(key) == 0) {
        printf("Usage: export KEY=VALUE\r\n");
        return 1;
    }

    if (shell_env_set(key, val)) {
        printf("%s=%s\r\n", key, val);
        return 0;
    } else {
        printf("export: failed to set '%s' (storage full or value too long)\r\n", key);
        return 1;
    }
}

/* ============================================================================
 * prompt command - PS1 prompt customization
 * ============================================================================
 *
 * Usage:
 *   prompt              - Show current PS1 format
 *   prompt set FORMAT   - Set PS1 format string
 *   prompt reset        - Reset PS1 to default
 *   prompt help         - Show format escape sequences
 */
int cmd_prompt(int argc, char *argv[]) {
    if (argc < 2) {
        /* Show current PS1 */
        const char *ps1 = shell_env_get("PS1");
        if (ps1) {
            printf("PS1=%s\r\n", ps1);
        } else {
            printf("PS1 not set (using default)\r\n");
        }

        /* Show rendered preview */
        char rendered[SHELL_PROMPT_MAX];
        if (shell_prompt_render(rendered, sizeof(rendered))) {
            printf("Rendered: %s\r\n", rendered);
        }
        return 0;
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) {
            printf("Usage: prompt set <FORMAT>\r\n");
            printf("Example: prompt set \\u@\\h:\\w\\$ \r\n");
            return 1;
        }

        /* Reassemble format string from argv[2..] */
        char format[SHELL_PS1_MAX];
        format[0] = '\0';

        for (int i = 2; i < argc; i++) {
            if (i > 2) {
                strncat(format, " ", sizeof(format) - strlen(format) - 1);
            }
            strncat(format, argv[i], sizeof(format) - strlen(format) - 1);
        }

        if (shell_prompt_set(format)) {
            printf("PS1=%s\r\n", format);

            /* Show preview */
            char rendered[SHELL_PROMPT_MAX];
            if (shell_prompt_render(rendered, sizeof(rendered))) {
                printf("Preview: %s\r\n", rendered);
            }
            return 0;
        } else {
            printf("prompt: format string too long (max %d chars)\r\n",
                   SHELL_PS1_MAX - 1);
            return 1;
        }
    }

    if (strcmp(argv[1], "reset") == 0) {
        shell_prompt_set(SHELL_ENV_DEFAULT_PS1);
        printf("PS1 reset to default: %s\r\n", SHELL_ENV_DEFAULT_PS1);
        return 0;
    }

    if (strcmp(argv[1], "help") == 0) {
        printf("Prompt Format Escapes:\r\n");
        printf("  \\u  - Username (USER env var)\r\n");
        printf("  \\h  - Hostname (HOSTNAME env var)\r\n");
        printf("  \\w  - Working directory (PWD env var)\r\n");
        printf("  \\$  - '#' for root, '$' for normal user\r\n");
        printf("  \\\\  - Literal backslash\r\n");
        printf("  \\033[NNm - ANSI color code\r\n");
        printf("\r\nExamples:\r\n");
        printf("  \\u@\\h:\\w\\$            - user@host:/path$ \r\n");
        printf("  [\\u] \\w\\$             - [user] /path$ \r\n");
        printf("  \\033[32m\\u\\033[0m:\\w\\$ - colored username\r\n");
        printf("\r\nDefault: %s\r\n", SHELL_ENV_DEFAULT_PS1);
        return 0;
    }

    /* Unknown subcommand */
    printf("Usage: prompt <set|reset|help>\r\n");
    printf("\r\nSubcommands:\r\n");
    printf("  (no args)     - Show current PS1 and preview\r\n");
    printf("  set FORMAT    - Set PS1 format string\r\n");
    printf("  reset         - Reset to default prompt\r\n");
    printf("  help          - Show format escape sequences\r\n");
    return 1;
}
