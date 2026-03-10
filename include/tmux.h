/* tmux.h - Terminal Multiplexer for littleOS */
#ifndef LITTLEOS_TMUX_H
#define LITTLEOS_TMUX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TMUX_MAX_WINDOWS    4
#define TMUX_NAME_LEN       16
#define TMUX_BUF_SIZE       1024

typedef struct {
    char     name[TMUX_NAME_LEN];
    uint8_t  buffer[TMUX_BUF_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    bool     active;
} tmux_window_t;

int  tmux_init(void);
int  tmux_create_window(const char *name);
int  tmux_destroy_window(int index);
int  tmux_switch(int index);
int  tmux_next(void);
int  tmux_prev(void);
int  tmux_write(int index, const void *data, size_t len);
int  tmux_get_active(void);
int  tmux_get_window_count(void);
int  tmux_get_output(int index, char *buf, size_t buflen);
int  tmux_rename_window(int index, const char *name);
int  tmux_status_bar(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* LITTLEOS_TMUX_H */
