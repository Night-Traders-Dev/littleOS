// src/sage_config.c
// SageLang Native Function Bindings for Configuration Storage
#include "sage_embed.h"
#include "config_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include SageLang headers
#include "value.h"
#include "env.h"

/**
 * @brief SageLang native function: config_set(key, value)
 * Set a configuration value
 * 
 * Usage in SageLang:
 *   config_set("timezone", "America/New_York");
 *   config_set("led_brightness", "75");
 */
static Value sage_config_set(int argc, Value* args) {
    if (argc != 2) {
        fprintf(stderr, "config_set() requires 2 arguments: key, value\r\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        fprintf(stderr, "config_set() arguments must be strings\r\n");
        return val_bool(false);
    }
    
    const char* key = args[0].as.string;
    const char* value = args[1].as.string;
    
    config_result_t result = config_set(key, value);
    
    if (result != CONFIG_OK) {
        fprintf(stderr, "config_set() error: %d\r\n", result);
        return val_bool(false);
    }
    
    // Auto-save after setting
    config_save();
    
    return val_bool(true);
}

/**
 * @brief SageLang native function: config_get(key)
 * Get a configuration value
 * 
 * Usage in SageLang:
 *   let tz = config_get("timezone");
 *   let brightness = config_get("led_brightness");
 */
static Value sage_config_get(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "config_get() requires 1 argument: key\r\n");
        return val_nil();
    }
    
    if (args[0].type != VAL_STRING) {
        fprintf(stderr, "config_get() argument must be a string\r\n");
        return val_nil();
    }
    
    const char* key = args[0].as.string;
    char value[CONFIG_MAX_VALUE_LEN];
    
    config_result_t result = config_get(key, value, sizeof(value));
    
    if (result != CONFIG_OK) {
        return val_nil();
    }
    
    char* str = malloc(strlen(value) + 1);
    strcpy(str, value);
    return val_string(str);
}

/**
 * @brief SageLang native function: config_delete(key)
 * Delete a configuration entry
 * 
 * Usage in SageLang:
 *   config_delete("old_setting");
 */
static Value sage_config_delete(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "config_delete() requires 1 argument: key\r\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_STRING) {
        fprintf(stderr, "config_delete() argument must be a string\r\n");
        return val_bool(false);
    }
    
    const char* key = args[0].as.string;
    config_result_t result = config_delete(key);
    
    if (result == CONFIG_OK) {
        config_save();
        return val_bool(true);
    }
    
    return val_bool(false);
}

/**
 * @brief SageLang native function: config_exists(key)
 * Check if configuration key exists
 * 
 * Usage in SageLang:
 *   if (config_exists("timezone")) {
 *       print("Timezone is configured");
 *   }
 */
static Value sage_config_exists(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "config_exists() requires 1 argument: key\r\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_STRING) {
        fprintf(stderr, "config_exists() argument must be a string\r\n");
        return val_bool(false);
    }
    
    const char* key = args[0].as.string;
    return val_bool(config_exists(key));
}

/**
 * @brief SageLang native function: config_list()
 * Get list of all configuration keys
 * 
 * Usage in SageLang:
 *   let keys = config_list();
 *   for (key in keys) {
 *       print(key);
 *   }
 */
static Value sage_config_list(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    const char* keys[CONFIG_MAX_ENTRIES];
    int count = config_list_keys(keys, CONFIG_MAX_ENTRIES);
    
    Value array = val_array();
    
    for (int i = 0; i < count; i++) {
        char* key_copy = malloc(strlen(keys[i]) + 1);
        strcpy(key_copy, keys[i]);
        array_push(&array, val_string(key_copy));
    }
    
    return array;
}

/**
 * @brief SageLang native function: config_count()
 * Get number of configuration entries
 * 
 * Usage in SageLang:
 *   let num_settings = config_count();
 */
static Value sage_config_count(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    return val_number(config_count());
}

/**
 * @brief SageLang native function: config_clear()
 * Clear all configuration (factory reset)
 * 
 * Usage in SageLang:
 *   config_clear();
 */
static Value sage_config_clear(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    return val_bool(config_clear());
}

/**
 * @brief SageLang native function: config_print()
 * Print all configuration to console
 * 
 * Usage in SageLang:
 *   config_print();
 */
static Value sage_config_print(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    config_print_all();
    return val_nil();
}

/**
 * @brief SageLang native function: config_set_autoboot(script)
 * Set script to run on boot
 * 
 * Usage in SageLang:
 *   config_set_autoboot("print('System started'); gpio_init(25, true);");
 */
static Value sage_config_set_autoboot(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "config_set_autoboot() requires 1 argument: script\r\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_STRING) {
        fprintf(stderr, "config_set_autoboot() argument must be a string\r\n");
        return val_bool(false);
    }
    
    const char* script = args[0].as.string;
    
    if (config_set_autoboot(script)) {
        config_save();
        return val_bool(true);
    }
    
    return val_bool(false);
}

/**
 * @brief SageLang native function: config_clear_autoboot()
 * Clear autoboot script
 * 
 * Usage in SageLang:
 *   config_clear_autoboot();
 */
static Value sage_config_clear_autoboot(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    if (config_clear_autoboot()) {
        config_save();
        return val_bool(true);
    }
    
    return val_bool(false);
}

/**
 * @brief SageLang native function: config_has_autoboot()
 * Check if autoboot script is set
 * 
 * Usage in SageLang:
 *   if (config_has_autoboot()) {
 *       print("Autoboot enabled");
 *   }
 */
static Value sage_config_has_autoboot(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    return val_bool(config_has_autoboot());
}

/**
 * @brief Register all configuration native functions with SageLang environment
 */
void sage_register_config_functions(Env* env) {
    if (!env) return;
    
    // Register configuration functions
    env_define(env, "config_set", 10, val_native(sage_config_set));
    env_define(env, "config_get", 10, val_native(sage_config_get));
    env_define(env, "config_delete", 13, val_native(sage_config_delete));
    env_define(env, "config_exists", 13, val_native(sage_config_exists));
    env_define(env, "config_list", 11, val_native(sage_config_list));
    env_define(env, "config_count", 12, val_native(sage_config_count));
    env_define(env, "config_clear", 12, val_native(sage_config_clear));
    env_define(env, "config_print", 12, val_native(sage_config_print));
    env_define(env, "config_set_autoboot", 19, val_native(sage_config_set_autoboot));
    env_define(env, "config_clear_autoboot", 21, val_native(sage_config_clear_autoboot));
    env_define(env, "config_has_autoboot", 19, val_native(sage_config_has_autoboot));
    
    printf("Config: Registered 11 native functions\r\n");
}
