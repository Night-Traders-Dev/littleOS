// shell.c
#include <stdio.h>
#include <string.h>

#define CMD_BUF_LEN 64

// Handles the logic for a received command
static void handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        printf("\r\nCommands: help, led, reboot\r\n");
    } else if (strcmp(cmd, "led") == 0) {
        printf("\r\n[toggle LED stub]\r\n");
    } else if (strcmp(cmd, "reboot") == 0) {
        printf("\r\n[reboot stub]\r\n");
    } else if (strlen(cmd) > 0) {
        printf("\r\nUnknown command: '%s'\r\n", cmd);
    }
}

// Main loop for the interactive shell
void shell_run(void) {
    char buf[CMD_BUF_LEN];
    unsigned int idx = 0;

    while (1) {
        char c = getchar(); // Read a character using the standard library

        if (c == '\r' || c == '\n') {
            buf[idx] = '\0'; // Null-terminate the string
            printf("\r\n");
            handle_command(buf);
            idx = 0; // Reset buffer index
            printf("> ");
        } else if (c == 0x7F || c == '\b') { // Handle backspace
            if (idx > 0) {
                idx--;
                printf("\b \b"); // Move cursor back, erase char, move back again
            }
        } else if (idx < CMD_BUF_LEN - 1) {
            buf[idx++] = c;
            putchar(c); // Echo character back to the terminal
        }
    }
}
