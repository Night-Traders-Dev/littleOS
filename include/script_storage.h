#ifndef SCRIPT_STORAGE_H
#define SCRIPT_STORAGE_H

#include <stddef.h>
#include <stdbool.h>

// Maximum script name length
#define SCRIPT_NAME_MAX 32

// Script storage structure
typedef struct Script {
    char name[SCRIPT_NAME_MAX];
    char* code;
    size_t code_size;
    struct Script* next;
} Script;

// Initialize script storage system
void script_storage_init(void);

// Save a script (creates new or updates existing)
bool script_save(const char* name, const char* code);

// Load a script by name
const char* script_load(const char* name);

// Delete a script by name
bool script_delete(const char* name);

// List all scripts (calls callback for each script)
void script_list(void (*callback)(const char* name, size_t size));

// Get script count
int script_count(void);

// Get total memory used by scripts
size_t script_memory_used(void);

// Clear all scripts
void script_clear_all(void);

// Check if script exists
bool script_exists(const char* name);

#endif // SCRIPT_STORAGE_H
