#ifndef LITTLEOS_COREDUMP_H
#define LITTLEOS_COREDUMP_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define COREDUMP_MAGIC      0xC0DEDEAD
#define COREDUMP_STACK_SAVE 256  /* bytes of stack to capture */

typedef struct {
    uint32_t magic;
    uint32_t timestamp_ms;
    uint32_t fault_type;    /* 0=HardFault, 1=MemFault, 2=BusFault, 3=UsageFault, 4=Panic */
    uint32_t pc;            /* Program counter at fault */
    uint32_t lr;            /* Link register */
    uint32_t sp;            /* Stack pointer */
    uint32_t r0, r1, r2, r3, r12;
    uint32_t psr;           /* Program status register */
    uint32_t cfsr;          /* Configurable Fault Status Register */
    uint32_t hfsr;          /* HardFault Status Register */
    uint32_t mmfar;         /* MemManage Fault Address */
    uint32_t bfar;          /* Bus Fault Address */
    uint8_t  stack_dump[COREDUMP_STACK_SAVE];
    uint32_t uptime_ms;
    uint32_t checksum;
} coredump_t;

void coredump_init(void);
void coredump_save(const coredump_t *dump);
bool coredump_load(coredump_t *dump);
void coredump_clear(void);
bool coredump_exists(void);
void coredump_print(const coredump_t *dump);
void coredump_panic(const char *reason);

#ifdef __cplusplus
}
#endif
#endif
