/* cmd_selftest.c - Built-in self-test framework for littleOS */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#endif

#include "board/board_config.h"
#include "memory_segmented.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static void test_result(const char *name, bool pass, const char *detail) {
    tests_run++;
    if (pass) {
        tests_passed++;
        printf("  [PASS] %s", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s", name);
    }
    if (detail && detail[0]) printf(" - %s", detail);
    printf("\r\n");
}

/* RAM test: write patterns and verify */
static void test_ram(void) {
    printf("\r\n--- RAM Tests ---\r\n");

    /* Pattern test on a small buffer */
    static uint32_t test_buf[64];
    bool pass = true;
    char detail[64];

    /* Walking ones */
    for (int i = 0; i < 64; i++) test_buf[i] = (1U << (i % 32));
    for (int i = 0; i < 64; i++) {
        if (test_buf[i] != (1U << (i % 32))) { pass = false; break; }
    }
    test_result("RAM walking ones", pass, pass ? "256 bytes OK" : "pattern mismatch");

    /* All-zeros */
    memset(test_buf, 0, sizeof(test_buf));
    pass = true;
    for (int i = 0; i < 64; i++) {
        if (test_buf[i] != 0) { pass = false; break; }
    }
    test_result("RAM all-zeros", pass, pass ? "256 bytes OK" : "zero check failed");

    /* All-ones */
    memset(test_buf, 0xFF, sizeof(test_buf));
    pass = true;
    for (int i = 0; i < 64; i++) {
        if (test_buf[i] != 0xFFFFFFFF) { pass = false; break; }
    }
    test_result("RAM all-ones", pass, pass ? "256 bytes OK" : "ones check failed");

    /* Address-as-data */
    for (int i = 0; i < 64; i++) test_buf[i] = (uint32_t)(uintptr_t)&test_buf[i];
    pass = true;
    for (int i = 0; i < 64; i++) {
        if (test_buf[i] != (uint32_t)(uintptr_t)&test_buf[i]) { pass = false; break; }
    }
    test_result("RAM address-as-data", pass, pass ? "64 words OK" : "addr pattern failed");
}

/* Memory allocator test */
static void test_memory(void) {
    printf("\r\n--- Memory Allocator Tests ---\r\n");

    MemoryStats stats = memory_get_stats();
    test_result("Kernel heap exists", stats.kernel_free > 0,
                stats.kernel_free > 0 ? "heap accessible" : "no free space");
    test_result("Interpreter heap exists", stats.interpreter_free > 0,
                stats.interpreter_free > 0 ? "heap accessible" : "no free space");

    /* Stack check */
    uint32_t stack_free = stack_get_free_space();
    char detail[48];
    snprintf(detail, sizeof(detail), "%lu bytes free", (unsigned long)stack_free);
    test_result("Stack space", stack_free > 512, detail);

    /* Layout validation */
    test_result("Memory layout valid", memory_validate_layout() == 1, "no overlaps");
    test_result("No heap collision", memory_check_collision() == 0, "heaps within bounds");
}

/* GPIO test */
static void test_gpio(void) {
    printf("\r\n--- GPIO Tests ---\r\n");

#ifdef PICO_BUILD
    /* Test LED pin (GPIO 25) toggle */
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, 1);
    sleep_us(100);
    bool led_high = gpio_get(25);
    gpio_put(25, 0);
    sleep_us(100);
    bool led_low = gpio_get(25);
    test_result("GPIO 25 (LED) output", led_high && !led_low, "toggle verified");

    /* Test GPIO read on unconnected pin (should be low or pull-down) */
    gpio_init(2);
    gpio_set_dir(2, GPIO_IN);
    gpio_pull_down(2);
    sleep_us(10);
    bool pin2 = gpio_get(2);
    test_result("GPIO 2 pull-down", !pin2, "reads low with pull-down");
#else
    test_result("GPIO (emulated)", true, "skipped on emulator");
#endif
}

/* ADC test */
static void test_adc(void) {
    printf("\r\n--- ADC Tests ---\r\n");

#ifdef PICO_BUILD
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4); /* Internal temperature sensor */

    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / (1 << 12);
    float temp = 27.0f - (voltage - 0.706f) / (-0.001721f);

    char detail[48];
    snprintf(detail, sizeof(detail), "raw=%u, %.1f°C", raw, temp);
    test_result("ADC temp sensor", raw > 0 && raw < 4096, detail);
    test_result("Temperature sane", temp > -10.0f && temp < 85.0f, detail);
#else
    test_result("ADC (emulated)", true, "skipped on emulator");
#endif
}

/* Timer test */
static void test_timer(void) {
    printf("\r\n--- Timer Tests ---\r\n");

#ifdef PICO_BUILD
    uint32_t t1 = time_us_32();
    sleep_us(1000); /* 1ms */
    uint32_t t2 = time_us_32();
    uint32_t elapsed = t2 - t1;

    char detail[48];
    snprintf(detail, sizeof(detail), "1ms sleep = %lu us", (unsigned long)elapsed);
    /* Allow 20% tolerance */
    test_result("Timer accuracy", elapsed >= 800 && elapsed <= 1200, detail);

    /* Monotonicity test */
    bool mono = true;
    uint32_t prev = time_us_32();
    for (int i = 0; i < 100; i++) {
        uint32_t now = time_us_32();
        if (now < prev) { mono = false; break; }
        prev = now;
    }
    test_result("Timer monotonic", mono, "100 samples");
#else
    test_result("Timer (emulated)", true, "skipped on emulator");
#endif
}

/* Flash test (read-only) */
static void test_flash(void) {
    printf("\r\n--- Flash Tests ---\r\n");

#ifdef PICO_BUILD
    /* Read from XIP base - should be our program */
    const uint8_t *flash = (const uint8_t *)XIP_BASE;
    bool readable = (flash[0] != 0 || flash[1] != 0); /* Boot2 shouldn't be all zeros */
    test_result("Flash readable", readable, "XIP base accessible");

    /* Check we can read a large range */
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i++) sum += flash[i];
    test_result("Flash consistency", sum > 0, "1KB checksum non-zero");
#else
    test_result("Flash (emulated)", true, "skipped on emulator");
#endif
}

int cmd_selftest(int argc, char *argv[]) {
    printf("=== littleOS Self-Test Suite ===\r\n");
    printf("Platform: %s %s\r\n", CHIP_MODEL_STR, CHIP_CORE_STR);

    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;

    bool run_all = (argc < 2);

    if (run_all || (argc >= 2 && strcmp(argv[1], "ram") == 0)) test_ram();
    if (run_all || (argc >= 2 && strcmp(argv[1], "memory") == 0)) test_memory();
    if (run_all || (argc >= 2 && strcmp(argv[1], "gpio") == 0)) test_gpio();
    if (run_all || (argc >= 2 && strcmp(argv[1], "adc") == 0)) test_adc();
    if (run_all || (argc >= 2 && strcmp(argv[1], "timer") == 0)) test_timer();
    if (run_all || (argc >= 2 && strcmp(argv[1], "flash") == 0)) test_flash();

    if (argc >= 2 && strcmp(argv[1], "help") == 0) {
        printf("Usage: selftest [all|ram|memory|gpio|adc|timer|flash]\r\n");
        return 0;
    }

    printf("\r\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\r\n");

    return tests_failed > 0 ? 1 : 0;
}
