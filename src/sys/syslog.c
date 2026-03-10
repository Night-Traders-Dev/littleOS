/* syslog.c - Persistent system log for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "syslog.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#endif

typedef struct {
    uint32_t       magic;
    uint32_t       head;
    uint32_t       count;
    uint32_t       boot_count;
    syslog_entry_t entries[SYSLOG_MAX_ENTRIES];
} syslog_store_t;

static syslog_store_t __attribute__((section(".uninitialized_data"))) syslog_store;

static const char *syslog_type_str(syslog_type_t type) {
    switch (type) {
        case SYSLOG_BOOT:     return "BOOT";
        case SYSLOG_SHUTDOWN: return "SHUT";
        case SYSLOG_ERROR:    return "ERR ";
        case SYSLOG_WARNING:  return "WARN";
        case SYSLOG_INFO:     return "INFO";
        case SYSLOG_CONFIG:   return "CONF";
        default:              return "????";
    }
}

void syslog_init(void) {
    if (syslog_store.magic != SYSLOG_MAGIC) {
        /* First boot or corrupted - initialize */
        memset(&syslog_store, 0, sizeof(syslog_store));
        syslog_store.magic = SYSLOG_MAGIC;
    }
    syslog_store.boot_count++;
    syslog_write(SYSLOG_BOOT, "System boot #%lu", (unsigned long)syslog_store.boot_count);
}

void syslog_write(syslog_type_t type, const char *fmt, ...) {
    uint32_t idx = syslog_store.head;
    syslog_entry_t *e = &syslog_store.entries[idx];

    e->type = type;
    e->boot_count = syslog_store.boot_count;
#ifdef PICO_BUILD
    e->timestamp_ms = to_ms_since_boot(get_absolute_time());
#else
    e->timestamp_ms = 0;
#endif

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, SYSLOG_MAX_MSG_LEN, fmt, ap);
    va_end(ap);
    e->msg[SYSLOG_MAX_MSG_LEN - 1] = '\0';

    syslog_store.head = (syslog_store.head + 1) % SYSLOG_MAX_ENTRIES;
    if (syslog_store.count < SYSLOG_MAX_ENTRIES)
        syslog_store.count++;
}

int syslog_read(syslog_entry_t *entries, int max_entries) {
    int count = (int)syslog_store.count;
    if (count > max_entries) count = max_entries;

    int start;
    if (syslog_store.count >= SYSLOG_MAX_ENTRIES) {
        start = (int)syslog_store.head; /* oldest entry */
    } else {
        start = 0;
    }

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % SYSLOG_MAX_ENTRIES;
        entries[i] = syslog_store.entries[idx];
    }
    return count;
}

void syslog_clear(void) {
    uint32_t boot = syslog_store.boot_count;
    memset(&syslog_store, 0, sizeof(syslog_store));
    syslog_store.magic = SYSLOG_MAGIC;
    syslog_store.boot_count = boot;
}

int syslog_get_count(void) {
    return (int)syslog_store.count;
}

uint32_t syslog_get_boot_count(void) {
    return syslog_store.boot_count;
}

void syslog_print_all(void) {
    if (syslog_store.count == 0) {
        printf("(no syslog entries)\r\n");
        return;
    }

    int start;
    int count = (int)syslog_store.count;
    if (syslog_store.count >= SYSLOG_MAX_ENTRIES) {
        start = (int)syslog_store.head;
    } else {
        start = 0;
    }

    printf("Boot#  Time(ms)   Type  Message\r\n");
    printf("-----  ---------  ----  -------\r\n");

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % SYSLOG_MAX_ENTRIES;
        syslog_entry_t *e = &syslog_store.entries[idx];
        printf("%-5lu  %-9lu  %s  %s\r\n",
               (unsigned long)e->boot_count,
               (unsigned long)e->timestamp_ms,
               syslog_type_str(e->type),
               e->msg);
    }
}
