/**
 * littleOS Sensor Validation Framework
 * 
 * Reads and validates GPIO, ADC sensors, and internal temperature
 * Includes health check and diagnostics reporting
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Forward declarations for driver functions (implement in your drivers) */
extern void gpio_init(uint32_t pin);
extern void gpio_set_dir(uint32_t pin, int dir);
extern uint32_t gpio_get(uint32_t pin);
extern void adc_init(void);
extern void adc_select_input(uint32_t input);
extern uint16_t adc_read(void);
extern void uart_puts(const char *s);
extern int uart_printf(const char *fmt, ...);

/* Memory management */
extern void *kernel_malloc(size_t size);
extern void memory_print_stats(void);
extern void memory_print_stack_status(void);
extern int memory_check_collision(void);

/* GPIO direction constants */
#define GPIO_IN  0
#define GPIO_OUT 1

/* ============================================================================
 * Sensor Registry
 * ============================================================================ */

typedef struct {
    uint32_t pin;              /* GPIO pin number */
    uint32_t min_expected;     /* Minimum expected value */
    uint32_t max_expected;     /* Maximum expected value */
    uint32_t sample_count;     /* Total samples read */
    uint64_t sum;              /* Sum of all readings for average */
    uint32_t min_observed;     /* Minimum value ever observed */
    uint32_t max_observed;     /* Maximum value ever observed */
    uint32_t out_of_range_count; /* Times value was out of range */
    int is_valid;              /* 1 if currently valid, 0 otherwise */
    const char *description;   /* Human-readable name */
} SensorState;

#define MAX_SENSORS 16
static SensorState sensors[MAX_SENSORS];
static int sensor_count = 0;

/* ============================================================================
 * Sensor Registration
 * ============================================================================ */

/**
 * Register a GPIO digital sensor
 * @param pin GPIO pin number
 * @param min_val Minimum expected value (0 or 1)
 * @param max_val Maximum expected value (0 or 1)
 * @param description Human-readable name
 * @return Sensor ID (0-15), or -1 if no space
 */
int sensor_register_gpio(uint32_t pin, uint32_t min_val, 
                         uint32_t max_val, const char *description)
{
    if (sensor_count >= MAX_SENSORS) {
        return -1;  /* No space in registry */
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
    
    /* Initialize GPIO as input */
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    
    sensor_count++;
    return sensor_id;
}

/* ============================================================================
 * Sensor Reading & Validation
 * ============================================================================ */

/**
 * Read and validate a GPIO sensor
 * @param sensor_id Sensor ID from sensor_register_gpio()
 * @param value Pointer to store the read value
 * @return 0 if successful and in range, -1 if out of range or error
 */
int sensor_read_validated(int sensor_id, uint32_t *value)
{
    if (sensor_id < 0 || sensor_id >= sensor_count) {
        return -1;  /* Invalid sensor ID */
    }
    
    SensorState *sensor = &sensors[sensor_id];
    
    /* Read GPIO pin */
    uint32_t raw_value = gpio_get(sensor->pin);
    
    /* Update statistics */
    sensor->sample_count++;
    sensor->sum += raw_value;
    
    if (raw_value < sensor->min_observed) {
        sensor->min_observed = raw_value;
    }
    if (raw_value > sensor->max_observed) {
        sensor->max_observed = raw_value;
    }
    
    /* Validate against expected range */
    if (raw_value < sensor->min_expected || raw_value > sensor->max_expected) {
        sensor->out_of_range_count++;
        sensor->is_valid = 0;
        return -1;  /* Out of range */
    }
    
    sensor->is_valid = 1;
    *value = raw_value;
    return 0;
}

/**
 * Read sensor without validation (just statistics)
 * @param sensor_id Sensor ID
 * @param value Pointer to store value
 * @return Always 0 (value is returned regardless of range)
 */
int sensor_read_raw(int sensor_id, uint32_t *value)
{
    if (sensor_id < 0 || sensor_id >= sensor_count) {
        return -1;
    }
    
    SensorState *sensor = &sensors[sensor_id];
    uint32_t raw_value = gpio_get(sensor->pin);
    
    /* Update statistics only */
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

/* ============================================================================
 * ADC Sensor Reading
 * ============================================================================ */

/**
 * Read ADC input
 * RP2040 has 5 ADC inputs: GPIO26-29 are pins, input 4 is internal temp
 * @param adc_input ADC input channel (0-4)
 * @param value Pointer to store ADC reading (0-4095 for 12-bit)
 * @return 0 on success, -1 on error
 */
int sensor_read_adc(uint32_t adc_input, uint16_t *value)
{
    if (adc_input > 4) {
        return -1;  /* Invalid ADC channel */
    }
    
    adc_init();
    adc_select_input(adc_input);
    
    uint16_t raw = adc_read();
    
    /* Validate 12-bit ADC range */
    if (raw > 4095) {
        return -1;
    }
    
    *value = raw;
    return 0;
}

/**
 * Read internal temperature sensor
 * Returns temperature in Celsius * 100
 * Example: 2500 = 25.00°C
 * 
 * Calculation from RP2040 datasheet:
 * Voltage = raw_adc * 3.3 / 4096
 * Temp (°C) = 27 - (Voltage - 0.706) / 0.001721
 * 
 * @param temp_x100 Pointer to store temperature (°C * 100)
 * @return 0 on success, -1 on error
 */
int sensor_read_temperature(int16_t *temp_x100)
{
    adc_init();
    adc_select_input(4);  /* Internal temperature sensor is channel 4 */
    
    uint16_t raw = adc_read();
    
    /* Convert ADC reading to voltage */
    float voltage = (float)raw * 3.3f / 4096.0f;
    
    /* Apply temperature formula */
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    
    /* Return as °C * 100 */
    *temp_x100 = (int16_t)(temperature * 100.0f);
    
    return 0;
}

/**
 * Read voltage from Pico VSYS pin via ADC
 * Measures the system supply voltage
 * @param voltage_mv Pointer to store voltage in millivolts
 * @return 0 on success, -1 on error
 */
int sensor_read_vsys_voltage(uint16_t *voltage_mv)
{
    uint16_t raw;
    if (sensor_read_adc(3, &raw) != 0) {
        return -1;
    }
    
    /* VSYS is measured through 1/3 divider
     * Voltage = (raw / 4096) * 3.3 * 3 = (raw * 3.3 * 3) / 4096
     */
    uint32_t voltage_uv = (uint32_t)raw * 3300 * 3 / 4096;
    *voltage_mv = voltage_uv / 1000;
    
    return 0;
}

/* ============================================================================
 * Sensor Statistics & Diagnostics
 * ============================================================================ */

/**
 * Print statistics for a single sensor
 * @param sensor_id Sensor ID to report on
 */
void sensor_print_stats(int sensor_id)
{
    if (sensor_id < 0 || sensor_id >= sensor_count) {
        return;
    }
    
    SensorState *sensor = &sensors[sensor_id];
    uint32_t average = sensor->sample_count > 0 ? 
                       (uint32_t)(sensor->sum / sensor->sample_count) : 0;
    
    uart_puts("\n╔════════════════════════════════════════════════════╗\n");
    uart_printf("║ SENSOR %d: %s\n", sensor_id, sensor->description);
    uart_puts("╠════════════════════════════════════════════════════╣\n");
    
    uart_printf("║ GPIO Pin:           %d\n", sensor->pin);
    uart_printf("║ Expected Range:     %d - %d\n", 
                sensor->min_expected, sensor->max_expected);
    uart_printf("║ Min Observed:       %d\n", sensor->min_observed);
    uart_printf("║ Max Observed:       %d\n", sensor->max_observed);
    uart_printf("║ Average:            %d\n", average);
    uart_printf("║ Samples:            %d\n", sensor->sample_count);
    uart_printf("║ Out of Range Count: %d\n", sensor->out_of_range_count);
    
    uart_puts("║ Status:             ");
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
void sensor_print_all_stats(void)
{
    uart_puts("\n");
    uart_puts("╔════════════════════════════════════════════════════╗\n");
    uart_puts("║           SENSOR DIAGNOSTICS                        ║\n");
    uart_puts("╚════════════════════════════════════════════════════╝\n");
    
    for (int i = 0; i < sensor_count; i++) {
        sensor_print_stats(i);
    }
}

/* ============================================================================
 * System Health Check
 * ============================================================================ */

/**
 * Run comprehensive system health check
 * Reports on memory, stack, and sensors
 */
void sensor_health_check(void)
{
    uart_puts("\n");
    uart_puts("╔════════════════════════════════════════════════════╗\n");
    uart_puts("║     LITTLEOS SYSTEM HEALTH CHECK                   ║\n");
    uart_puts("╚════════════════════════════════════════════════════╝\n");
    
    /* ====== Memory Status ====== */
    uart_puts("\n--- MEMORY SYSTEM ---\n");
    memory_print_stats();
    memory_print_stack_status();
    
    if (memory_check_collision()) {
        uart_puts("\n❌ CRITICAL: Heap-stack collision detected!\n");
    }
    
    /* ====== Sensor Status ====== */
    uart_puts("\n--- SENSOR SYSTEM ---\n");
    
    if (sensor_count == 0) {
        uart_puts("No sensors registered\n");
    } else {
        uart_printf("%d sensors registered\n\n", sensor_count);
        sensor_print_all_stats();
    }
    
    /* ====== Overall Status ====== */
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
        uart_puts("║ OVERALL STATUS: ✓ HEALTHY                          ║\n");
    } else if (memory_check_collision()) {
        uart_puts("║ OVERALL STATUS: ❌ CRITICAL                        ║\n");
    } else {
        uart_puts("║ OVERALL STATUS: ⚠️  WARNING                         ║\n");
    }
    
    uart_puts("╚════════════════════════════════════════════════════╝\n\n");
}

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * Simple test to verify GPIO sensors work
 * Reads each sensor 10 times
 */
void sensor_test_gpio_reads(void)
{
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
void sensor_test_adc_reads(void)
{
    uart_puts("\n=== ADC SENSOR TEST ===\n");
    
    for (int ch = 0; ch < 5; ch++) {
        uint16_t value;
        if (sensor_read_adc(ch, &value) == 0) {
            uart_printf("ADC%d: %d (0x%04x)\n", ch, value, value);
        } else {
            uart_printf("ADC%d: Read failed\n", ch);
        }
    }
    
    /* Test temperature */
    int16_t temp_x100;
    if (sensor_read_temperature(&temp_x100) == 0) {
        uart_printf("Temperature: %.2f°C\n", (float)temp_x100 / 100.0f);
    } else {
        uart_puts("Temperature: Read failed\n");
    }
    
    uart_puts("\n=== TEST COMPLETE ===\n");
}
