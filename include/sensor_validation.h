/**
 * littleOS Sensor Validation Framework Header
 */

#ifndef _SENSOR_VALIDATION_H
#define _SENSOR_VALIDATION_H

#include <stdint.h>

/* ============================================================================
 * GPIO Sensor Management
 * ============================================================================ */

/**
 * Register a GPIO digital sensor for monitoring
 * @param pin GPIO pin number (0-29)
 * @param min_val Minimum expected value (usually 0)
 * @param max_val Maximum expected value (usually 1)
 * @param description Human-readable sensor name (e.g., "LED", "Button")
 * @return Sensor ID (0-15) on success, -1 if registry full
 */
int sensor_register_gpio(uint32_t pin, uint32_t min_val, 
                         uint32_t max_val, const char *description);

/**
 * Read GPIO sensor with validation
 * Compares reading against expected range
 * @param sensor_id Sensor ID from sensor_register_gpio()
 * @param value Pointer to uint32_t to receive reading
 * @return 0 if in range, -1 if out of range or invalid sensor
 */
int sensor_read_validated(int sensor_id, uint32_t *value);

/**
 * Read GPIO sensor without validation
 * Useful for collecting statistics on noisy signals
 * @param sensor_id Sensor ID
 * @param value Pointer to uint32_t to receive reading
 * @return 0 on success, -1 if invalid sensor
 */
int sensor_read_raw(int sensor_id, uint32_t *value);

/* ============================================================================
 * ADC Sensor Reading
 * ============================================================================ */

/**
 * Read ADC channel
 * RP2040 ADC channels:
 *   0-3: GPIO26-GPIO29 (analog input pins)
 *   4: Internal temperature sensor
 * 
 * @param adc_input Channel number (0-4)
 * @param value Pointer to uint16_t to receive 12-bit reading (0-4095)
 * @return 0 on success, -1 on error
 */
int sensor_read_adc(uint32_t adc_input, uint16_t *value);

/**
 * Read internal temperature sensor
 * Converts ADC reading to degrees Celsius using RP2040 formula
 * @param temp_x100 Pointer to int16_t (output in °C * 100)
 *                  Example: 2500 = 25.00°C
 * @return 0 on success, -1 on error
 */
int sensor_read_temperature(int16_t *temp_x100);

/**
 * Read system supply voltage (VSYS)
 * Measures 3.3V rail through ADC divider
 * @param voltage_mv Pointer to uint16_t (output in millivolts)
 * @return 0 on success, -1 on error
 */
int sensor_read_vsys_voltage(uint16_t *voltage_mv);

/* ============================================================================
 * Diagnostics & Statistics
 * ============================================================================ */

/**
 * Print statistics for a single sensor
 * Shows: pin, expected range, min/max observed, average, sample count
 * Output goes to UART
 * @param sensor_id Sensor ID to report
 */
void sensor_print_stats(int sensor_id);

/**
 * Print statistics for all registered sensors
 */
void sensor_print_all_stats(void);

/**
 * Run comprehensive system health check
 * Reports on:
 * - Memory usage and status
 * - Stack usage and collision detection
 * - All sensor readings and statistics
 * 
 * Output goes to UART (formatted for readability)
 */
void sensor_health_check(void);

/* ============================================================================
 * Test Functions
 * ============================================================================ */

/**
 * Test GPIO sensor reads
 * Reads each registered sensor 10 times and prints results
 */
void sensor_test_gpio_reads(void);

/**
 * Test ADC sensor reads
 * Reads all ADC channels and internal temperature
 */
void sensor_test_adc_reads(void);

#endif /* _SENSOR_VALIDATION_H */
