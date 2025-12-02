#include "script_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Head of the script linked list
static Script* scripts_head = NULL;

/**
 * @brief Initialize script storage system
 */
void script_storage_init(void) {
    scripts_head = NULL;
}

/**
 * @brief Find a script by name
 * @return Pointer to script or NULL if not found
 */
static Script* find_script(const char* name) {
    Script* current = scripts_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Save a script (creates new or updates existing)
 * @param name Script name (max SCRIPT_NAME_MAX-1 chars)
 * @param code Script source code
 * @return true on success, false on failure
 */
bool script_save(const char* name, const char* code) {
    if (!name || !code) {
        return false;
    }
    
    // Validate name length
    if (strlen(name) >= SCRIPT_NAME_MAX) {
        return false;
    }
    
    // Check if script already exists
    Script* existing = find_script(name);
    if (existing != NULL) {
        // Update existing script
        char* new_code = (char*)malloc(strlen(code) + 1);
        if (!new_code) {
            return false;
        }
        
        // Free old code and update
        free(existing->code);
        strcpy(new_code, code);
        existing->code = new_code;
        existing->code_size = strlen(code);
        return true;
    }
    
    // Create new script
    Script* new_script = (Script*)malloc(sizeof(Script));
    if (!new_script) {
        return false;
    }
    
    new_script->code = (char*)malloc(strlen(code) + 1);
    if (!new_script->code) {
        free(new_script);
        return false;
    }
    
    // Initialize script
    strncpy(new_script->name, name, SCRIPT_NAME_MAX - 1);
    new_script->name[SCRIPT_NAME_MAX - 1] = '\0';
    strcpy(new_script->code, code);
    new_script->code_size = strlen(code);
    new_script->next = scripts_head;
    
    // Add to head of list
    scripts_head = new_script;
    
    return true;
}

/**
 * @brief Load a script by name
 * @param name Script name
 * @return Pointer to script code or NULL if not found
 */
const char* script_load(const char* name) {
    Script* script = find_script(name);
    return script ? script->code : NULL;
}

/**
 * @brief Delete a script by name
 * @param name Script name
 * @return true if deleted, false if not found
 */
bool script_delete(const char* name) {
    if (!name) {
        return false;
    }
    
    Script* current = scripts_head;
    Script* prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // Found it - remove from list
            if (prev == NULL) {
                // Removing head
                scripts_head = current->next;
            } else {
                prev->next = current->next;
            }
            
            // Free memory
            free(current->code);
            free(current);
            return true;
        }
        
        prev = current;
        current = current->next;
    }
    
    return false;
}

/**
 * @brief List all scripts (calls callback for each script)
 * @param callback Function to call for each script
 */
void script_list(void (*callback)(const char* name, size_t size)) {
    if (!callback) {
        return;
    }
    
    Script* current = scripts_head;
    while (current != NULL) {
        callback(current->name, current->code_size);
        current = current->next;
    }
}

/**
 * @brief Get script count
 * @return Number of stored scripts
 */
int script_count(void) {
    int count = 0;
    Script* current = scripts_head;
    
    while (current != NULL) {
        count++;
        current = current->next;
    }
    
    return count;
}

/**
 * @brief Get total memory used by scripts
 * @return Total bytes used (includes overhead)
 */
size_t script_memory_used(void) {
    size_t total = 0;
    Script* current = scripts_head;
    
    while (current != NULL) {
        // Script struct + code string
        total += sizeof(Script) + current->code_size + 1;
        current = current->next;
    }
    
    return total;
}

/**
 * @brief Clear all scripts
 */
void script_clear_all(void) {
    Script* current = scripts_head;
    
    while (current != NULL) {
        Script* next = current->next;
        free(current->code);
        free(current);
        current = next;
    }
    
    scripts_head = NULL;
}

/**
 * @brief Check if script exists
 * @param name Script name
 * @return true if exists, false otherwise
 */
bool script_exists(const char* name) {
    return find_script(name) != NULL;
}
