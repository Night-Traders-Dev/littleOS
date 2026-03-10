#ifndef LITTLEOS_TRACE_H
#define LITTLEOS_TRACE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_MAX_ENTRIES    256
#define TRACE_MAX_NAME_LEN   24

typedef enum {
    TRACE_ENTER = 0,
    TRACE_EXIT  = 1,
    TRACE_EVENT = 2,
    TRACE_ISR   = 3,
    TRACE_SCHED = 4,
} trace_type_t;

typedef struct {
    uint32_t     timestamp_us;
    trace_type_t type;
    char         name[TRACE_MAX_NAME_LEN];
    uint32_t     arg;
} trace_entry_t;

void trace_init(void);
void trace_record(trace_type_t type, const char *name, uint32_t arg);
void trace_clear(void);
int  trace_dump(char *buf, size_t buflen);
int  trace_get_count(void);
bool trace_is_enabled(void);
void trace_enable(bool enable);

/* Convenience macros */
#define TRACE_ENTER_FN(name) trace_record(TRACE_ENTER, name, 0)
#define TRACE_EXIT_FN(name)  trace_record(TRACE_EXIT, name, 0)
#define TRACE_EVENT_VAL(name, val) trace_record(TRACE_EVENT, name, val)

#ifdef __cplusplus
}
#endif
#endif
