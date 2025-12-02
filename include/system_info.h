// include/system_info.h
// System Information Interface for littleOS
#ifndef LITTLEOS_SYSTEM_INFO_H
#define LITTLEOS_SYSTEM_INFO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CPU information structure
 */
typedef struct {
    uint32_t clock_speed_hz;    // CPU clock frequency in Hz
    uint32_t core_count;         // Number of cores
    char* chip_model;            // Chip model name
    uint32_t chip_revision;      // Chip revision number
} cpu_info_t;

/**
 * @brief Memory information structure
 */
typedef struct {
    uint32_t total_ram;          // Total RAM in bytes
    uint32_t free_ram;           // Free RAM in bytes
    uint32_t used_ram;           // Used RAM in bytes
    uint32_t flash_size;         // Flash size in bytes
} memory_info_t;

/**
 * @brief System uptime information
 */
typedef struct {
    uint64_t uptime_ms;          // System uptime in milliseconds
    uint32_t uptime_seconds;     // Uptime in seconds
    uint32_t uptime_minutes;     // Uptime in minutes
    uint32_t uptime_hours;       // Uptime in hours
    uint32_t uptime_days;        // Uptime in days
} uptime_info_t;

/**
 * @brief Get CPU information
 * @param info Pointer to cpu_info_t structure to fill
 * @return true on success, false on error
 */
bool system_get_cpu_info(cpu_info_t* info);

/**
 * @brief Get memory information
 * @param info Pointer to memory_info_t structure to fill
 * @return true on success, false on error
 */
bool system_get_memory_info(memory_info_t* info);

/**
 * @brief Get system uptime
 * @param info Pointer to uptime_info_t structure to fill
 * @return true on success, false on error
 */
bool system_get_uptime(uptime_info_t* info);

/**
 * @brief Get system temperature in Celsius
 * @return Temperature in degrees Celsius, or -273.0 on error
 */
float system_get_temperature(void);

/**
 * @brief Get unique board ID
 * @param buffer Buffer to store ID string (min 17 bytes for RP2040)
 * @param buffer_size Size of buffer
 * @return true on success, false on error
 */
bool system_get_board_id(char* buffer, size_t buffer_size);

/**
 * @brief Get littleOS version string
 * @return Version string (e.g., "0.2.0")
 */
const char* system_get_version(void);

/**
 * @brief Get build date/time string
 * @return Build timestamp string
 */
const char* system_get_build_date(void);

/**
 * @brief Print formatted system information to stdout
 */
void system_print_info(void);

#ifdef __cplusplus
}
#endif

#endif // LITTLEOS_SYSTEM_INFO_H
