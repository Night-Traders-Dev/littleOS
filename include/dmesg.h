// dmesg.h - Kernel Debug Message Buffer Header
#ifndef DMESG_H
#define DMESG_H

#include <stdint.h>

/* Log level definitions (similar to Linux kernel) */
#define DMESG_LEVEL_EMERG   0  /* System is unusable */
#define DMESG_LEVEL_ALERT   1  /* Action must be taken immediately */
#define DMESG_LEVEL_CRIT    2  /* Critical conditions */
#define DMESG_LEVEL_ERR     3  /* Error conditions */
#define DMESG_LEVEL_WARN    4  /* Warning conditions */
#define DMESG_LEVEL_NOTICE  5  /* Normal but significant condition */
#define DMESG_LEVEL_INFO    6  /* Informational */
#define DMESG_LEVEL_DEBUG   7  /* Debug-level messages */

/* Configuration */
#define DMESG_BUFFER_SIZE   128  /* Number of messages to store */
#define DMESG_MSG_MAX       120  /* Max message length */

/* Message entry structure */
typedef struct {
    uint32_t timestamp_ms;          /* Milliseconds since boot */
    uint8_t  level;                 /* Log level */
    char     message[DMESG_MSG_MAX]; /* Message text */
} dmesg_entry_t;

/* Core API functions */
void dmesg_init(void);
void dmesg_log(uint8_t level, const char *fmt, ...);
uint32_t dmesg_get_count(void);
uint32_t dmesg_get_uptime(void);
void dmesg_print_all(void);
void dmesg_print_level(uint8_t min_level);
void dmesg_clear(void);

/* Convenience macros for different log levels */
#define dmesg_emerg(fmt, ...)   dmesg_log(DMESG_LEVEL_EMERG,  fmt, ##__VA_ARGS__)
#define dmesg_alert(fmt, ...)   dmesg_log(DMESG_LEVEL_ALERT,  fmt, ##__VA_ARGS__)
#define dmesg_crit(fmt, ...)    dmesg_log(DMESG_LEVEL_CRIT,   fmt, ##__VA_ARGS__)
#define dmesg_err(fmt, ...)     dmesg_log(DMESG_LEVEL_ERR,    fmt, ##__VA_ARGS__)
#define dmesg_warn(fmt, ...)    dmesg_log(DMESG_LEVEL_WARN,   fmt, ##__VA_ARGS__)
#define dmesg_notice(fmt, ...)  dmesg_log(DMESG_LEVEL_NOTICE, fmt, ##__VA_ARGS__)
#define dmesg_info(fmt, ...)    dmesg_log(DMESG_LEVEL_INFO,   fmt, ##__VA_ARGS__)
#define dmesg_debug(fmt, ...)   dmesg_log(DMESG_LEVEL_DEBUG,  fmt, ##__VA_ARGS__)

#endif /* DMESG_H */
