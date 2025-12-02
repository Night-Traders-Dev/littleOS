// include/config_storage.h
// Persistent Configuration Storage for littleOS
#ifndef LITTLEOS_CONFIG_STORAGE_H
#define LITTLEOS_CONFIG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration storage limits
#define CONFIG_MAX_KEY_LEN 32
#define CONFIG_MAX_VALUE_LEN 256
#define CONFIG_MAX_ENTRIES 32
#define CONFIG_AUTOBOOT_SCRIPT_SIZE 2048

// Configuration result codes
typedef enum {
    CONFIG_OK = 0,
    CONFIG_ERROR_NOT_FOUND = 1,
    CONFIG_ERROR_FULL = 2,
    CONFIG_ERROR_INVALID_KEY = 3,
    CONFIG_ERROR_INVALID_VALUE = 4,
    CONFIG_ERROR_FLASH = 5,
    CONFIG_ERROR_CORRUPT = 6
} config_result_t;

/**
 * @brief Initialize configuration storage system
 * Loads configuration from flash or initializes defaults
 * @return true on success, false on error
 */
bool config_init(void);

/**
 * @brief Set a configuration value
 * @param key Configuration key (max CONFIG_MAX_KEY_LEN)
 * @param value Configuration value (max CONFIG_MAX_VALUE_LEN)
 * @return CONFIG_OK on success, error code otherwise
 */
config_result_t config_set(const char* key, const char* value);

/**
 * @brief Get a configuration value
 * @param key Configuration key
 * @param value Buffer to store value
 * @param value_size Size of value buffer
 * @return CONFIG_OK on success, error code otherwise
 */
config_result_t config_get(const char* key, char* value, size_t value_size);

/**
 * @brief Delete a configuration entry
 * @param key Configuration key to delete
 * @return CONFIG_OK on success, error code otherwise
 */
config_result_t config_delete(const char* key);

/**
 * @brief Check if a configuration key exists
 * @param key Configuration key to check
 * @return true if exists, false otherwise
 */
bool config_exists(const char* key);

/**
 * @brief Save configuration to flash
 * @return true on success, false on error
 */
bool config_save(void);

/**
 * @brief Load configuration from flash
 * @return true on success, false on error
 */
bool config_load(void);

/**
 * @brief Clear all configuration (factory reset)
 * @return true on success, false on error
 */
bool config_clear(void);

/**
 * @brief List all configuration keys
 * @param keys Array to store key pointers
 * @param max_keys Maximum number of keys to return
 * @return Number of keys returned
 */
int config_list_keys(const char** keys, int max_keys);

/**
 * @brief Get number of stored configuration entries
 * @return Number of entries
 */
int config_count(void);

/**
 * @brief Set autoboot script
 * @param script Script content to run on boot
 * @return true on success, false on error
 */
bool config_set_autoboot(const char* script);

/**
 * @brief Get autoboot script
 * @param script Buffer to store script
 * @param script_size Size of script buffer
 * @return true on success, false if no autoboot script
 */
bool config_get_autoboot(char* script, size_t script_size);

/**
 * @brief Check if autoboot is enabled
 * @return true if autoboot script exists, false otherwise
 */
bool config_has_autoboot(void);

/**
 * @brief Clear autoboot script
 * @return true on success, false on error
 */
bool config_clear_autoboot(void);

/**
 * @brief Print all configuration entries to stdout
 */
void config_print_all(void);

/**
 * @brief Get configuration storage statistics
 * @param used_entries Pointer to store number of used entries
 * @param total_entries Pointer to store total available entries
 * @param flash_used Pointer to store bytes used in flash
 * @param flash_total Pointer to store total flash allocated
 */
void config_get_stats(int* used_entries, int* total_entries, 
                      size_t* flash_used, size_t* flash_total);

#ifdef __cplusplus
}
#endif

#endif // LITTLEOS_CONFIG_STORAGE_H
