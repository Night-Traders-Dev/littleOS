#!/usr/bin/env bash

set -e

echo "========================================="
echo " littleOS Build Helper"
echo "========================================="
echo

# Defaults
DEFAULT_ENABLE_USER="y"
DEFAULT_USERNAME="littleOS"
DEFAULT_UID="1000"
DEFAULT_CAPS="0"
export PICO_SDK_PATH=${PICO_SDK_PATH:-$HOME/pico-sdk}

# Board definitions
BOARDS_RP2040=("pico" "pico_w")
BOARDS_RP2350=("pico2" "pico2_riscv" "pico2_w" "pico2_w_riscv" "adafruit_feather_rp2350" "adafruit_feather_rp2350_riscv")
ALL_BOARDS=("${BOARDS_RP2040[@]}" "${BOARDS_RP2350[@]}")

# =========================================================================
# Batch build flags
# =========================================================================
if [[ "$1" == "--rp2040-all" ]]; then
    echo "Batch build: All RP2040 boards"
    echo
    for board in "${BOARDS_RP2040[@]}"; do
        echo "--- Building: $board ---"
        build_dir="build_${board}"
        rm -rf "$build_dir"
        mkdir -p "$build_dir"
        (cd "$build_dir" && cmake -DLITTLEOS_BOARD="$board" .. && make -j"$(nproc)")
        if [[ -f "$build_dir/littleos.uf2" ]]; then
            cp "$build_dir/littleos.uf2" "littleos_${board}.uf2"
            echo "  -> littleos_${board}.uf2"
        fi
        echo
    done
    echo "RP2040 batch build complete."
    exit 0
fi

if [[ "$1" == "--rp2350-all" ]]; then
    echo "Batch build: All RP2350 boards"
    echo
    for board in "${BOARDS_RP2350[@]}"; do
        echo "--- Building: $board ---"
        build_dir="build_${board}"
        rm -rf "$build_dir"
        mkdir -p "$build_dir"
        (cd "$build_dir" && cmake -DLITTLEOS_BOARD="$board" .. && make -j"$(nproc)")
        if [[ -f "$build_dir/littleos.uf2" ]]; then
            cp "$build_dir/littleos.uf2" "littleos_${board}.uf2"
            echo "  -> littleos_${board}.uf2"
        fi
        echo
    done
    echo "RP2350 batch build complete."
    exit 0
fi

if [[ "$1" == "--all" ]]; then
    echo "Batch build: All boards"
    echo
    for board in "${ALL_BOARDS[@]}"; do
        echo "--- Building: $board ---"
        build_dir="build_${board}"
        rm -rf "$build_dir"
        mkdir -p "$build_dir"
        (cd "$build_dir" && cmake -DLITTLEOS_BOARD="$board" .. && make -j"$(nproc)")
        if [[ -f "$build_dir/littleos.uf2" ]]; then
            cp "$build_dir/littleos.uf2" "littleos_${board}.uf2"
            echo "  -> littleos_${board}.uf2"
        fi
        echo
    done
    echo "All boards built."
    exit 0
fi

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo "Usage: $0 [option]"
    echo
    echo "Options:"
    echo "  (none)          Interactive build with board selection"
    echo "  --rp2040-all    Build all RP2040 boards (Pico, Pico W)"
    echo "  --rp2350-all    Build all RP2350 boards (Feather ARM, Feather RISC-V)"
    echo "  --all           Build all boards"
    echo "  --help          Show this help"
    echo
    echo "Boards:"
    for b in "${ALL_BOARDS[@]}"; do
        echo "  $b"
    done
    exit 0
fi

# =========================================================================
# Interactive build
# =========================================================================

make clean 2>/dev/null || true
git pull origin main
git submodule update --init --recursive --remote

# Board selection
echo "Select target board:"
echo "  1) Raspberry Pi Pico (default)"
echo "  2) Raspberry Pi Pico W"
echo "  3) Raspberry Pi Pico 2 (ARM Cortex-M33)"
echo "  4) Raspberry Pi Pico 2 (RISC-V Hazard3)"
echo "  5) Raspberry Pi Pico 2 W (ARM Cortex-M33)"
echo "  6) Raspberry Pi Pico 2 W (RISC-V Hazard3)"
echo "  7) Adafruit Feather RP2350 (ARM Cortex-M33)"
echo "  8) Adafruit Feather RP2350 (RISC-V Hazard3)"
read -rp "Board [1]: " board_choice
case "$board_choice" in
    2) BOARD="pico_w" ;;
    3) BOARD="pico2" ;;
    4) BOARD="pico2_riscv" ;;
    5) BOARD="pico2_w" ;;
    6) BOARD="pico2_w_riscv" ;;
    7) BOARD="adafruit_feather_rp2350" ;;
    8) BOARD="adafruit_feather_rp2350_riscv" ;;
    *) BOARD="pico" ;;
esac
echo "  Selected: $BOARD"
echo

# User account configuration
read -rp "Enable non-root user account? [Y/n] " enable_user
enable_user=${enable_user:-$DEFAULT_ENABLE_USER}

if [[ "$enable_user" =~ ^[Yy]$ ]]; then
    echo
    read -rp "Username [$DEFAULT_USERNAME]: " username
    username=${username:-$DEFAULT_USERNAME}

    read -rp "UID [$DEFAULT_UID]: " uid
    uid=${uid:-$DEFAULT_UID}

    echo "Capabilities are a bitmask of CAP_* flags from permissions.h"
    echo "For now, enter 0 for no special capabilities."
    read -rp "Capability bitmask [$DEFAULT_CAPS]: " caps
    caps=${caps:-$DEFAULT_CAPS}

    USER_CMAKE_OPTS=(
        "-DLITTLEOS_ENABLE_USER_ACCOUNT=ON"
        "-DLITTLEOS_USER_NAME=${username}"
        "-DLITTLEOS_USER_UID=${uid}"
        "-DLITTLEOS_USER_CAPABILITIES=${caps}"
        "-DLITTLEOS_USER_UMASK=0022"
    )

    echo
    echo "Configured user:"
    echo " Username : ${username}"
    echo " UID      : ${uid}"
    echo " Capabilities: ${caps}"
    echo " Umask    : 0022"
else
    USER_CMAKE_OPTS=(
        "-DLITTLEOS_ENABLE_USER_ACCOUNT=OFF"
    )

    echo
    echo "Building root-only configuration (no extra user account)."
fi

echo
echo "Cleaning previous build..."
rm -rf build

echo "Configuring CMake..."
mkdir -p build
cd build

cmake .. -DLITTLEOS_BOARD="$BOARD" "${USER_CMAKE_OPTS[@]}"

echo
echo "Building littleOS..."
make -j"$(nproc)"

if [[ -f littleos.uf2 ]]; then
    echo
    echo "Moving firmware (littleos.uf2) to project root..."
    mv littleos.uf2 ../
else
    echo "Warning: littleos.uf2 not found in build directory."
fi

cd ..

echo
if [[ "$enable_user" =~ ^[Yy]$ ]]; then
    echo "Build complete with user account: $username (UID $uid)"
else
    echo "Build complete (root-only configuration)"
fi
echo "Board: $BOARD"
echo "Firmware: ./littleos.uf2"
