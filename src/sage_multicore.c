#include "sage_embed.h"
#include "multicore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SAGE_ENABLED

#include "value.h"
#include "env.h"

/**
 * @brief core1_launch_script(name) - Launch script on Core 1
 * 
 * SageLang: core1_launch_script("blink")
 * Returns: true on success, false on failure
 */
static Value sage_core1_launch_script(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "Error: core1_launch_script() takes 1 argument\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: core1_launch_script() argument must be string\n");
        return val_bool(false);
    }
    
    bool result = multicore_launch_script(args[0].as.string);
    return val_bool(result);
}

/**
 * @brief core1_launch_code(code) - Launch inline code on Core 1
 * 
 * SageLang: core1_launch_code("gpio_toggle(25)")
 * Returns: true on success, false on failure
 */
static Value sage_core1_launch_code(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "Error: core1_launch_code() takes 1 argument\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: core1_launch_code() argument must be string\n");
        return val_bool(false);
    }
    
    bool result = multicore_launch_code(args[0].as.string);
    return val_bool(result);
}

/**
 * @brief core1_stop() - Stop Core 1 execution
 * 
 * SageLang: core1_stop()
 * Returns: true if stopped, false if already idle
 */
static Value sage_core1_stop(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core1_stop() takes no arguments\n");
        return val_bool(false);
    }
    
    bool result = multicore_stop();
    return val_bool(result);
}

/**
 * @brief core1_is_running() - Check if Core 1 is running
 * 
 * SageLang: core1_is_running()
 * Returns: true if running, false otherwise
 */
static Value sage_core1_is_running(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core1_is_running() takes no arguments\n");
        return val_bool(false);
    }
    
    bool result = multicore_is_running();
    return val_bool(result);
}

/**
 * @brief core1_get_state() - Get Core 1 state
 * 
 * SageLang: core1_get_state()
 * Returns: State as integer (0=idle, 1=running, 2=error, 3=stopped)
 */
static Value sage_core1_get_state(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core1_get_state() takes no arguments\n");
        return val_number(0);
    }
    
    int state = (int)multicore_get_state();
    return val_number((double)state);
}

/**
 * @brief core_send(data) - Send 32-bit value to other core via FIFO
 * 
 * SageLang: core_send(42)
 * Returns: null
 * 
 * Note: Blocks if FIFO is full (8 entries deep)
 */
static Value sage_core_send(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "Error: core_send() takes 1 argument\n");
        return val_nil();
    }
    
    if (args[0].type != VAL_NUMBER) {
        fprintf(stderr, "Error: core_send() argument must be number\n");
        return val_nil();
    }
    
    uint32_t data = (uint32_t)args[0].as.number;
    multicore_send(data);
    
    return val_nil();
}

/**
 * @brief core_send_nb(data) - Send value to other core (non-blocking)
 * 
 * SageLang: core_send_nb(42)
 * Returns: true if sent, false if FIFO full
 */
static Value sage_core_send_nb(int argc, Value* args) {
    if (argc != 1) {
        fprintf(stderr, "Error: core_send_nb() takes 1 argument\n");
        return val_bool(false);
    }
    
    if (args[0].type != VAL_NUMBER) {
        fprintf(stderr, "Error: core_send_nb() argument must be number\n");
        return val_bool(false);
    }
    
    uint32_t data = (uint32_t)args[0].as.number;
    bool result = multicore_send_nb(data);
    
    return val_bool(result);
}

/**
 * @brief core_receive() - Receive 32-bit value from other core
 * 
 * SageLang: let data = core_receive()
 * Returns: 32-bit number from other core
 * 
 * Note: Blocks if FIFO is empty
 */
static Value sage_core_receive(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core_receive() takes no arguments\n");
        return val_number(0);
    }
    
    uint32_t data = multicore_receive();
    return val_number((double)data);
}

/**
 * @brief core_receive_nb() - Receive value from other core (non-blocking)
 * 
 * SageLang: let data = core_receive_nb()
 * Returns: Number if available, null if FIFO empty
 */
static Value sage_core_receive_nb(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core_receive_nb() takes no arguments\n");
        return val_nil();
    }
    
    uint32_t data;
    bool received = multicore_receive_nb(&data);
    
    if (received) {
        return val_number((double)data);
    } else {
        return val_nil();
    }
}

/**
 * @brief core_fifo_available() - Check if FIFO has data
 * 
 * SageLang: core_fifo_available()
 * Returns: Number of values available (0 or 1)
 */
static Value sage_core_fifo_available(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core_fifo_available() takes no arguments\n");
        return val_number(0);
    }
    
    int available = multicore_fifo_available();
    return val_number((double)available);
}

/**
 * @brief core_num() - Get current core number
 * 
 * SageLang: let core = core_num()
 * Returns: 0 for Core 0, 1 for Core 1
 */
static Value sage_core_num(int argc, Value* args) {
    (void)args;
    
    if (argc != 0) {
        fprintf(stderr, "Error: core_num() takes no arguments\n");
        return val_number(0);
    }
    
    uint32_t core_num = multicore_get_core_num();
    return val_number((double)core_num);
}

/**
 * @brief Register all multi-core functions with SageLang environment
 */
void sage_register_multicore_functions(Env* env) {
    if (!env) return;
    
    // Core 1 control - use val_native() constructor
    env_define(env, "core1_launch_script", 21, val_native(sage_core1_launch_script));
    env_define(env, "core1_launch_code", 18, val_native(sage_core1_launch_code));
    env_define(env, "core1_stop", 10, val_native(sage_core1_stop));
    env_define(env, "core1_is_running", 16, val_native(sage_core1_is_running));
    env_define(env, "core1_get_state", 15, val_native(sage_core1_get_state));
    
    // Inter-core communication
    env_define(env, "core_send", 9, val_native(sage_core_send));
    env_define(env, "core_send_nb", 12, val_native(sage_core_send_nb));
    env_define(env, "core_receive", 12, val_native(sage_core_receive));
    env_define(env, "core_receive_nb", 15, val_native(sage_core_receive_nb));
    env_define(env, "core_fifo_available", 19, val_native(sage_core_fifo_available));
    env_define(env, "core_num", 8, val_native(sage_core_num));
    
    printf("Multi-core: Registered 11 native functions\r\n");
}

#endif // SAGE_ENABLED
