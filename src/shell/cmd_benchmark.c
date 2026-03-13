/* cmd_benchmark.c - Performance benchmarks for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#endif

#include "board/board_config.h"

static uint32_t get_us(void) {
#ifdef PICO_BUILD
    return time_us_32();
#else
    return 0;
#endif
}

/* CPU integer benchmark - simple loop operations */
static void bench_cpu_int(void) {
    printf("  CPU Integer... ");
    volatile uint32_t acc = 0;
    uint32_t start = get_us();
    for (uint32_t i = 0; i < 1000000; i++) {
        acc += i;
        acc ^= (acc >> 3);
        acc += (acc << 5);
    }
    uint32_t elapsed = get_us() - start;
    uint32_t mops = (elapsed > 0) ? (3000000UL / (elapsed / 1000)) : 0;
    printf("%lu us (%lu.%03lu MOps/s)\r\n", (unsigned long)elapsed,
           (unsigned long)(mops / 1000), (unsigned long)(mops % 1000));
    (void)acc;
}

/* Memory copy benchmark */
static void bench_memcpy(void) {
    printf("  Memory copy... ");
    static uint8_t src[1024], dst[1024];
    memset(src, 0xAA, sizeof(src));

    uint32_t start = get_us();
    for (int i = 0; i < 10000; i++) {
        memcpy(dst, src, sizeof(src));
    }
    uint32_t elapsed = get_us() - start;
    /* 10000 * 1024 bytes = 10240000 bytes */
    uint32_t mbps = (elapsed > 0) ? (10240000UL / (elapsed / 1000)) : 0;
    printf("%lu us (%lu KB/s)\r\n", (unsigned long)elapsed, (unsigned long)mbps);
}

/* Memory set benchmark */
static void bench_memset(void) {
    printf("  Memory set...  ");
    static uint8_t buf[1024];

    uint32_t start = get_us();
    for (int i = 0; i < 10000; i++) {
        memset(buf, (uint8_t)i, sizeof(buf));
    }
    uint32_t elapsed = get_us() - start;
    uint32_t mbps = (elapsed > 0) ? (10240000UL / (elapsed / 1000)) : 0;
    printf("%lu us (%lu KB/s)\r\n", (unsigned long)elapsed, (unsigned long)mbps);
}

/* String operations benchmark */
static void bench_string(void) {
    printf("  String ops...  ");
    static char buf[256];

    uint32_t start = get_us();
    for (int i = 0; i < 100000; i++) {
        snprintf(buf, sizeof(buf), "test %d value %d end", i, i * 3);
        (void)strlen(buf);
    }
    uint32_t elapsed = get_us() - start;
    uint32_t ops = (elapsed > 0) ? (100000000UL / elapsed) : 0;
    printf("%lu us (%lu KOps/s)\r\n", (unsigned long)elapsed, (unsigned long)ops);
}

/* Context switch estimate (function call overhead) */
static volatile int bench_dummy;
static void bench_func(void) { bench_dummy++; }

static void bench_call_overhead(void) {
    printf("  Function call. ");
    bench_dummy = 0;

    uint32_t start = get_us();
    for (int i = 0; i < 1000000; i++) {
        bench_func();
    }
    uint32_t elapsed = get_us() - start;
    uint32_t ns_per = (elapsed > 0) ? (elapsed * 1000UL / 1000000UL) : 0;
    printf("%lu us (%lu ns/call)\r\n", (unsigned long)elapsed, (unsigned long)ns_per);
}

/* Divmod benchmark (expensive on Cortex-M0+) */
static void bench_divmod(void) {
    printf("  Division...    ");
    volatile uint32_t a = 123456789, b = 127;
    volatile uint32_t q, r;

    uint32_t start = get_us();
    for (int i = 0; i < 100000; i++) {
        q = a / b;
        r = a % b;
        a = q + r + i;
        if (a == 0) a = 1;
    }
    uint32_t elapsed = get_us() - start;
    uint32_t ops = (elapsed > 0) ? (200000000UL / elapsed) : 0;
    printf("%lu us (%lu KOps/s)\r\n", (unsigned long)elapsed, (unsigned long)ops);
}

int cmd_benchmark(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        printf("Usage: benchmark [all|cpu|mem|string|call|div]\r\n");
        printf("Runs performance benchmarks on RP2040\r\n");
        return 0;
    }

    bool run_all = (argc < 2 || strcmp(argv[1], "all") == 0);

    printf("=== littleOS Benchmark Suite ===\r\n");
#ifdef PICO_BUILD
    printf("Platform: %s @ %lu MHz\r\n\r\n", CHIP_MODEL_STR,
           (unsigned long)(clock_get_hz(clk_sys) / 1000000));
#else
    printf("Platform: %s\r\n\r\n", CHIP_MODEL_STR);
#endif

    uint32_t total_start = get_us();

    if (run_all || (argc >= 2 && strcmp(argv[1], "cpu") == 0))
        bench_cpu_int();
    if (run_all || (argc >= 2 && strcmp(argv[1], "mem") == 0)) {
        bench_memcpy();
        bench_memset();
    }
    if (run_all || (argc >= 2 && strcmp(argv[1], "string") == 0))
        bench_string();
    if (run_all || (argc >= 2 && strcmp(argv[1], "call") == 0))
        bench_call_overhead();
    if (run_all || (argc >= 2 && strcmp(argv[1], "div") == 0))
        bench_divmod();

    uint32_t total = get_us() - total_start;
    printf("\r\nTotal: %lu.%03lu ms\r\n",
           (unsigned long)(total / 1000), (unsigned long)(total % 1000));

    return 0;
}
