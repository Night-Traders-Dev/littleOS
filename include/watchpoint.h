#ifndef LITTLEOS_WATCHPOINT_H
#define LITTLEOS_WATCHPOINT_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define WATCHPOINT_MAX  8

typedef enum {
    WP_TYPE_WRITE = 0,
    WP_TYPE_READ  = 1,
    WP_TYPE_VALUE = 2,  /* trigger when value changes */
} watchpoint_type_t;

typedef struct {
    uint32_t          address;
    uint32_t          size;       /* 1, 2, or 4 bytes */
    watchpoint_type_t type;
    uint32_t          last_value;
    uint32_t          trigger_count;
    bool              active;
    char              label[16];
} watchpoint_t;

void watchpoint_init(void);
int  watchpoint_add(uint32_t addr, uint32_t size, watchpoint_type_t type, const char *label);
int  watchpoint_remove(int index);
void watchpoint_check(void);  /* Call periodically to detect value changes */
int  watchpoint_list(char *buf, size_t buflen);
int  watchpoint_get_count(void);

#ifdef __cplusplus
}
#endif
#endif
