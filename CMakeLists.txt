cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

# PICO_SDK_PATH must be set before configuring
include(pico_sdk_import.cmake)

project(littleOS C CXX ASM)

# Let the SDK choose the right boot2 for Pico; no override needed.
pico_sdk_init()


set(CPU_FLAGS "-mcpu=cortex-m0plus -mthumb -ffreestanding -fno-builtin")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-free-nonheap-object")


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} -O2 -g")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_FLAGS} -O2 -g")

message(STATUS "Using PICO_SDK_PATH=${PICO_SDK_PATH}")

# =============================================================================
# User / Permissions Configuration
# =============================================================================
# Build-time configuration for the permissions + user system

# Option: Enable non-root user account
option(LITTLEOS_ENABLE_USER_ACCOUNT "Enable custom user account" OFF)

# Option: User ID (UID) for custom user
set(LITTLEOS_USER_UID 1000 CACHE STRING "User ID for custom user")

# Option: Username
set(LITTLEOS_USER_NAME "user" CACHE STRING "Username for custom user")

# Option: Default umask for user
set(LITTLEOS_USER_UMASK 0022 CACHE STRING "Default umask for custom user")

# Option: Capabilities for custom user (bitmask)
set(LITTLEOS_USER_CAPABILITIES 0 CACHE STRING "Capability flags for custom user")

# Option: UID of startup task
set(LITTLEOS_STARTUP_TASK_UID 0 CACHE STRING "UID to run startup task as (0=root)")

# Option: Enable debug output for user database
option(LITTLEOS_DEBUG_USERS "Enable user database debug output" OFF)

message(STATUS "")
message(STATUS "=== littleOS User Configuration ===")
if(LITTLEOS_ENABLE_USER_ACCOUNT)
    message(STATUS "  ✓ Custom user account ENABLED")
    message(STATUS "    - Username: ${LITTLEOS_USER_NAME}")
    message(STATUS "    - UID: ${LITTLEOS_USER_UID}")
    message(STATUS "    - Umask: 0${LITTLEOS_USER_UMASK}")
    message(STATUS "    - Capabilities: ${LITTLEOS_USER_CAPABILITIES}")
else()
    message(STATUS "  ✗ Custom user account DISABLED (root only)")
endif()
message(STATUS "  - Startup task UID: ${LITTLEOS_STARTUP_TASK_UID}")
if(LITTLEOS_DEBUG_USERS)
    message(STATUS "  ✓ User database debug output ENABLED")
endif()
message(STATUS "")

# =============================================================================
# SageLang Integration
# =============================================================================

# SageLang source files from submodule
set(SAGELANG_DIR ${CMAKE_SOURCE_DIR}/third_party/sagelang)

if(EXISTS ${SAGELANG_DIR})
    message(STATUS "Found SageLang at ${SAGELANG_DIR}")
    
    # SageLang library
    add_library(sagelang STATIC
        ${SAGELANG_DIR}/src/lexer.c
        ${SAGELANG_DIR}/src/parser.c
        ${SAGELANG_DIR}/src/ast.c
        ${SAGELANG_DIR}/src/interpreter.c
        ${SAGELANG_DIR}/src/value.c
        ${SAGELANG_DIR}/src/env.c
        ${SAGELANG_DIR}/src/gc.c
        ${SAGELANG_DIR}/src/module.c
    )
    
    target_include_directories(sagelang PUBLIC
        ${SAGELANG_DIR}/include
    )
    
    # Define PICO_BUILD for platform detection
    target_compile_definitions(sagelang PUBLIC PICO_BUILD=1)
    
    # Link against pico_stdlib for embedded functions
    target_link_libraries(sagelang PUBLIC pico_stdlib)
    
    set(SAGELANG_AVAILABLE TRUE)
else()
    message(WARNING "SageLang not found at ${SAGELANG_DIR}")
    message(WARNING "Run: git submodule add https://github.com/Night-Traders-Dev/SageLang.git third_party/sagelang")
    set(SAGELANG_AVAILABLE FALSE)
endif()

# =============================================================================
# littleOS Core Library
# =============================================================================

add_library(littleos_core STATIC
    src/kernel/kernel.c
    src/kernel/dmesg.c
    src/kernel/memory_segmented.c
#
    src/shell/shell.c
    src/shell/cmd_supervisor.c
    src/shell/cmd_dmesg.c
    src/shell/cmd_perms.c
    src/shell/cmd_users.c
 
    src/drivers/uart.c
    src/drivers/watchdog.c
    src/drivers/supervisor.c
    src/drivers/scheduler.c
    src/drivers/sensor_validation.c
#
    src/storage/script_storage.c
    src/storage/config_storage.c
#
    src/hal/gpio.c
#
    src/sys/system_info.c
    src/sys/permissions.c
    src/sys/littlefetch.c
#
    src/usr/users_config.c
)

target_include_directories(littleos_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link against pico_stdlib so you inherit SDK runtime and headers.
target_link_libraries(littleos_core PUBLIC 
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_flash
    hardware_sync
    hardware_watchdog
    pico_unique_id
)

# Add SageLang embedding if available
if(SAGELANG_AVAILABLE)
    target_sources(littleos_core PRIVATE
        src/sage/sage_embed.c
        src/sage/sage_gpio.c
        src/sage/sage_system.c
        src/sage/sage_time.c
        src/sage/sage_config.c
        src/sage/sage_watchdog.c
#
        src/shell/cmd_sage.c
        src/shell/cmd_script.c
        src/shell/cmd_tasks.c
        src/shell/cmd_memory.c
    )
    
    # CRITICAL FIX: Add SageLang include directory to littleos_core
    target_include_directories(littleos_core PRIVATE
        ${SAGELANG_DIR}/include
    )
    
    target_link_libraries(littleos_core PUBLIC sagelang)
    target_compile_definitions(littleos_core PUBLIC SAGE_ENABLED=1)
    
    message(STATUS "SageLang integration enabled with GPIO, System, Time, Config, and Watchdog support")
else()
    message(STATUS "Building without SageLang support")
endif()

# Propagate user/permissions config to core (so all OS code sees it)
target_compile_definitions(littleos_core PUBLIC
    LITTLEOS_ENABLE_USER_ACCOUNT=${LITTLEOS_ENABLE_USER_ACCOUNT}
    LITTLEOS_USER_UID=${LITTLEOS_USER_UID}
    LITTLEOS_USER_UMASK=${LITTLEOS_USER_UMASK}
    LITTLEOS_USER_CAPABILITIES=${LITTLEOS_USER_CAPABILITIES}
    LITTLEOS_STARTUP_TASK_UID=${LITTLEOS_STARTUP_TASK_UID}
    LITTLEOS_USER_NAME="${LITTLEOS_USER_NAME}"
)

if(LITTLEOS_DEBUG_USERS)
    target_compile_definitions(littleos_core PUBLIC LITTLEOS_DEBUG_USERS=1)
endif()

# =============================================================================
# littleOS Executable
# =============================================================================

# Final ELF image - this contains your main() function
add_executable(littleos
    boot/boot.c
)

# Link your final executable against the core OS library and hardware libraries
target_link_libraries(littleos PRIVATE
    littleos_core
    hardware_uart
    hardware_gpio
    hardware_timer
    hardware_clocks
    hardware_watchdog
    hardware_flash
    hardware_sync
    pico_multicore
    pico_bootrom
)

# Enable stdio over both UART and USB
pico_enable_stdio_uart(littleos 1)
pico_enable_stdio_usb(littleos 1)

target_link_options(littleos PRIVATE
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/littleos.map
)

# Generate .elf, .bin, .uf2, etc.
pico_add_extra_outputs(littleos)

# Print build summary
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "Board: ${PICO_BOARD}")
message(STATUS "Platform: ${PICO_PLATFORM}")
message(STATUS "SageLang: ${SAGELANG_AVAILABLE}")
message(STATUS "Supervisor: Core 1 health monitoring")
message(STATUS "Kernel Messages: dmesg ring buffer")
message(STATUS "===========================" )
message(STATUS "")
