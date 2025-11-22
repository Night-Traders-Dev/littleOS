cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

# PICO_SDK_PATH must be set before configuring
include(pico_sdk_import.cmake)

project(littleOS C CXX ASM)

# Let the SDK choose the right boot2 for Pico; no override needed. [web:169][web:171]
pico_sdk_init()

set(CPU_FLAGS "-mcpu=cortex-m0plus -mthumb -ffreestanding -fno-builtin")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} -O2 -g")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_FLAGS} -O2 -g")

message(STATUS "Using PICO_SDK_PATH=${PICO_SDK_PATH}")

# Core library with your OS code
# NOTE: do NOT add boot/boot.c here; SDK crt0 provides reset/startup. [web:107][web:151]
add_library(littleos_core STATIC
    boot/boot.c
    src/kernel.c
    src/shell/shell.c
)

target_include_directories(littleos_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link against pico_stdlib so you inherit SDK runtime and headers. [web:151][web:160]
target_link_libraries(littleos_core PUBLIC pico_stdlib)

# Final ELF image – no duplicate sources, just link the core library
add_executable(littleos)

target_link_libraries(littleos PRIVATE
    littleos_core
    hardware_uart
    hardware_gpio
    hardware_timer
)

pico_enable_stdio_uart(littleos 1)
pico_enable_stdio_usb(littleos 1)

target_link_options(littleos PRIVATE
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/littleos.map
)

# Generate .elf, .bin, .uf2, etc. [web:151][web:157]
pico_add_extra_outputs(littleos)
