/* cmd_timer.c - Shell command for the general-purpose timer HAL */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hal/timer.h"

/* Callback for demo one-shot timer */
static bool demo_oneshot_cb(uint8_t timer_id, void *user_data) {
    (void)user_data;
    printf("[timer %u] one-shot fired\r\n", timer_id);
    return false;
}

/* Callback for demo repeating timer */
static uint32_t repeat_count = 0;
static bool demo_repeat_cb(uint8_t timer_id, void *user_data) {
    uint32_t max = user_data ? *(uint32_t *)user_data : 5;
    repeat_count++;
    printf("[timer %u] tick %lu/%lu\r\n", timer_id,
           (unsigned long)repeat_count, (unsigned long)max);
    return repeat_count < max;
}

static int cmd_timer_status(void) {
    timer_hal_print_status();
    return 0;
}

static int cmd_timer_uptime(void) {
    uint64_t us = timer_hal_get_us();
    uint32_t sec = (uint32_t)(us / 1000000ULL);
    uint32_t ms  = (uint32_t)((us / 1000ULL) % 1000ULL);
    printf("Uptime: %lu.%03lu s (%llu us)\r\n",
           (unsigned long)sec, (unsigned long)ms,
           (unsigned long long)us);
    return 0;
}

static int cmd_timer_oneshot(int argc, char *argv[]) {
    uint32_t delay_ms = 1000;
    if (argc >= 2) delay_ms = (uint32_t)atoi(argv[1]);
    if (delay_ms == 0) delay_ms = 1000;

    printf("Setting one-shot timer for %lu ms...\r\n", (unsigned long)delay_ms);
    int id = timer_hal_set_oneshot(delay_ms * 1000, demo_oneshot_cb, NULL);
    if (id < 0) {
        printf("Failed (no free slots?)\r\n");
        return 1;
    }
    printf("Timer %d armed\r\n", id);
    return 0;
}

static uint32_t max_ticks = 5;

static int cmd_timer_repeat(int argc, char *argv[]) {
    uint32_t interval_ms = 1000;
    max_ticks = 5;

    if (argc >= 2) interval_ms = (uint32_t)atoi(argv[1]);
    if (argc >= 3) max_ticks = (uint32_t)atoi(argv[2]);
    if (interval_ms == 0) interval_ms = 1000;
    if (max_ticks == 0) max_ticks = 5;

    repeat_count = 0;
    printf("Setting repeating timer: %lu ms interval, %lu ticks\r\n",
           (unsigned long)interval_ms, (unsigned long)max_ticks);
    int id = timer_hal_set_repeating(interval_ms * 1000, demo_repeat_cb, &max_ticks);
    if (id < 0) {
        printf("Failed (no free slots?)\r\n");
        return 1;
    }
    printf("Timer %d started\r\n", id);
    return 0;
}

static int cmd_timer_cancel(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: timer cancel <id|all>\r\n");
        return 1;
    }

    if (strcmp(argv[1], "all") == 0) {
        timer_hal_cancel_all();
        printf("All timers cancelled\r\n");
        return 0;
    }

    uint8_t id = (uint8_t)atoi(argv[1]);
    if (timer_hal_cancel(id) == 0) {
        printf("Timer %u cancelled\r\n", id);
        return 0;
    }
    printf("Invalid timer ID\r\n");
    return 1;
}

static int cmd_timer_measure(void) {
    printf("Measuring 10ms busy-wait accuracy...\r\n");
    uint64_t start = timer_hal_start_measurement();
    timer_hal_delay_ms(10);
    uint32_t elapsed = timer_hal_elapsed_us(start);
    printf("Expected: 10000 us\r\n");
    printf("Actual:   %lu us\r\n", (unsigned long)elapsed);

    int32_t error = (int32_t)elapsed - 10000;
    printf("Error:    %ld us (%.2f%%)\r\n",
           (long)error, (float)error / 100.0f);
    return 0;
}

int cmd_timer(int argc, char *argv[]) {
    timer_hal_init();

    if (argc < 2) {
        printf("Usage: timer <subcommand>\r\n");
        printf("\r\nSubcommands:\r\n");
        printf("  status                    - Show active timers\r\n");
        printf("  uptime                    - Show system uptime\r\n");
        printf("  oneshot [ms]              - Set one-shot timer (default 1000ms)\r\n");
        printf("  repeat [ms] [count]       - Set repeating timer\r\n");
        printf("  cancel <id|all>           - Cancel a timer\r\n");
        printf("  measure                   - Test timer accuracy\r\n");
        return 0;
    }

    if (strcmp(argv[1], "status") == 0)  return cmd_timer_status();
    if (strcmp(argv[1], "uptime") == 0)  return cmd_timer_uptime();
    if (strcmp(argv[1], "oneshot") == 0) return cmd_timer_oneshot(argc - 1, argv + 1);
    if (strcmp(argv[1], "repeat") == 0)  return cmd_timer_repeat(argc - 1, argv + 1);
    if (strcmp(argv[1], "cancel") == 0)  return cmd_timer_cancel(argc - 1, argv + 1);
    if (strcmp(argv[1], "measure") == 0) return cmd_timer_measure();

    printf("Unknown subcommand: %s\r\n", argv[1]);
    return 1;
}
