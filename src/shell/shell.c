#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

// Forward declarations
extern int cmd_sage(int argc, char* argv[]);
extern int cmd_script(int argc, char* argv[]);

// Simple argument parsing helper
static int parse_args(char* buffer, char* argv[], int max_args) {
    int argc = 0;
    char* token = strtok(buffer, " ");
    
    while (token != NULL && argc < max_args) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    return argc;
}

void shell_run() {
    char buffer[512];  // Increased buffer for script storage commands
    int idx = 0;

    printf("\r\nWelcome to littleOS Shell!\r\n");
    printf("Type 'help' for available commands\r\n> ");

    while (1) {
        int c = getchar_timeout_us(0);  // Non-blocking read
        
        if (c == PICO_ERROR_TIMEOUT) {
            // No character available, continue
            sleep_ms(10);
            continue;
        }

        // Echo character
        putchar(c);
        fflush(stdout);

        if (c == '\r' || c == '\n') { // Enter key
            putchar('\n');
            buffer[idx] = '\0';

            // Parse command and arguments
            char* argv[32];  // Increased for script commands
            int argc = parse_args(buffer, argv, 32);

            if (argc > 0) {
                if (strcmp(argv[0], "help") == 0) {
                    printf("Available commands:\r\n");
                    printf("  help     - Show this help message\r\n");
                    printf("  version  - Show OS version\r\n");
                    printf("  clear    - Clear the screen\r\n");
                    printf("  reboot   - Reboot the system\r\n");
                    printf("  sage     - SageLang interpreter (type 'sage --help')\r\n");
                    printf("  script   - Script management (type 'script' for help)\r\n");
                } else if (strcmp(argv[0], "version") == 0) {
                    printf("littleOS v0.2.0 - RP2040\r\n");
                    printf("With SageLang v0.8.0\r\n");
                } else if (strcmp(argv[0], "clear") == 0) {
                    // ANSI escape codes to clear screen and move cursor to top-left
                    printf("\033[2J");      // Clear entire screen
                    printf("\033[H");       // Move cursor to home (1,1)
                    printf(">");            // Show prompt
                    fflush(stdout);
                    idx = 0;
                    continue;  // Skip the normal prompt below
                } else if (strcmp(argv[0], "reboot") == 0) {
                    printf("Rebooting...\r\n");
                    // Trigger watchdog reset or similar (omitted)
                } else if (strcmp(argv[0], "sage") == 0) {
                    // Call SageLang command handler
                    cmd_sage(argc, argv);
                } else if (strcmp(argv[0], "script") == 0) {
                    // Call script management handler
                    cmd_script(argc, argv);
                } else {
                    printf("Unknown command: %s\r\n", argv[0]);
                    printf("Type 'help' for available commands\r\n");
                }
            }

            idx = 0;
            printf(">");
            fflush(stdout);
        } else if (c == '\b' || c == 0x7F) { // Backspace
            if (idx > 0) {
                idx--;
                printf("\b \b");  // Erase character on screen
                fflush(stdout);
            }
        } else if (c >= 32 && c < 127) {  // Printable characters only
            if (idx < sizeof(buffer) - 1) {
                buffer[idx++] = (char)c;
            }
        }
    }
}
