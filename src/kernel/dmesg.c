// dmesg.c - Kernel Debug Message Buffer Implementation
#include "dmesg.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pico/time.h"

/* Global dmesg buffer and state */
static dmesg_entry_t dmesg_buffer[DMESG_BUFFER_SIZE];
static uint32_t dmesg_write_index = 0;
static uint32_t dmesg_total_messages = 0;
static uint32_t dmesg_boot_time_us = 0;
static int dmesg_initialized = 0;

/* Level name strings */
static const char *level_names[] = {
    "EMERG", "ALERT", "CRIT", "ERR", "WARN", "NOTC", "INFO", "DBG"
};

/* Initialize dmesg system - call this early in boot */
void dmesg_init(void) {
    if (dmesg_initialized) return;
    dmesg_boot_time_us = time_us_32();
    dmesg_write_index = 0;
    dmesg_total_messages = 0;
    dmesg_initialized = 1;
    dmesg_info("littleOS dmesg initialized");
    dmesg_info("Boot sequence started");
}

/* Get current uptime in milliseconds */
uint32_t dmesg_get_uptime(void) {
    if (!dmesg_initialized) return 0;
    return (time_us_32() - dmesg_boot_time_us) / 1000;
}

/* Log a message with variable arguments */
void dmesg_log(uint8_t level, const char *fmt, ...) {
    if (!dmesg_initialized) return;
    if (level > DMESG_LEVEL_DEBUG) level = DMESG_LEVEL_DEBUG;
    
    dmesg_entry_t *entry = &dmesg_buffer[dmesg_write_index];
    entry->timestamp_ms = dmesg_get_uptime();
    entry->level = level;
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->message, DMESG_MSG_MAX, fmt, args);
    va_end(args);
    
    dmesg_write_index = (dmesg_write_index + 1) % DMESG_BUFFER_SIZE;
    dmesg_total_messages++;
    
    printf("[%5lums] <%s> %s\n", entry->timestamp_ms, level_names[level], entry->message);
}

/* Get number of messages in buffer */
uint32_t dmesg_get_count(void) {
    return (dmesg_total_messages > DMESG_BUFFER_SIZE) ? DMESG_BUFFER_SIZE : dmesg_total_messages;
}

/* Print all buffered messages */
void dmesg_print_all(void) {
    uint32_t count = dmesg_get_count();
    uint32_t start_idx = (dmesg_total_messages >= DMESG_BUFFER_SIZE) ? dmesg_write_index : 0;
    
    printf("\n========== littleOS Kernel Message Buffer ==========\n");
    printf("Total messages: %lu | Uptime: %lums\n", dmesg_total_messages, dmesg_get_uptime());
    printf("=====================================================\n");
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % DMESG_BUFFER_SIZE;
        dmesg_entry_t *entry = &dmesg_buffer[idx];
        printf("[%5lums] <%s> %s\n", entry->timestamp_ms, level_names[entry->level], entry->message);
    }
    
    printf("=====================================================\n\n");
}

/* Print messages from specific log level and above */
void dmesg_print_level(uint8_t min_level) {
    if (min_level > DMESG_LEVEL_DEBUG) min_level = DMESG_LEVEL_DEBUG;
    uint32_t count = dmesg_get_count();
    uint32_t start_idx = (dmesg_total_messages >= DMESG_BUFFER_SIZE) ? dmesg_write_index : 0;
    
    printf("\n========== Filtered Kernel Messages (level >= %s) ==========\n", level_names[min_level]);
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start_idx + i) % DMESG_BUFFER_SIZE;
        dmesg_entry_t *entry = &dmesg_buffer[idx];
        if (entry->level <= min_level) {
            printf("[%5lums] <%s> %s\n", entry->timestamp_ms, level_names[entry->level], entry->message);
        }
    }
    
    printf("==========================================================\n\n");
}

/* Clear the message buffer */
void dmesg_clear(void) {
    dmesg_write_index = 0;
    dmesg_total_messages = 0;
    memset(dmesg_buffer, 0, sizeof(dmesg_buffer));
    dmesg_info("dmesg buffer cleared");
}
