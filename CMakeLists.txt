cmake_minimum_required(VERSION 3.13)

# =============================================================================
# Board Selection
# =============================================================================
# Usage: cmake -DLITTLEOS_BOARD=<board> ..
#   pico                           - Raspberry Pi Pico (RP2040, default)
#   pico_w                         - Raspberry Pi Pico W (RP2040 + WiFi)
#   pico2                          - Raspberry Pi Pico 2 (RP2350, ARM Cortex-M33)
#   pico2_riscv                    - Raspberry Pi Pico 2 (RP2350, RISC-V Hazard3)
#   pico2_w                        - Raspberry Pi Pico 2 W (RP2350 + WiFi, ARM)
#   pico2_w_riscv                  - Raspberry Pi Pico 2 W (RP2350 + WiFi, RISC-V)
#   adafruit_feather_rp2350        - Adafruit Feather RP2350 (ARM Cortex-M33)
#   adafruit_feather_rp2350_riscv  - Adafruit Feather RP2350 (RISC-V Hazard3)

set(LITTLEOS_BOARD "pico" CACHE STRING "Target board")
option(LITTLEOS_USB_STDIO "Enable stdio over USB CDC (in addition to UART)" OFF)
set(LITTLEOS_PICO_W OFF)

if(LITTLEOS_BOARD STREQUAL "pico")
    set(PICO_BOARD pico)
    set(PICO_PLATFORM rp2040)
elseif(LITTLEOS_BOARD STREQUAL "pico_w")
    set(PICO_BOARD pico_w)
    set(PICO_PLATFORM rp2040)
    set(LITTLEOS_PICO_W ON)
elseif(LITTLEOS_BOARD STREQUAL "pico2")
    set(PICO_BOARD pico2)
    set(PICO_PLATFORM rp2350-arm-s)
elseif(LITTLEOS_BOARD STREQUAL "pico2_riscv")
    set(PICO_BOARD pico2)
    set(PICO_PLATFORM rp2350-riscv)
elseif(LITTLEOS_BOARD STREQUAL "pico2_w")
    set(PICO_BOARD pico2_w)
    set(PICO_PLATFORM rp2350-arm-s)
    set(LITTLEOS_PICO_W ON)
elseif(LITTLEOS_BOARD STREQUAL "pico2_w_riscv")
    set(PICO_BOARD pico2_w)
    set(PICO_PLATFORM rp2350-riscv)
    set(LITTLEOS_PICO_W ON)
elseif(LITTLEOS_BOARD STREQUAL "adafruit_feather_rp2350")
    set(PICO_BOARD adafruit_feather_rp2350)
    set(PICO_PLATFORM rp2350-arm-s)
elseif(LITTLEOS_BOARD STREQUAL "adafruit_feather_rp2350_riscv")
    set(PICO_BOARD adafruit_feather_rp2350)
    set(PICO_PLATFORM rp2350-riscv)
else()
    message(FATAL_ERROR "Unknown board: ${LITTLEOS_BOARD}. Valid boards: pico, pico_w, pico2, pico2_riscv, pico2_w, pico2_w_riscv, adafruit_feather_rp2350, adafruit_feather_rp2350_riscv")
endif()

# PICO_SDK_PATH must be set before configuring
include(pico_sdk_import.cmake)

project(littleOS C CXX ASM)

# Let the SDK choose the right boot stage and CPU flags for the platform.
pico_sdk_init()

# Generic compiler flags (SDK handles -mcpu and -mthumb per platform)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-free-nonheap-object")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fno-builtin -O2 -g")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -O2 -g")

message(STATUS "Using PICO_SDK_PATH=${PICO_SDK_PATH}")

# Pico W WiFi support
if(LITTLEOS_PICO_W)
    add_compile_definitions(PICO_W=1)
    message(STATUS "WiFi: Pico W support ENABLED (cyw43 + lwIP)")
else()
    message(STATUS "WiFi: Disabled")
endif()

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

# ============================================================================
# CRITICAL: Pass user/permissions configuration to C compiler via -D flags
# ============================================================================
# IMPORTANT: Convert CMake ON/OFF to C 1/0 for preprocessor
# CMake passes -DLITTLEOS_ENABLE_USER_ACCOUNT=ON as a string literal "ON"
# We need to convert it to the integer 1 for the C preprocessor

if(LITTLEOS_ENABLE_USER_ACCOUNT)
    # Enable custom user account - pass ALL config values to compiler
    # Convert ON to 1 explicitly for C preprocessor
    add_compile_definitions(
        LITTLEOS_ENABLE_USER_ACCOUNT=1
        LITTLEOS_USER_UID=${LITTLEOS_USER_UID}
        LITTLEOS_USER_NAME="${LITTLEOS_USER_NAME}"
        LITTLEOS_USER_UMASK=${LITTLEOS_USER_UMASK}
        LITTLEOS_USER_CAPABILITIES=${LITTLEOS_USER_CAPABILITIES}
    )
else()
    # Root-only configuration - explicitly set to 0
    add_compile_definitions(LITTLEOS_ENABLE_USER_ACCOUNT=0)
endif()

# Always define startup task UID
add_compile_definitions(LITTLEOS_STARTUP_TASK_UID=${LITTLEOS_STARTUP_TASK_UID})

# Define debug flag if enabled
if(LITTLEOS_DEBUG_USERS)
    add_compile_definitions(LITTLEOS_DEBUG_USERS=1)
else()
    add_compile_definitions(LITTLEOS_DEBUG_USERS=0)
endif()

# =============================================================================
# SageLang Integration
# =============================================================================

# SageLang source files from submodule
set(SAGELANG_DIR ${CMAKE_SOURCE_DIR}/third_party/sagelang)

if(EXISTS ${SAGELANG_DIR})
    message(STATUS "Found SageLang at ${SAGELANG_DIR}")
    
    # SageLang library (C sources in src/c/, self-hosting build in src/sage/)
    add_library(sagelang STATIC
        ${SAGELANG_DIR}/src/c/lexer.c
        ${SAGELANG_DIR}/src/c/parser.c
        ${SAGELANG_DIR}/src/c/ast.c
        ${SAGELANG_DIR}/src/c/interpreter.c
        ${SAGELANG_DIR}/src/c/value.c
        ${SAGELANG_DIR}/src/c/env.c
        ${SAGELANG_DIR}/src/c/gc.c
        ${SAGELANG_DIR}/src/c/module.c
        ${SAGELANG_DIR}/src/c/sage_thread.c
    )
    
    target_include_directories(sagelang PUBLIC
        ${SAGELANG_DIR}/include
    )
    
    # Define PICO_BUILD for platform detection
    target_compile_definitions(sagelang PUBLIC PICO_BUILD=1 SAGE_NO_FFI=1)
    
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
    src/shell/cmd_fs.c
    src/shell/cmd_memory.c
    src/shell/cmd_sage.c
    src/shell/cmd_script.c
    src/shell/cmd_tasks.c
 
    src/drivers/uart.c
    src/drivers/watchdog.c
    src/drivers/supervisor.c
    src/drivers/scheduler.c
    src/drivers/sensor_validation.c
    src/drivers/fs/fs_core.c
    src/drivers/fs/fs_dir.c
    src/drivers/fs/fs_file.c
    src/drivers/fs/fs_inode.c
    src/drivers/net.c
    src/drivers/ota.c
    src/drivers/remote_shell.c
    src/drivers/sensor.c
    src/drivers/mqtt.c
#
    src/storage/script_storage.c
    src/storage/config_storage.c
#
    src/kernel/ipc.c
#
    src/hal/gpio.c
    src/hal/flash.c
    src/hal/i2c.c
    src/hal/spi.c
    src/hal/pwm.c
    src/hal/adc.c
    src/hal/pio.c
    src/hal/dma.c
    src/hal/usb_device.c
    src/hal/power.c
    src/hal/rtc.c
    src/hal/timer.c
#
    src/shell/cmd_ipc.c
    src/shell/cmd_hw.c
    src/shell/cmd_net.c
    src/shell/cmd_ota.c
    src/shell/cmd_usb.c
    src/shell/cmd_pio.c
    src/shell/cmd_dma.c
    src/shell/cmd_remote.c
    src/shell/cmd_sensor.c
    src/shell/cmd_power.c
    src/shell/cmd_profile.c
    src/shell/cmd_env.c
    src/shell/cmd_text.c
    src/shell/cmd_top.c
    src/shell/cmd_pinout.c
    src/shell/cmd_proc.c
    src/shell/cmd_dev.c
    src/shell/cmd_cron.c
    src/shell/cmd_mqtt.c
    src/shell/cmd_pkg.c
    src/shell/cmd_tmux.c
    src/shell/cmd_man.c
    src/shell/cmd_logcat.c
    src/shell/cmd_trace.c
    src/shell/cmd_watchpoint.c
    src/shell/cmd_benchmark.c
    src/shell/cmd_selftest.c
    src/shell/cmd_coredump.c
    src/shell/cmd_syslog.c
    src/shell/cmd_i2cscan.c
    src/shell/cmd_wire.c
    src/shell/cmd_pwmtune.c
    src/shell/cmd_adcstream.c
    src/shell/cmd_gpiowatch.c
    src/shell/cmd_neopixel.c
    src/shell/cmd_display.c
    src/shell/cmd_rtc.c
    src/shell/cmd_timer.c
#
    src/sys/system_info.c
    src/sys/permissions.c
    src/sys/littlefetch.c
    src/sys/profiler.c
    src/sys/shell_env.c
    src/sys/procfs.c
    src/sys/devfs.c
    src/sys/cron.c
    src/sys/pkg.c
    src/sys/tmux.c
    src/sys/logcat.c
    src/sys/trace.c
    src/sys/watchpoint.c
    src/sys/coredump.c
    src/sys/syslog.c
#
    src/drivers/neopixel.c
    src/drivers/display.c
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
    hardware_i2c
    hardware_spi
    hardware_pwm
    hardware_clocks
    hardware_pio
    hardware_dma
    hardware_resets
    hardware_timer
    pico_unique_id
)

# Add Pico W networking libraries
if(LITTLEOS_PICO_W)
    target_link_libraries(littleos_core PUBLIC
        pico_cyw43_arch_lwip_threadsafe_background
    )
endif()

# RP2350-specific: HSTX DVI output
if(PICO_PLATFORM MATCHES "rp2350")
    target_sources(littleos_core PRIVATE
        src/hal/hstx_dvi.c
        src/shell/cmd_display_dvi.c
    )
    target_compile_definitions(littleos_core PUBLIC LITTLEOS_HAS_HSTX=1)
    message(STATUS "HSTX: DVI output ENABLED (RP2350)")
endif()

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
    hardware_i2c
    hardware_spi
    hardware_pwm
    hardware_adc
    pico_multicore
    pico_bootrom
    hardware_pio
    hardware_dma
    hardware_resets
)

# Stdio: UART always on, USB optional
pico_enable_stdio_uart(littleos 1)
if(LITTLEOS_USB_STDIO)
    pico_enable_stdio_usb(littleos 1)
    message(STATUS "USB stdio: ON (will appear as /dev/ttyACM* after boot)")
else()
    pico_enable_stdio_usb(littleos 0)
endif()

target_link_options(littleos PRIVATE
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/littleos.map
)

# Generate .elf, .bin, .uf2, etc.
pico_add_extra_outputs(littleos)

# Print build summary
message(STATUS "")
message(STATUS "=== Build Configuration ===")
message(STATUS "Board: ${LITTLEOS_BOARD} (${PICO_BOARD})")
message(STATUS "Platform: ${PICO_PLATFORM}")
message(STATUS "WiFi: ${LITTLEOS_PICO_W}")
message(STATUS "SageLang: ${SAGELANG_AVAILABLE}")
if(PICO_PLATFORM MATCHES "rp2350")
    message(STATUS "HSTX DVI: YES")
endif()
message(STATUS "===========================")
message(STATUS "")