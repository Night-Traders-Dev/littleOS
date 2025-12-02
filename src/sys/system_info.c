// src/system_info.c
// System Information Implementation for RP2040
#include "system_info.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <string.h>

// Version information
#define LITTLEOS_VERSION "0.2.0"
#define LITTLEOS_BUILD_DATE __DATE__ " " __TIME__

// RP2040 specifications
#define RP2040_RAM_SIZE (264 * 1024)      // 264KB SRAM
#define RP2040_FLASH_SIZE (2 * 1024 * 1024) // 2MB Flash (typical)
#define RP2040_CORE_COUNT 2

// External symbols for heap tracking (defined by linker)
extern char __HeapLimit;
extern char __StackLimit;
extern char end;

/**
 * @brief Get CPU information
 */
bool system_get_cpu_info(cpu_info_t* info) {
    if (!info) return false;
    
    info->clock_speed_hz = clock_get_hz(clk_sys);
    info->core_count = RP2040_CORE_COUNT;
    info->chip_model = "RP2040";
    info->chip_revision = rp2040_chip_version();
    
    return true;
}

/**
 * @brief Estimate free RAM (simplified approach)
 */
static uint32_t get_free_ram(void) {
    // This is a simplified estimation
    // For accurate measurement, implement proper heap tracking
    char stack_var;
    uint32_t heap_end = (uint32_t)&end;
    uint32_t stack_ptr = (uint32_t)&stack_var;
    
    if (stack_ptr > heap_end) {
        return stack_ptr - heap_end;
    }
    return 0;
}

/**
 * @brief Get memory information
 */
bool system_get_memory_info(memory_info_t* info) {
    if (!info) return false;
    
    info->total_ram = RP2040_RAM_SIZE;
    info->flash_size = RP2040_FLASH_SIZE;
    info->free_ram = get_free_ram();
    info->used_ram = info->total_ram - info->free_ram;
    
    return true;
}

/**
 * @brief Get system uptime
 */
bool system_get_uptime(uptime_info_t* info) {
    if (!info) return false;
    
    info->uptime_ms = to_ms_since_boot(get_absolute_time());
    info->uptime_seconds = info->uptime_ms / 1000;
    info->uptime_minutes = info->uptime_seconds / 60;
    info->uptime_hours = info->uptime_minutes / 60;
    info->uptime_days = info->uptime_hours / 24;
    
    return true;
}

/**
 * @brief Get system temperature from RP2040 internal sensor
 */
float system_get_temperature(void) {
    // RP2040 has internal temperature sensor on ADC channel 4
    static bool adc_initialized = false;
    
    if (!adc_initialized) {
        adc_init();
        adc_set_temp_sensor_enabled(true);
        adc_initialized = true;
    }
    
    adc_select_input(4);  // Select temperature sensor
    uint16_t raw = adc_read();
    
    // Convert to temperature using RP2040 formula
    // T = 27 - (ADC_voltage - 0.706) / 0.001721
    const float conversion_factor = 3.3f / (1 << 12);  // 12-bit ADC
    float voltage = raw * conversion_factor;
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    
    return temperature;
}

/**
 * @brief Get unique board ID
 */
bool system_get_board_id(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 17) return false;
    
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    
    // Format as hex string: XXXXXXXXXXXXXXXX (16 hex chars)
    snprintf(buffer, buffer_size, "%02X%02X%02X%02X%02X%02X%02X%02X",
             id.id[0], id.id[1], id.id[2], id.id[3],
             id.id[4], id.id[5], id.id[6], id.id[7]);
    
    return true;
}

/**
 * @brief Get littleOS version string
 */
const char* system_get_version(void) {
    return LITTLEOS_VERSION;
}

/**
 * @brief Get build date/time string
 */
const char* system_get_build_date(void) {
    return LITTLEOS_BUILD_DATE;
}

/**
 * @brief Print formatted system information to stdout
 */
void system_print_info(void) {
    cpu_info_t cpu;
    memory_info_t mem;
    uptime_info_t uptime;
    char board_id[17];
    
    printf("\r\n");
    printf("=================================\r\n");
    printf("    littleOS System Information\r\n");
    printf("=================================\r\n\r\n");
    
    // OS Information
    printf("OS Version:    %s\r\n", system_get_version());
    printf("Build Date:    %s\r\n\r\n", system_get_build_date());
    
    // CPU Information
    if (system_get_cpu_info(&cpu)) {
        printf("--- CPU Information ---\r\n");
        printf("Model:         %s\r\n", cpu.chip_model);
        printf("Revision:      %u\r\n", cpu.chip_revision);
        printf("Cores:         %u\r\n", cpu.core_count);
        printf("Clock Speed:   %u MHz\r\n\r\n", cpu.clock_speed_hz / 1000000);
    }
    
    // Memory Information
    if (system_get_memory_info(&mem)) {
        printf("--- Memory Information ---\r\n");
        printf("Total RAM:     %u KB\r\n", mem.total_ram / 1024);
        printf("Used RAM:      %u KB\r\n", mem.used_ram / 1024);
        printf("Free RAM:      %u KB\r\n", mem.free_ram / 1024);
        printf("Flash Size:    %u MB\r\n\r\n", mem.flash_size / (1024 * 1024));
    }
    
    // Uptime Information
    if (system_get_uptime(&uptime)) {
        printf("--- System Uptime ---\r\n");
        if (uptime.uptime_days > 0) {
            printf("Uptime:        %u days, %u hours, %u min\r\n",
                   uptime.uptime_days,
                   uptime.uptime_hours % 24,
                   uptime.uptime_minutes % 60);
        } else if (uptime.uptime_hours > 0) {
            printf("Uptime:        %u hours, %u min, %u sec\r\n",
                   uptime.uptime_hours,
                   uptime.uptime_minutes % 60,
                   uptime.uptime_seconds % 60);
        } else if (uptime.uptime_minutes > 0) {
            printf("Uptime:        %u min, %u sec\r\n",
                   uptime.uptime_minutes,
                   uptime.uptime_seconds % 60);
        } else {
            printf("Uptime:        %u seconds\r\n", uptime.uptime_seconds);
        }
        printf("\r\n");
    }
    
    // Temperature
    float temp = system_get_temperature();
    if (temp > -200.0f) {  // Sanity check
        printf("--- Sensors ---\r\n");
        printf("Temperature:   %.1f°C\r\n\r\n", temp);
    }
    
    // Board ID
    if (system_get_board_id(board_id, sizeof(board_id))) {
        printf("--- Hardware ---\r\n");
        printf("Board ID:      %s\r\n", board_id);
    }
    
    printf("\r\n=================================\r\n\r\n");
}
