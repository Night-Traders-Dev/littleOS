/* tmux.c - Terminal Multiplexer for littleOS */
#include <stdio.h>
#include <string.h>
#include "tmux.h"

static tmux_window_t windows[TMUX_MAX_WINDOWS];
static int active_window = 0;
static int window_count = 0;

int tmux_init(void) {
    memset(windows, 0, sizeof(windows));
    /* Create default window 0: shell */
    strncpy(windows[0].name, "shell", TMUX_NAME_LEN - 1);
    windows[0].name[TMUX_NAME_LEN - 1] = '\0';
    windows[0].active = true;
    window_count = 1;
    active_window = 0;
    return 0;
}

int tmux_create_window(const char *name) {
    if (window_count >= TMUX_MAX_WINDOWS) return -1;

    for (int i = 0; i < TMUX_MAX_WINDOWS; i++) {
        if (!windows[i].active) {
            memset(&windows[i], 0, sizeof(tmux_window_t));
            strncpy(windows[i].name, name ? name : "window", TMUX_NAME_LEN - 1);
            windows[i].name[TMUX_NAME_LEN - 1] = '\0';
            windows[i].active = true;
            window_count++;
            return i;
        }
    }
    return -1;
}

int tmux_destroy_window(int index) {
    if (index < 0 || index >= TMUX_MAX_WINDOWS) return -1;
    if (index == 0) return -2; /* Cannot destroy window 0 */
    if (!windows[index].active) return -3;

    windows[index].active = false;
    window_count--;

    if (active_window == index) {
        /* Switch to window 0 */
        active_window = 0;
    }
    return 0;
}

int tmux_switch(int index) {
    if (index < 0 || index >= TMUX_MAX_WINDOWS) return -1;
    if (!windows[index].active) return -2;

    active_window = index;

    /* Clear screen and show window content */
    printf("\033[2J\033[H");

    /* Replay buffer content */
    if (windows[index].count > 0) {
        uint16_t pos = windows[index].tail;
        for (uint16_t i = 0; i < windows[index].count; i++) {
            putchar(windows[index].buffer[pos]);
            pos = (pos + 1) % TMUX_BUF_SIZE;
        }
    }

    /* Show status bar */
    char bar[128];
    tmux_status_bar(bar, sizeof(bar));
    printf("\r\n%s\r\n", bar);
    fflush(stdout);
    return 0;
}

int tmux_next(void) {
    int next = active_window;
    for (int i = 0; i < TMUX_MAX_WINDOWS; i++) {
        next = (next + 1) % TMUX_MAX_WINDOWS;
        if (windows[next].active)
            return tmux_switch(next);
    }
    return -1;
}

int tmux_prev(void) {
    int prev = active_window;
    for (int i = 0; i < TMUX_MAX_WINDOWS; i++) {
        prev = (prev - 1 + TMUX_MAX_WINDOWS) % TMUX_MAX_WINDOWS;
        if (windows[prev].active)
            return tmux_switch(prev);
    }
    return -1;
}

int tmux_write(int index, const void *data, size_t len) {
    if (index < 0 || index >= TMUX_MAX_WINDOWS) return -1;
    if (!windows[index].active) return -2;

    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        windows[index].buffer[windows[index].head] = bytes[i];
        windows[index].head = (windows[index].head + 1) % TMUX_BUF_SIZE;
        if (windows[index].count < TMUX_BUF_SIZE) {
            windows[index].count++;
        } else {
            /* Overwrite oldest */
            windows[index].tail = (windows[index].tail + 1) % TMUX_BUF_SIZE;
        }
    }
    return (int)len;
}

int tmux_get_active(void) {
    return active_window;
}

int tmux_get_window_count(void) {
    return window_count;
}

int tmux_get_output(int index, char *buf, size_t buflen) {
    if (index < 0 || index >= TMUX_MAX_WINDOWS) return -1;
    if (!windows[index].active) return -2;

    size_t to_copy = windows[index].count;
    if (to_copy > buflen - 1) to_copy = buflen - 1;

    uint16_t pos = windows[index].tail;
    for (size_t i = 0; i < to_copy; i++) {
        buf[i] = (char)windows[index].buffer[pos];
        pos = (pos + 1) % TMUX_BUF_SIZE;
    }
    buf[to_copy] = '\0';
    return (int)to_copy;
}

int tmux_rename_window(int index, const char *name) {
    if (index < 0 || index >= TMUX_MAX_WINDOWS) return -1;
    if (!windows[index].active) return -2;
    strncpy(windows[index].name, name, TMUX_NAME_LEN - 1);
    windows[index].name[TMUX_NAME_LEN - 1] = '\0';
    return 0;
}

int tmux_status_bar(char *buf, size_t buflen) {
    int pos = 0;
    pos += snprintf(buf + pos, buflen - pos, "\033[7m"); /* Reverse video */
    for (int i = 0; i < TMUX_MAX_WINDOWS; i++) {
        if (windows[i].active) {
            if (i == active_window)
                pos += snprintf(buf + pos, buflen - pos, " [%d:%s*] ", i, windows[i].name);
            else
                pos += snprintf(buf + pos, buflen - pos, " [%d:%s] ", i, windows[i].name);
        }
    }
    pos += snprintf(buf + pos, buflen - pos, "\033[0m");
    return pos;
}
