/**
 * littleOS Sensor Validation Framework
 * Reads and validates GPIO, ADC sensors, and internal temperature
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern void gpio_init(uint32_t pin);
extern void gpio_set_dir(uint32_t pin, int dir);
extern uint32_t gpio_get(uint32_t pin);
extern void adc_init(void);
extern void adc_select_input(uint32_t input);
extern uint16_t adc_read(void);
extern void uart_puts(const char *s);
extern int uart_printf(const char *fmt, ...);

extern void *kernel_malloc(size_t size);
extern void memory_print_stats(void);
extern void memory_print_stack_status(void);
extern int memory_check_collision(void);

#define GPIO_IN 0
#define GPIO_OUT 1

typedef struct {
    uint32_t pin;
    uint32_t min_expected;
    uint32_t max_expected;
    uint32_t sample_count;
    uint64_t sum;
    uint32_t min_observed;
    uint32_t max_observed;
    uint32_t out_of_range_count;
    int is_valid;
    const char *description;
} SensorState;

#define MAX_SENSORS 16
static SensorState sensors[MAX_SENSORS];
static int sensor_count = 0;

/**
 * Register a GPIO digital sensor
 */
int sensor_register_gpio(uint32_t pin, uint32_t min_val,
                         uint32_t max_val, const char *description) {
    if (sensor_count >= MAX_SENSORS) {
        return -1;
    }

    int sensor_id = sensor_count;
    SensorState *sensor = &sensors[sensor_id];

    sensor->pin = pin;
    sensor->min_expected = min_val;
    sensor->max_expected = max_val;
    sensor->sample_count = 0;
    sensor->sum = 0;
    sensor->min_observed = 0xFFFFFFFF;
    sensor->max_observed = 0;
    sensor->out_of_range_count = 0;
    sensor->is_valid = 1;
    sensor->description = description;

    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);

    sensor_count++;
    return sensor_id;
}

/**
 * Read and validate a GPIO sensor
 */
int sensor_read_validated(int sensor_id, uint32_t *value) {
    if (sensor_id < 0 || sensor_id >= sensor_count) {
        return -1;
    }

    SensorState *sensor = &sensors[sensor_id];
    uint32_t raw_value = gpio_get(sensor->pin);

    sensor->sample_count++;
    sensor->sum += raw_value;

    if (raw_value < sensor->min_observed) {
        sensor->min_observed = raw_value;
    }

    if (raw_value > sensor->max_observed) {
        sensor->max_observed = raw_value;
    }

    if (raw_value < sensor->min_expected || raw_value > sensor->max_expected) {
        sensor->out_of_range_count++;
        sensor->is_valid = 0;
        return -1;
    }

    sensor->is_valid = 1;
    *value = raw_value;
    return 0;
}

/**
 * Read sensor without validation
 */
int sensor_read_raw(int sensor_id, uint32_t *value) {
    if (sensor_id < 0 || sensor_id >= sensor_count) {
        return -1;
    }

    SensorState *sensor = &sensors[sensor_id];
    uint32_t raw_value = gpio_get(sensor->pin);

    sensor->sample_count++;
    sensor->sum += raw_value;

    if (raw_value < sensor->min_observed) {
        sensor->min_observed = raw_value;
    }

    if (raw_value > sensor->max_observed) {
        sensor->max_observed = raw_value;
    }

    *value = raw_value;
    return 0;
}

/**
 * Read ADC input
 */
int sensor_read_adc(uint32_t adc_input, uint16_t *value) {
    if (adc_input > 4) {
        return -1;
    }

    adc_init();
    adc_select_input(adc_input);
    uint16_t raw = adc_read();

    if (raw > 4095) {
        return -1;
    }

    *value = raw;
    return 0;
}

/**
 * Read internal temperature sensor
 */
int sensor_read_temperature(int16_t *temp_x100) {
    adc_init();
    adc_select_input(4);
    uint16_t raw = adc_read();

    float voltage = (float)raw * 3.3f / 4096.0f;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;

    *temp_x100 = (int16_t)(temperature * 100.0f);
    return 0;
}

/**
 * Read voltage from Pico VSYS pin via ADC
 */
int sensor_read_vsys_voltage(uint16_t *voltage_mv) {
    uint16_t raw;
    if (sensor_read_adc(3, &raw) != 0) {
        return -1;
    }

    uint32_t voltage_uv = (uint32_t)raw * 3300 * 3 / 4096;
    *voltage_mv = voltage_uv / 1000;
    return 0;
}

/**
 * Print statistics for a single sensor
 */
void sensor_print_stats(int sensor_id) {
    if (sensor_id < 0 || sensor_id >= sensor_count) {
        return;
    }

    SensorState *sensor = &sensors[sensor_id];
    uint32_t average = sensor->sample_count > 0 ?
        (uint32_t)(sensor->sum / sensor->sample_count) : 0;

    uart_puts("\n╔════════════════════════════════════════════════════╗\n");
    uart_printf("║ SENSOR %d: %s\n", sensor_id, sensor->description);
    uart_puts("╠════════════════════════════════════════════════════╣\n");
    uart_printf("║ GPIO Pin: %d\n", sensor->pin);
    uart_printf("║ Expected Range: %d - %d\n",
        sensor->min_expected, sensor->max_expected);
    uart_printf("║ Min Observed: %d\n", sensor->min_observed);
    uart_printf("║ Max Observed: %d\n", sensor->max_observed);
    uart_printf("║ Average: %d\n", average);
    uart_printf("║ Samples: %d\n", sensor->sample_count);
    uart_printf("║ Out of Range Count: %d\n", sensor->out_of_range_count);
    uart_puts("║ Status: ");
    if (sensor->is_valid) {
        uart_puts("✓ VALID");
    } else {
        uart_puts("❌ OUT_OF_RANGE");
    }
    uart_puts("\n");
    uart_puts("╚════════════════════════════════════════════════════╝\n");
}

/**
 * Print all sensor statistics
 */
void sensor_print_all_stats(void) {
    uart_puts("\n");
    uart_puts("╔════════════════════════════════════════════════════╗\n");
    uart_puts("║ SENSOR DIAGNOSTICS                                ║\n");
    uart_puts("╚════════════════════════════════════════════════════╝\n");

    for (int i = 0; i < sensor_count; i++) {
        sensor_print_stats(i);
    }
}

/**
 * Run comprehensive system health check
 */
void sensor_health_check(void) {
    uart_puts("\n");
    uart_puts("╔════════════════════════════════════════════════════╗\n");
    uart_puts("║ LITTLEOS SYSTEM HEALTH CHECK                       ║\n");
    uart_puts("╚════════════════════════════════════════════════════╝\n");

    uart_puts("\n--- MEMORY SYSTEM ---\n");
    memory_print_stats();
    memory_print_stack_status();
    if (memory_check_collision()) {
        uart_puts("\n❌ CRITICAL: Heap-stack collision detected!\n");
    }

    uart_puts("\n--- SENSOR SYSTEM ---\n");
    if (sensor_count == 0) {
        uart_puts("No sensors registered\n");
    } else {
        uart_printf("%d sensors registered\n\n", sensor_count);
        sensor_print_all_stats();
    }

    uart_puts("\n");
    uart_puts("╔════════════════════════════════════════════════════╗\n");
    int all_sensors_valid = 1;
    for (int i = 0; i < sensor_count; i++) {
        if (!sensors[i].is_valid) {
            all_sensors_valid = 0;
            break;
        }
    }

    if (!memory_check_collision() && all_sensors_valid) {
        uart_puts("║ OVERALL STATUS: ✓ HEALTHY                         ║\n");
    } else if (memory_check_collision()) {
        uart_puts("║ OVERALL STATUS: ❌ CRITICAL                        ║\n");
    } else {
        uart_puts("║ OVERALL STATUS: ⚠️  WARNING                         ║\n");
    }

    uart_puts("╚════════════════════════════════════════════════════╝\n\n");
}

/**
 * Simple test to verify GPIO sensors work
 */
void sensor_test_gpio_reads(void) {
    uart_puts("\n=== GPIO SENSOR TEST ===\n");
    uart_printf("Testing %d sensors, 10 reads each:\n\n", sensor_count);

    for (int sensor_id = 0; sensor_id < sensor_count; sensor_id++) {
        SensorState *sensor = &sensors[sensor_id];
        uart_printf("Sensor %d (%s, GPIO%d): ",
            sensor_id, sensor->description, sensor->pin);

        for (int i = 0; i < 10; i++) {
            uint32_t value;
            if (sensor_read_raw(sensor_id, &value) == 0) {
                uart_printf("%d ", value);
            } else {
                uart_puts("E ");
            }
        }
        uart_puts("\n");
    }

    uart_puts("\n=== TEST COMPLETE ===\n");
}

/**
 * Test ADC readings
 */
void sensor_test_adc_reads(void) {
    uart_puts("\n=== ADC SENSOR TEST ===\n");

    for (int ch = 0; ch < 5; ch++) {
        uint16_t value;
        if (sensor_read_adc(ch, &value) == 0) {
            uart_printf("ADC%d: %d (0x%04x)\n", ch, value, value);
        } else {
            uart_printf("ADC%d: Read failed\n", ch);
        }
    }

    int16_t temp_x100;
    if (sensor_read_temperature(&temp_x100) == 0) {
        uart_printf("Temperature: %.2f°C\n", (float)temp_x100 / 100.0f);
    } else {
        uart_puts("Temperature: Read failed\n");
    }

    uart_puts("\n=== TEST COMPLETE ===\n");
}
