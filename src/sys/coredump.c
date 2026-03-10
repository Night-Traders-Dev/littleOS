/* coredump.c - Crash dump system for littleOS */
#include <stdio.h>
#include <string.h>
#include "coredump.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#endif

/* Coredump stored in .noinit section - survives soft reboot */
static coredump_t __attribute__((section(".uninitialized_data"))) saved_coredump;

static uint32_t compute_checksum(const coredump_t *d) {
    const uint32_t *p = (const uint32_t *)d;
    uint32_t sum = 0;
    /* XOR all fields except the checksum itself (last field) */
    size_t n = (sizeof(coredump_t) - sizeof(uint32_t)) / sizeof(uint32_t);
    for (size_t i = 0; i < n; i++) {
        sum ^= p[i];
    }
    return sum;
}

void coredump_init(void) {
    /* Check if there's a valid coredump from previous crash */
    if (coredump_exists()) {
        printf("[COREDUMP] Previous crash dump found!\r\n");
        printf("  Use 'coredump show' to view, 'coredump clear' to dismiss\r\n");
    }
}

void coredump_save(const coredump_t *dump) {
    memcpy(&saved_coredump, dump, sizeof(coredump_t));
    saved_coredump.magic = COREDUMP_MAGIC;
    saved_coredump.checksum = compute_checksum(&saved_coredump);
}

bool coredump_load(coredump_t *dump) {
    if (!coredump_exists()) return false;
    memcpy(dump, &saved_coredump, sizeof(coredump_t));
    return true;
}

void coredump_clear(void) {
    memset(&saved_coredump, 0, sizeof(coredump_t));
}

bool coredump_exists(void) {
    if (saved_coredump.magic != COREDUMP_MAGIC) return false;
    return saved_coredump.checksum == compute_checksum(&saved_coredump);
}

void coredump_print(const coredump_t *d) {
    const char *fault_names[] = { "HardFault", "MemFault", "BusFault", "UsageFault", "Panic" };
    const char *fault_name = (d->fault_type < 5) ? fault_names[d->fault_type] : "Unknown";

    printf("=== COREDUMP ===\r\n");
    printf("Fault:    %s (type=%lu)\r\n", fault_name, (unsigned long)d->fault_type);
    printf("Uptime:   %lu ms\r\n", (unsigned long)d->uptime_ms);
    printf("\r\nRegisters:\r\n");
    printf("  PC:   0x%08lX\r\n", (unsigned long)d->pc);
    printf("  LR:   0x%08lX\r\n", (unsigned long)d->lr);
    printf("  SP:   0x%08lX\r\n", (unsigned long)d->sp);
    printf("  R0:   0x%08lX  R1:  0x%08lX\r\n", (unsigned long)d->r0, (unsigned long)d->r1);
    printf("  R2:   0x%08lX  R3:  0x%08lX\r\n", (unsigned long)d->r2, (unsigned long)d->r3);
    printf("  R12:  0x%08lX  PSR: 0x%08lX\r\n", (unsigned long)d->r12, (unsigned long)d->psr);
    printf("\r\nFault Status:\r\n");
    printf("  CFSR: 0x%08lX\r\n", (unsigned long)d->cfsr);
    printf("  HFSR: 0x%08lX\r\n", (unsigned long)d->hfsr);
    printf("  MMFAR: 0x%08lX\r\n", (unsigned long)d->mmfar);
    printf("  BFAR:  0x%08lX\r\n", (unsigned long)d->bfar);

    printf("\r\nStack dump (%d bytes from SP):\r\n", COREDUMP_STACK_SAVE);
    for (int i = 0; i < COREDUMP_STACK_SAVE; i++) {
        if (i % 16 == 0) printf("  %04X: ", i);
        printf("%02X ", d->stack_dump[i]);
        if (i % 16 == 15) printf("\r\n");
    }
    if (COREDUMP_STACK_SAVE % 16 != 0) printf("\r\n");
}

void coredump_panic(const char *reason) {
#ifdef PICO_BUILD
    coredump_t dump;
    memset(&dump, 0, sizeof(dump));
    dump.fault_type = 4; /* Panic */
    dump.uptime_ms = to_ms_since_boot(get_absolute_time());

    /* Capture registers */
    uint32_t sp_val;
    asm volatile("mov %0, sp" : "=r"(sp_val));
    dump.sp = sp_val;
    asm volatile("mov %0, lr" : "=r"(dump.lr));

    /* Capture stack */
    const uint8_t *sp_ptr = (const uint8_t *)sp_val;
    for (int i = 0; i < COREDUMP_STACK_SAVE && (sp_val + i) < 0x20042000; i++) {
        dump.stack_dump[i] = sp_ptr[i];
    }

    printf("\r\n!!! KERNEL PANIC: %s !!!\r\n", reason ? reason : "unknown");
    printf("Saving coredump and resetting...\r\n");

    coredump_save(&dump);

    /* Reset via watchdog */
    watchdog_enable(1, false);
    while(1) tight_loop_contents();
#else
    printf("PANIC: %s (no reset in emulator)\r\n", reason ? reason : "unknown");
#endif
}
