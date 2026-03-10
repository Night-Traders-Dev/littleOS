#ifndef LITTLEOS_SYSLOG_H
#define LITTLEOS_SYSLOG_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SYSLOG_MAX_ENTRIES   64
#define SYSLOG_MAX_MSG_LEN   64
#define SYSLOG_MAGIC         0x59534C47  /* "YSLG" */

typedef enum {
    SYSLOG_BOOT    = 0,
    SYSLOG_SHUTDOWN = 1,
    SYSLOG_ERROR   = 2,
    SYSLOG_WARNING = 3,
    SYSLOG_INFO    = 4,
    SYSLOG_CONFIG  = 5,
} syslog_type_t;

typedef struct {
    uint32_t     timestamp_ms;
    uint32_t     boot_count;
    syslog_type_t type;
    char         msg[SYSLOG_MAX_MSG_LEN];
} syslog_entry_t;

void syslog_init(void);
void syslog_write(syslog_type_t type, const char *fmt, ...);
int  syslog_read(syslog_entry_t *entries, int max_entries);
void syslog_clear(void);
int  syslog_get_count(void);
uint32_t syslog_get_boot_count(void);
void syslog_print_all(void);

#ifdef __cplusplus
}
#endif
#endif
