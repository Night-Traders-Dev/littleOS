# Minimal wrapper to locate the Pico SDK via either the CMake cache variable
# or the PICO_SDK_PATH environment variable.
if(DEFINED PICO_SDK_PATH AND NOT "${PICO_SDK_PATH}" STREQUAL "")
    set(_PICO_SDK_PATH "${PICO_SDK_PATH}")
elseif(DEFINED ENV{PICO_SDK_PATH} AND NOT "$ENV{PICO_SDK_PATH}" STREQUAL "")
    set(_PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
else()
    message(FATAL_ERROR
        "PICO_SDK_PATH is not set. Pass -DPICO_SDK_PATH=/path/to/pico-sdk "
        "or export PICO_SDK_PATH in your environment.")
endif()

string(REGEX REPLACE "^~" "$ENV{HOME}" _PICO_SDK_PATH "${_PICO_SDK_PATH}")
set(PICO_SDK_PATH "${_PICO_SDK_PATH}" CACHE PATH "Path to the Pico SDK" FORCE)
if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Pico SDK import not found at ${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
