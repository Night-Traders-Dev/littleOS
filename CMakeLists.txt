cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

# PICO_SDK_PATH must be set before configuring
include(pico_sdk_import.cmake)

project(littleOS C CXX ASM)

# Let the SDK choose the right boot2 for Pico; no override needed.
pico_sdk_init()

set(CPU_FLAGS "-mcpu=cortex-m0plus -mthumb -ffreestanding -fno-builtin")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} -O2 -g")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_FLAGS} -O2 -g")

message(STATUS "Using PICO_SDK_PATH=${PICO_SDK_PATH}")

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
    src/kernel.c
    src/shell/shell.c
    src/shell/cmd_supervisor.c
    src/shell/cmd_dmesg.c
    src/uart.c
    src/script_storage.c
    src/hal/gpio.c
    src/system_info.c
    src/config_storage.c
    src/watchdog.c
    src/supervisor.c
    src/dmesg.c
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
        src/sage_embed.c
        src/sage_gpio.c
        src/sage_system.c
        src/sage_time.c
        src/sage_config.c
        src/sage_watchdog.c
        src/shell/cmd_sage.c
        src/shell/cmd_script.c
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
