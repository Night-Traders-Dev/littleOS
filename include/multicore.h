#ifndef MULTICORE_H
#define MULTICORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file multicore.h
 * @brief Multi-core management for RP2040
 * 
 * Provides high-level API for managing Core 1 execution:
 * - Launch/stop Core 1 with SageLang scripts
 * - Inter-core communication via FIFO
 * - Core status monitoring
 * - Thread-safe operations
 */

/**
 * @brief Maximum script name length for Core 1 execution
 */
#define MULTICORE_MAX_SCRIPT_NAME 32

/**
 * @brief Core 1 execution state
 */
typedef enum {
    CORE1_STATE_IDLE = 0,      ///< Core 1 is idle (not running user code)
    CORE1_STATE_RUNNING,       ///< Core 1 is executing user code
    CORE1_STATE_ERROR,         ///< Core 1 encountered an error
    CORE1_STATE_STOPPED        ///< Core 1 was stopped by user
} core1_state_t;

/**
 * @brief Initialize multi-core system
 * 
 * Must be called before any other multi-core functions.
 * This initializes the FIFO and prepares Core 1 for launching.
 */
void multicore_init(void);

/**
 * @brief Launch a SageLang script on Core 1
 * 
 * Loads and executes a stored script on Core 1. The script runs
 * independently until completion or until stopped.
 * 
 * @param script_name Name of stored script to execute
 * @return true if launch successful, false otherwise
 * 
 * @note Only one script can run on Core 1 at a time
 * @note Core 1 shares memory and peripherals with Core 0
 * @note Use mutexes or spinlocks for shared resource access
 */
bool multicore_launch_script(const char* script_name);

/**
 * @brief Launch inline SageLang code on Core 1
 * 
 * Executes SageLang code directly on Core 1 without requiring
 * a stored script.
 * 
 * @param code SageLang source code to execute
 * @return true if launch successful, false otherwise
 */
bool multicore_launch_code(const char* code);

/**
 * @brief Stop Core 1 execution
 * 
 * Gracefully stops the currently running script on Core 1.
 * Core 1 will return to idle state.
 * 
 * @return true if stop successful, false if Core 1 already idle
 */
bool multicore_stop(void);

/**
 * @brief Reset Core 1 to idle state
 * 
 * Force-resets Core 1. Use this if Core 1 is unresponsive.
 * 
 * @warning This is a hard reset - use multicore_stop() for graceful shutdown
 */
void multicore_reset_core1(void);

/**
 * @brief Get Core 1 execution state
 * 
 * @return Current state of Core 1
 */
core1_state_t multicore_get_state(void);

/**
 * @brief Check if Core 1 is running
 * 
 * @return true if Core 1 is executing user code
 */
bool multicore_is_running(void);

/**
 * @brief Send data to Core 1 via FIFO
 * 
 * Sends a 32-bit value to Core 1. Blocks if FIFO is full.
 * 
 * @param data 32-bit value to send
 * 
 * @note Core 1 must call multicore_receive() to read data
 * @note FIFO depth is 8 entries
 */
void multicore_send(uint32_t data);

/**
 * @brief Send data to Core 1 (non-blocking)
 * 
 * Attempts to send a 32-bit value to Core 1.
 * 
 * @param data 32-bit value to send
 * @return true if sent successfully, false if FIFO full
 */
bool multicore_send_nb(uint32_t data);

/**
 * @brief Receive data from Core 1 via FIFO
 * 
 * Reads a 32-bit value from Core 1. Blocks if FIFO is empty.
 * 
 * @return 32-bit value received from Core 1
 * 
 * @note Core 1 must call multicore_send() to send data
 */
uint32_t multicore_receive(void);

/**
 * @brief Receive data from Core 1 (non-blocking)
 * 
 * Attempts to read a 32-bit value from Core 1.
 * 
 * @param data Pointer to store received value
 * @return true if received successfully, false if FIFO empty
 */
bool multicore_receive_nb(uint32_t* data);

/**
 * @brief Check if FIFO has data available
 * 
 * @return Number of values available to read
 */
int multicore_fifo_available(void);

/**
 * @brief Get current core number
 * 
 * @return 0 for Core 0, 1 for Core 1
 */
uint32_t multicore_get_core_num(void);

#ifdef __cplusplus
}
#endif

#endif // MULTICORE_H
