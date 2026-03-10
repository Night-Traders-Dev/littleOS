#ifndef LITTLEOS_LOGCAT_H
#define LITTLEOS_LOGCAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Log levels */
typedef enum {
    LOG_VERBOSE = 0,
    LOG_DEBUG   = 1,
    LOG_INFO    = 2,
    LOG_WARN    = 3,
    LOG_ERROR   = 4,
    LOG_FATAL   = 5,
} log_level_t;

#define LOGCAT_MAX_ENTRIES   64
#define LOGCAT_MAX_MSG_LEN   64
#define LOGCAT_MAX_TAG_LEN   16
#define LOGCAT_MAX_FILTERS   8

typedef struct {
    uint32_t    timestamp_ms;
    log_level_t level;
    char        tag[LOGCAT_MAX_TAG_LEN];
    char        msg[LOGCAT_MAX_MSG_LEN];
} log_entry_t;

void logcat_init(void);
void logcat_log(log_level_t level, const char *tag, const char *fmt, ...);
int  logcat_dump(char *buf, size_t buflen, log_level_t min_level, const char *tag_filter);
void logcat_clear(void);
int  logcat_get_count(void);
int  logcat_set_level(log_level_t level);
log_level_t logcat_get_level(void);
const char *logcat_level_str(log_level_t level);
log_level_t logcat_parse_level(const char *str);

/* Convenience macros */
#define LOGV(tag, fmt, ...) logcat_log(LOG_VERBOSE, tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) logcat_log(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) logcat_log(LOG_INFO, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) logcat_log(LOG_WARN, tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) logcat_log(LOG_ERROR, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif
