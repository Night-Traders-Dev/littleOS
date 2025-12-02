// src/config_storage.c
// Persistent Configuration Storage Implementation
#include "config_storage.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Flash storage configuration
// Use last sector of flash (2MB Pico = 0x200000 bytes)
// Reserve 4KB (1 flash sector) at end of flash
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CONFIG_MAGIC 0x434F4E46  // "CONF" in hex
#define CONFIG_VERSION 1

// Configuration entry structure
typedef struct {
    char key[CONFIG_MAX_KEY_LEN];
    char value[CONFIG_MAX_VALUE_LEN];
    bool used;
} config_entry_t;

// Configuration storage structure (stored in flash)
typedef struct {
    uint32_t magic;           // Magic number for validation
    uint32_t version;         // Config format version
    uint32_t entry_count;     // Number of entries
    uint32_t checksum;        // Simple checksum
    config_entry_t entries[CONFIG_MAX_ENTRIES];
    char autoboot_script[CONFIG_AUTOBOOT_SCRIPT_SIZE];
    bool autoboot_enabled;
} config_storage_t;

// RAM copy of configuration
static config_storage_t config_data;
static bool config_initialized = false;
static bool config_dirty = false;  // Needs saving

// Calculate simple checksum
static uint32_t calculate_checksum(const config_storage_t* cfg) {
    uint32_t sum = 0;
    const uint8_t* data = (const uint8_t*)cfg;
    
    // Skip magic, version, entry_count, and checksum fields
    size_t start = offsetof(config_storage_t, entries);
    size_t end = sizeof(config_storage_t);
    
    for (size_t i = start; i < end; i++) {
        sum += data[i];
    }
    
    return sum;
}

// Validate configuration structure
static bool validate_config(const config_storage_t* cfg) {
    if (cfg->magic != CONFIG_MAGIC) {
        return false;
    }
    
    if (cfg->version != CONFIG_VERSION) {
        return false;
    }
    
    if (cfg->entry_count > CONFIG_MAX_ENTRIES) {
        return false;
    }
    
    uint32_t expected_checksum = calculate_checksum(cfg);
    if (cfg->checksum != expected_checksum) {
        return false;
    }
    
    return true;
}

// Initialize default configuration
static void init_defaults(void) {
    memset(&config_data, 0, sizeof(config_data));
    config_data.magic = CONFIG_MAGIC;
    config_data.version = CONFIG_VERSION;
    config_data.entry_count = 0;
    config_data.autoboot_enabled = false;
    
    // Mark all entries as unused
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        config_data.entries[i].used = false;
    }
}

/**
 * @brief Initialize configuration storage
 */
bool config_init(void) {
    if (config_initialized) {
        return true;
    }
    
    printf("Config: Initializing...\r\n");
    
    // Try to load from flash
    if (config_load()) {
        printf("Config: Loaded %d entries from flash\r\n", config_data.entry_count);
        config_initialized = true;
        return true;
    }
    
    // No valid config in flash, use defaults
    printf("Config: No valid configuration found, using defaults\r\n");
    init_defaults();
    config_initialized = true;
    config_dirty = true;
    
    // Save defaults to flash
    config_save();
    
    return true;
}

/**
 * @brief Load configuration from flash
 */
bool config_load(void) {
    const config_storage_t* flash_config = 
        (const config_storage_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    
    // Validate flash contents
    if (!validate_config(flash_config)) {
        return false;
    }
    
    // Copy to RAM
    memcpy(&config_data, flash_config, sizeof(config_storage_t));
    config_dirty = false;
    
    return true;
}

/**
 * @brief Save configuration to flash
 */
bool config_save(void) {
    if (!config_initialized) {
        return false;
    }
    
    // Update checksum
    config_data.checksum = calculate_checksum(&config_data);
    
    printf("Config: Saving to flash...\r\n");
    
    // Disable interrupts during flash write
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase flash sector (4KB)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    // Write configuration to flash (must be 256-byte aligned)
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&config_data, 
                       sizeof(config_storage_t));
    
    // Re-enable interrupts
    restore_interrupts(ints);
    
    config_dirty = false;
    printf("Config: Saved successfully\r\n");
    
    return true;
}

/**
 * @brief Set configuration value
 */
config_result_t config_set(const char* key, const char* value) {
    if (!config_initialized) {
        return CONFIG_ERROR_FLASH;
    }
    
    if (!key || strlen(key) == 0 || strlen(key) >= CONFIG_MAX_KEY_LEN) {
        return CONFIG_ERROR_INVALID_KEY;
    }
    
    if (!value || strlen(value) >= CONFIG_MAX_VALUE_LEN) {
        return CONFIG_ERROR_INVALID_VALUE;
    }
    
    // Check if key already exists
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if (config_data.entries[i].used && 
            strcmp(config_data.entries[i].key, key) == 0) {
            // Update existing entry
            strncpy(config_data.entries[i].value, value, CONFIG_MAX_VALUE_LEN - 1);
            config_data.entries[i].value[CONFIG_MAX_VALUE_LEN - 1] = '\0';
            config_dirty = true;
            return CONFIG_OK;
        }
    }
    
    // Find empty slot
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if (!config_data.entries[i].used) {
            strncpy(config_data.entries[i].key, key, CONFIG_MAX_KEY_LEN - 1);
            config_data.entries[i].key[CONFIG_MAX_KEY_LEN - 1] = '\0';
            strncpy(config_data.entries[i].value, value, CONFIG_MAX_VALUE_LEN - 1);
            config_data.entries[i].value[CONFIG_MAX_VALUE_LEN - 1] = '\0';
            config_data.entries[i].used = true;
            config_data.entry_count++;
            config_dirty = true;
            return CONFIG_OK;
        }
    }
    
    return CONFIG_ERROR_FULL;
}

/**
 * @brief Get configuration value
 */
config_result_t config_get(const char* key, char* value, size_t value_size) {
    if (!config_initialized || !key || !value) {
        return CONFIG_ERROR_FLASH;
    }
    
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if (config_data.entries[i].used && 
            strcmp(config_data.entries[i].key, key) == 0) {
            strncpy(value, config_data.entries[i].value, value_size - 1);
            value[value_size - 1] = '\0';
            return CONFIG_OK;
        }
    }
    
    return CONFIG_ERROR_NOT_FOUND;
}

/**
 * @brief Delete configuration entry
 */
config_result_t config_delete(const char* key) {
    if (!config_initialized || !key) {
        return CONFIG_ERROR_FLASH;
    }
    
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if (config_data.entries[i].used && 
            strcmp(config_data.entries[i].key, key) == 0) {
            config_data.entries[i].used = false;
            memset(config_data.entries[i].key, 0, CONFIG_MAX_KEY_LEN);
            memset(config_data.entries[i].value, 0, CONFIG_MAX_VALUE_LEN);
            config_data.entry_count--;
            config_dirty = true;
            return CONFIG_OK;
        }
    }
    
    return CONFIG_ERROR_NOT_FOUND;
}

/**
 * @brief Check if key exists
 */
bool config_exists(const char* key) {
    if (!config_initialized || !key) {
        return false;
    }
    
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if (config_data.entries[i].used && 
            strcmp(config_data.entries[i].key, key) == 0) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Clear all configuration
 */
bool config_clear(void) {
    if (!config_initialized) {
        return false;
    }
    
    printf("Config: Clearing all configuration...\r\n");
    init_defaults();
    config_dirty = true;
    
    return config_save();
}

/**
 * @brief List all configuration keys
 */
int config_list_keys(const char** keys, int max_keys) {
    if (!config_initialized || !keys) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < CONFIG_MAX_ENTRIES && count < max_keys; i++) {
        if (config_data.entries[i].used) {
            keys[count++] = config_data.entries[i].key;
        }
    }
    
    return count;
}

/**
 * @brief Get entry count
 */
int config_count(void) {
    return config_initialized ? config_data.entry_count : 0;
}

/**
 * @brief Set autoboot script
 */
bool config_set_autoboot(const char* script) {
    if (!config_initialized || !script) {
        return false;
    }
    
    if (strlen(script) >= CONFIG_AUTOBOOT_SCRIPT_SIZE) {
        return false;
    }
    
    strncpy(config_data.autoboot_script, script, CONFIG_AUTOBOOT_SCRIPT_SIZE - 1);
    config_data.autoboot_script[CONFIG_AUTOBOOT_SCRIPT_SIZE - 1] = '\0';
    config_data.autoboot_enabled = true;
    config_dirty = true;
    
    return true;
}

/**
 * @brief Get autoboot script
 */
bool config_get_autoboot(char* script, size_t script_size) {
    if (!config_initialized || !script || !config_data.autoboot_enabled) {
        return false;
    }
    
    strncpy(script, config_data.autoboot_script, script_size - 1);
    script[script_size - 1] = '\0';
    
    return true;
}

/**
 * @brief Check if autoboot enabled
 */
bool config_has_autoboot(void) {
    return config_initialized && config_data.autoboot_enabled;
}

/**
 * @brief Clear autoboot script
 */
bool config_clear_autoboot(void) {
    if (!config_initialized) {
        return false;
    }
    
    memset(config_data.autoboot_script, 0, CONFIG_AUTOBOOT_SCRIPT_SIZE);
    config_data.autoboot_enabled = false;
    config_dirty = true;
    
    return true;
}

/**
 * @brief Print all configuration
 */
void config_print_all(void) {
    if (!config_initialized) {
        printf("Config: Not initialized\r\n");
        return;
    }
    
    printf("\r\n=== Configuration ===\r\n");
    printf("Entries: %d / %d\r\n", config_data.entry_count, CONFIG_MAX_ENTRIES);
    printf("Autoboot: %s\r\n\r\n", config_data.autoboot_enabled ? "enabled" : "disabled");
    
    if (config_data.entry_count == 0) {
        printf("(no entries)\r\n");
    } else {
        for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
            if (config_data.entries[i].used) {
                printf("  %s = %s\r\n", 
                       config_data.entries[i].key, 
                       config_data.entries[i].value);
            }
        }
    }
    
    printf("\r\n");
}

/**
 * @brief Get storage statistics
 */
void config_get_stats(int* used_entries, int* total_entries,
                      size_t* flash_used, size_t* flash_total) {
    if (used_entries) *used_entries = config_data.entry_count;
    if (total_entries) *total_entries = CONFIG_MAX_ENTRIES;
    if (flash_used) *flash_used = sizeof(config_storage_t);
    if (flash_total) *flash_total = FLASH_SECTOR_SIZE;
}
