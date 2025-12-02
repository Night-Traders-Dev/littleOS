#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

void shell_run() {
    char buffer[64];
    int idx = 0;

    printf("\r\nWelcome to littleOS Shell!\r\n> ");

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

            if (strcmp(buffer, "help") == 0) {
                printf("Available commands: help, version, reboot\r\n");
            } else if (strcmp(buffer, "version") == 0) {
                printf("littleOS v0.1.0 - RP2040\r\n");
            } else if (strcmp(buffer, "reboot") == 0) {
                printf("Rebooting...\r\n");
                // Trigger watchdog reset or similar (omitted)
            } else if (idx > 0) {
                printf("Unknown command.\r\n");
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
            if (idx < 63) {
                buffer[idx++] = (char)c;
            }
        }
    }
}
