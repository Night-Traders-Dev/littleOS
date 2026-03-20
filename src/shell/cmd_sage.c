#include <stdio.h>
#include <string.h>
#include "sage_embed.h"
#include "linter.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

// External global sage context (defined in kernel.c)
extern sage_context_t* sage_ctx;

/**
 * @brief Handle 'sage' command from shell
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, non-zero on error
 */
int cmd_sage(int argc, char* argv[]) {
    if (!sage_ctx) {
#ifdef PICO_BUILD
        printf("SageLang not initialized\r\n");
#else
        printf("SageLang not initialized\n");
#endif
        return 1;
    }
    
    sage_result_t result = SAGE_OK;
    
    if (argc == 1) {
        // No arguments - start REPL
        result = sage_repl(sage_ctx);
    } else if (argc == 2) {
        // One argument
        const char* arg = argv[1];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            // Print help
#ifdef PICO_BUILD
            printf("Usage: sage [options] [code]\r\n");
            printf("\r\nOptions:\r\n");
            printf("  (no args)        Start interactive REPL\r\n");
            printf("  -e, --eval CODE  Evaluate inline code\r\n");
            printf("  -m, --mem        Show memory statistics\r\n");
            printf("  -l, --lint CODE  Lint code for issues\r\n");
            printf("  -h, --help       Show this help\r\n");
            printf("\r\nEmbedded mode (bytecode VM + AST interpreter)\r\n");
#else
            printf("Usage: sage [options] [script.sage]\n");
            printf("\nOptions:\n");
            printf("  (no args)        Start interactive REPL\n");
            printf("  script.sage      Execute SageLang file\n");
            printf("  -e, --eval CODE  Evaluate inline code\n");
            printf("  -m, --mem        Show memory statistics\n");
            printf("  -l, --lint CODE  Lint code for issues\n");
            printf("  -h, --help       Show this help\n");
            printf("\nPC mode (bytecode VM + AST interpreter)\n");
#endif
            return 0;
        }

        if (strcmp(arg, "-m") == 0 || strcmp(arg, "--mem") == 0) {
            size_t allocated, objects;
            sage_get_memory_stats(sage_ctx, &allocated, &objects);
#ifdef PICO_BUILD
            printf("SageLang Memory:\r\n");
            printf("  Allocated: %u bytes\r\n", (unsigned)allocated);
            printf("  Objects: %u\r\n", (unsigned)objects);
#else
            printf("SageLang Memory:\n");
            printf("  Allocated: %zu bytes\n", allocated);
            printf("  Objects: %zu\n", objects);
#endif
            return 0;
        }

#ifdef PICO_BUILD
        // On embedded, treat argument as code to evaluate
        result = sage_eval_string(sage_ctx, arg, strlen(arg));
        if (result != SAGE_OK) {
            printf("Error: %s\r\n", sage_get_error(sage_ctx));
        }
#else
        // On PC, check if it's a file or code
        if (strstr(arg, ".sage") != NULL) {
            // Has .sage extension - treat as file
            result = sage_eval_file(sage_ctx, arg);
            if (result != SAGE_OK) {
                printf("Error: %s\n", sage_get_error(sage_ctx));
            }
        } else {
            // No extension - treat as code
            result = sage_eval_string(sage_ctx, arg, strlen(arg));
            if (result != SAGE_OK) {
                printf("Error: %s\n", sage_get_error(sage_ctx));
            }
        }
#endif
    } else if (argc >= 3) {
        // Multiple arguments
        if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--eval") == 0) {
            // Evaluate inline code
            result = sage_eval_string(sage_ctx, argv[2], strlen(argv[2]));
            if (result != SAGE_OK) {
#ifdef PICO_BUILD
                printf("Error: %s\r\n", sage_get_error(sage_ctx));
#else
                printf("Error: %s\n", sage_get_error(sage_ctx));
#endif
            }
        } else if (strcmp(argv[1], "-m") == 0 || strcmp(argv[1], "--mem") == 0) {
            // Show memory stats
            size_t allocated, objects;
            sage_get_memory_stats(sage_ctx, &allocated, &objects);
#ifdef PICO_BUILD
            printf("SageLang Memory:\r\n");
            printf("  Allocated: %u bytes\r\n", (unsigned)allocated);
            printf("  Objects: %u\r\n", (unsigned)objects);
#else
            printf("SageLang Memory:\n");
            printf("  Allocated: %zu bytes\n", allocated);
            printf("  Objects: %zu\n", objects);
#endif
        } else if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--lint") == 0) {
            LintOptions opts = lint_default_options();
            LintMessage* msgs = lint_source(argv[2], "<shell>", opts);
            if (!msgs) {
#ifdef PICO_BUILD
                printf("No issues found.\r\n");
#else
                printf("No issues found.\n");
#endif
            } else {
                int count = 0;
                for (LintMessage* m = msgs; m; m = m->next) {
                    const char* sev = m->severity == LINT_ERROR ? "error" :
                                      m->severity == LINT_WARNING ? "warn" : "style";
#ifdef PICO_BUILD
                    printf("  [%s] %s:%d:%d: %s\r\n", sev, m->rule,
                           m->line, m->column, m->message);
#else
                    printf("  [%s] %s:%d:%d: %s\n", sev, m->rule,
                           m->line, m->column, m->message);
#endif
                    count++;
                }
#ifdef PICO_BUILD
                printf("%d issue(s) found.\r\n", count);
#else
                printf("%d issue(s) found.\n", count);
#endif
                free_lint_messages(msgs);
            }
        } else {
#ifdef PICO_BUILD
            printf("Unknown option: %s\r\n", argv[1]);
            printf("Try 'sage --help' for usage\r\n");
#else
            printf("Unknown option: %s\n", argv[1]);
            printf("Try 'sage --help' for usage\n");
#endif
            result = SAGE_ERROR_RUNTIME;
        }
    }
    
    return (result == SAGE_OK) ? 0 : 1;
}
