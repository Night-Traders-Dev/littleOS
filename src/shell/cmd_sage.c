#include <stdio.h>
#include <string.h>
#include "sage_embed.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

/**
 * @brief Handle 'sage' command from shell
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, non-zero on error
 */
int cmd_sage(int argc, char* argv[]) {
    sage_context_t* ctx = sage_init();
    if (!ctx) {
#ifdef PICO_BUILD
        printf("Failed to initialize SageLang\r\n");
#else
        printf("Failed to initialize SageLang\n");
#endif
        return 1;
    }
    
    sage_result_t result = SAGE_OK;
    
    if (argc == 1) {
        // No arguments - start REPL
        result = sage_repl(ctx);
    } else if (argc == 2) {
        // One argument - execute file (PC only) or eval string
        const char* arg = argv[1];
        
#ifdef PICO_BUILD
        // On embedded, treat argument as code to evaluate
        result = sage_eval_string(ctx, arg, strlen(arg));
        if (result != SAGE_OK) {
            printf("Error: %s\r\n", sage_get_error(ctx));
        }
#else
        // On PC, check if it's a file or code
        if (strstr(arg, ".sage") != NULL) {
            // Has .sage extension - treat as file
            result = sage_eval_file(ctx, arg);
            if (result != SAGE_OK) {
                printf("Error: %s\n", sage_get_error(ctx));
            }
        } else {
            // No extension - treat as code
            result = sage_eval_string(ctx, arg, strlen(arg));
            if (result != SAGE_OK) {
                printf("Error: %s\n", sage_get_error(ctx));
            }
        }
#endif
    } else if (argc >= 3) {
        // Multiple arguments
        if (strcmp(argv[1], "-e") == 0 || strcmp(argv[1], "--eval") == 0) {
            // Evaluate inline code
            result = sage_eval_string(ctx, argv[2], strlen(argv[2]));
            if (result != SAGE_OK) {
#ifdef PICO_BUILD
                printf("Error: %s\r\n", sage_get_error(ctx));
#else
                printf("Error: %s\n", sage_get_error(ctx));
#endif
            }
        } else if (strcmp(argv[1], "-m") == 0 || strcmp(argv[1], "--mem") == 0) {
            // Show memory stats
            size_t allocated, objects;
            sage_get_memory_stats(ctx, &allocated, &objects);
#ifdef PICO_BUILD
            printf("Memory: %u bytes, %u objects\r\n", 
                   (unsigned)allocated, (unsigned)objects);
#else
            printf("Memory: %zu bytes, %zu objects\n", allocated, objects);
#endif
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            // Print help
#ifdef PICO_BUILD
            printf("Usage: sage [options] [script]\r\n");
            printf("\r\nOptions:\r\n");
            printf("  (no args)        Start interactive REPL\r\n");
            printf("  -e, --eval CODE  Evaluate inline code\r\n");
            printf("  -m, --mem        Show memory statistics\r\n");
            printf("  -h, --help       Show this help\r\n");
            printf("\r\nEmbedded mode (interpreter only)\r\n");
#else
            printf("Usage: sage [options] [script.sage]\n");
            printf("\nOptions:\n");
            printf("  (no args)        Start interactive REPL\n");
            printf("  script.sage      Execute SageLang file\n");
            printf("  -e, --eval CODE  Evaluate inline code\n");
            printf("  -m, --mem        Show memory statistics\n");
            printf("  -h, --help       Show this help\n");
            printf("\nPC mode (interpreter + future compiler support)\n");
#endif
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
    
    sage_cleanup(ctx);
    return (result == SAGE_OK) ? 0 : 1;
}
