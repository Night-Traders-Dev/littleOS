#include <stdio.h>
#include <string.h>
#include "script_storage.h"
#include "sage_embed.h"

// External sage context
extern sage_context_t* sage_ctx;

// Callback for listing scripts
static void list_callback(const char* name, size_t size) {
    printf("  - %s (%zu bytes)\r\n", name, size);
}

/**
 * @brief Save a script
 * Usage: save <name> <code>
 */
static void cmd_save(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: save <name> <code>\r\n");
        printf("Example: save blink \"print 'LED ON'\"\r\n");
        return;
    }
    
    const char* name = argv[1];
    
    // Reconstruct code from remaining arguments
    char code[512];
    int offset = 0;
    
    for (int i = 2; i < argc && offset < sizeof(code) - 2; i++) {
        int len = strlen(argv[i]);
        if (offset + len + 1 < sizeof(code)) {
            if (i > 2) {
                code[offset++] = ' ';
            }
            strcpy(code + offset, argv[i]);
            offset += len;
        }
    }
    code[offset] = '\0';
    
    // Remove quotes if present
    char* clean_code = code;
    int code_len = strlen(code);
    if (code_len > 2 && code[0] == '"' && code[code_len - 1] == '"') {
        clean_code = code + 1;
        code[code_len - 1] = '\0';
    }
    
    if (script_save(name, clean_code)) {
        printf("Script '%s' saved (%zu bytes)\r\n", name, strlen(clean_code));
    } else {
        printf("Error: Failed to save script '%s'\r\n", name);
    }
}

/**
 * @brief Load and execute a script
 * Usage: run <name>
 */
static void cmd_run(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: run <name>\r\n");
        return;
    }
    
    const char* name = argv[1];
    const char* code = script_load(name);
    
    if (!code) {
        printf("Error: Script '%s' not found\r\n", name);
        return;
    }
    
    printf("Running '%s'...\r\n", name);
    
    // Execute the script
    if (sage_ctx) {
        sage_result_t result = sage_eval_string(sage_ctx, code, strlen(code));
        if (result != SAGE_OK) {
            printf("Error: %s\r\n", sage_get_error(sage_ctx));
        }
    } else {
        printf("Error: SageLang not initialized\r\n");
    }
}

/**
 * @brief List all saved scripts
 * Usage: list
 */
static void cmd_list(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    int count = script_count();
    size_t memory = script_memory_used();
    
    if (count == 0) {
        printf("No scripts saved\r\n");
        return;
    }
    
    printf("Saved scripts (%d total, %zu bytes):\r\n", count, memory);
    script_list(list_callback);
}

/**
 * @brief Delete a script
 * Usage: delete <name>
 */
static void cmd_delete(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: delete <name>\r\n");
        return;
    }
    
    const char* name = argv[1];
    
    if (script_delete(name)) {
        printf("Script '%s' deleted\r\n", name);
    } else {
        printf("Error: Script '%s' not found\r\n", name);
    }
}

/**
 * @brief Show script contents
 * Usage: show <name>
 */
static void cmd_show(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: show <name>\r\n");
        return;
    }
    
    const char* name = argv[1];
    const char* code = script_load(name);
    
    if (!code) {
        printf("Error: Script '%s' not found\r\n", name);
        return;
    }
    
    printf("Script '%s':\r\n", name);
    printf("%s\r\n", code);
}

/**
 * @brief Clear all scripts
 * Usage: clear-scripts
 */
static void cmd_clear_scripts(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    int count = script_count();
    
    if (count == 0) {
        printf("No scripts to clear\r\n");
        return;
    }
    
    script_clear_all();
    printf("Cleared %d script(s)\r\n", count);
}

/**
 * @brief Main script command dispatcher
 */
int cmd_script(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Script commands:\r\n");
        printf("  save <name> <code>  - Save a script\r\n");
        printf("  run <name>          - Run a saved script\r\n");
        printf("  list                - List all scripts\r\n");
        printf("  show <name>         - Show script contents\r\n");
        printf("  delete <name>       - Delete a script\r\n");
        printf("  clear-scripts       - Delete all scripts\r\n");
        return 0;
    }
    
    const char* subcmd = argv[1];
    
    if (strcmp(subcmd, "save") == 0) {
        cmd_save(argc - 1, argv + 1);
    } else if (strcmp(subcmd, "run") == 0) {
        cmd_run(argc - 1, argv + 1);
    } else if (strcmp(subcmd, "list") == 0) {
        cmd_list(argc - 1, argv + 1);
    } else if (strcmp(subcmd, "show") == 0) {
        cmd_show(argc - 1, argv + 1);
    } else if (strcmp(subcmd, "delete") == 0) {
        cmd_delete(argc - 1, argv + 1);
    } else if (strcmp(subcmd, "clear-scripts") == 0) {
        cmd_clear_scripts(argc - 1, argv + 1);
    } else {
        printf("Unknown script command: %s\r\n", subcmd);
        printf("Type 'script' for help\r\n");
    }
    
    return 0;
}
