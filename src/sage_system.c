// src/sage_system.c
// SageLang Native Function Bindings for System Information
#include "sage_embed.h"
#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>

// Include SageLang headers
#include "value.h"
#include "env.h"

/**
 * @brief SageLang native function: sys_version()
 * Get littleOS version string
 * 
 * Usage in SageLang:
 *   let version = sys_version();
 *   print(version);  // "0.2.0"
 */
static Value sage_sys_version(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    const char* version = system_get_version();
    char* str = malloc(strlen(version) + 1);
    strcpy(str, version);
    return val_string(str);
}

/**
 * @brief SageLang native function: sys_uptime()
 * Get system uptime in seconds
 * 
 * Usage in SageLang:
 *   let seconds = sys_uptime();
 */
static Value sage_sys_uptime(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    uptime_info_t info;
    if (system_get_uptime(&info)) {
        return val_number(info.uptime_seconds);
    }
    return val_number(0);
}

/**
 * @brief SageLang native function: sys_temp()
 * Get system temperature in Celsius
 * 
 * Usage in SageLang:
 *   let temp = sys_temp();
 *   print("Temperature: " + str(temp) + "C");
 */
static Value sage_sys_temp(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    float temp = system_get_temperature();
    return val_number((double)temp);
}

/**
 * @brief SageLang native function: sys_clock()
 * Get CPU clock speed in MHz
 * 
 * Usage in SageLang:
 *   let mhz = sys_clock();
 */
static Value sage_sys_clock(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    cpu_info_t info;
    if (system_get_cpu_info(&info)) {
        return val_number(info.clock_speed_hz / 1000000.0);
    }
    return val_number(0);
}

/**
 * @brief SageLang native function: sys_free_ram()
 * Get free RAM in kilobytes
 * 
 * Usage in SageLang:
 *   let free_kb = sys_free_ram();
 */
static Value sage_sys_free_ram(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    memory_info_t info;
    if (system_get_memory_info(&info)) {
        return val_number(info.free_ram / 1024.0);
    }
    return val_number(0);
}

/**
 * @brief SageLang native function: sys_total_ram()
 * Get total RAM in kilobytes
 * 
 * Usage in SageLang:
 *   let total_kb = sys_total_ram();
 */
static Value sage_sys_total_ram(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    memory_info_t info;
    if (system_get_memory_info(&info)) {
        return val_number(info.total_ram / 1024.0);
    }
    return val_number(0);
}

/**
 * @brief SageLang native function: sys_board_id()
 * Get unique board ID as hex string
 * 
 * Usage in SageLang:
 *   let id = sys_board_id();
 */
static Value sage_sys_board_id(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    char board_id[17];
    if (system_get_board_id(board_id, sizeof(board_id))) {
        char* str = malloc(strlen(board_id) + 1);
        strcpy(str, board_id);
        return val_string(str);
    }
    return val_string("UNKNOWN");
}

/**
 * @brief SageLang native function: sys_info()
 * Get system information as dictionary
 * 
 * Usage in SageLang:
 *   let info = sys_info();
 *   print(info["version"]);
 *   print(info["uptime"]);
 *   print(info["temperature"]);
 */
static Value sage_sys_info(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    Value dict = val_dict();
    
    // Version
    const char* version = system_get_version();
    char* ver_str = malloc(strlen(version) + 1);
    strcpy(ver_str, version);
    dict_set(&dict, "version", val_string(ver_str));
    
    // Build date
    const char* build_date = system_get_build_date();
    char* date_str = malloc(strlen(build_date) + 1);
    strcpy(date_str, build_date);
    dict_set(&dict, "build_date", val_string(date_str));
    
    // CPU info
    cpu_info_t cpu;
    if (system_get_cpu_info(&cpu)) {
        dict_set(&dict, "cpu_model", val_string("RP2040"));
        dict_set(&dict, "cpu_mhz", val_number(cpu.clock_speed_hz / 1000000.0));
        dict_set(&dict, "cpu_cores", val_number(cpu.core_count));
        dict_set(&dict, "cpu_revision", val_number(cpu.chip_revision));
    }
    
    // Memory info
    memory_info_t mem;
    if (system_get_memory_info(&mem)) {
        dict_set(&dict, "total_ram_kb", val_number(mem.total_ram / 1024.0));
        dict_set(&dict, "free_ram_kb", val_number(mem.free_ram / 1024.0));
        dict_set(&dict, "used_ram_kb", val_number(mem.used_ram / 1024.0));
        dict_set(&dict, "flash_mb", val_number(mem.flash_size / (1024.0 * 1024.0)));
    }
    
    // Uptime
    uptime_info_t uptime;
    if (system_get_uptime(&uptime)) {
        dict_set(&dict, "uptime_seconds", val_number(uptime.uptime_seconds));
        dict_set(&dict, "uptime_minutes", val_number(uptime.uptime_minutes));
        dict_set(&dict, "uptime_hours", val_number(uptime.uptime_hours));
        dict_set(&dict, "uptime_days", val_number(uptime.uptime_days));
    }
    
    // Temperature
    float temp = system_get_temperature();
    dict_set(&dict, "temperature", val_number((double)temp));
    
    // Board ID
    char board_id[17];
    if (system_get_board_id(board_id, sizeof(board_id))) {
        char* id_str = malloc(strlen(board_id) + 1);
        strcpy(id_str, board_id);
        dict_set(&dict, "board_id", val_string(id_str));
    }
    
    return dict;
}

/**
 * @brief SageLang native function: sys_print()
 * Print formatted system information
 * 
 * Usage in SageLang:
 *   sys_print();
 */
static Value sage_sys_print(int argc, Value* args) {
    (void)argc;
    (void)args;
    
    system_print_info();
    return val_nil();
}

/**
 * @brief Register all system information native functions with SageLang environment
 */
void sage_register_system_functions(Env* env) {
    if (!env) return;
    
    // Register system information functions
    env_define(env, "sys_version", 11, val_native(sage_sys_version));
    env_define(env, "sys_uptime", 10, val_native(sage_sys_uptime));
    env_define(env, "sys_temp", 8, val_native(sage_sys_temp));
    env_define(env, "sys_clock", 9, val_native(sage_sys_clock));
    env_define(env, "sys_free_ram", 12, val_native(sage_sys_free_ram));
    env_define(env, "sys_total_ram", 13, val_native(sage_sys_total_ram));
    env_define(env, "sys_board_id", 12, val_native(sage_sys_board_id));
    env_define(env, "sys_info", 8, val_native(sage_sys_info));
    env_define(env, "sys_print", 9, val_native(sage_sys_print));
    
    printf("System: Registered 9 native functions\r\n");
}
