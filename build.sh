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

# =========================================================================
# RISC-V Toolchain Detection
# =========================================================================
# The Pico SDK requires riscv32-unknown-elf-gcc (bare-metal) for RISC-V builds.
# Note: riscv32-unknown-linux-gnu-gcc (Linux target) will NOT work — it lacks
# nosys.specs and other bare-metal support files the SDK needs.
RISCV_AVAILABLE=false

setup_riscv_toolchain() {
    if $RISCV_AVAILABLE; then
        return 0
    fi

    # Already have the bare-metal toolchain in PATH?
    if command -v riscv32-unknown-elf-gcc &>/dev/null; then
        RISCV_AVAILABLE=true
        return 0
    fi

    # Check common install locations for bare-metal toolchain
    for dir in /opt/risc-v-hazard/bin /opt/riscv/bin /opt/riscv32-elf/bin "$HOME/.local/bin"; do
        if [[ -x "$dir/riscv32-unknown-elf-gcc" ]]; then
            export PATH="$dir:$PATH"
            RISCV_AVAILABLE=true
            return 0
        fi
    done

    # No bare-metal RISC-V toolchain found
    echo "  WARNING: No RISC-V bare-metal toolchain found."
    echo "  The Pico SDK requires riscv32-unknown-elf-gcc (bare-metal)."
    echo "  Note: riscv32-unknown-linux-gnu-gcc (Linux target) will NOT work."
    echo "  Install: https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack"
    return 1
}

# Check if a board target requires RISC-V
is_riscv_board() {
    [[ "$1" == *"riscv"* ]]
}

# Build a single board target
build_board() {
    local board="$1"
    echo "--- Building: $board ---"

    if is_riscv_board "$board"; then
        if ! setup_riscv_toolchain; then
            echo "  SKIPPED (no RISC-V toolchain)"
            echo
            return 0
        fi
    fi

    local build_dir="build_${board}"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    if (cd "$build_dir" && cmake -DLITTLEOS_BOARD="$board" .. && make -j"$(nproc)"); then
        if [[ -f "$build_dir/littleos.uf2" ]]; then
            cp "$build_dir/littleos.uf2" "littleos_${board}.uf2"
            echo "  -> littleos_${board}.uf2"
        fi
    else
        echo "  FAILED: Build error for $board"
    fi
    echo
}

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
        build_board "$board"
    done
    echo "RP2040 batch build complete."
    exit 0
fi

if [[ "$1" == "--rp2350-all" ]]; then
    echo "Batch build: All RP2350 boards"
    echo
    for board in "${BOARDS_RP2350[@]}"; do
        build_board "$board"
    done
    echo "RP2350 batch build complete."
    exit 0
fi

if [[ "$1" == "--all" ]]; then
    echo "Batch build: All boards"
    echo
    for board in "${ALL_BOARDS[@]}"; do
        build_board "$board"
    done
    echo "All boards built."
    exit 0
fi

if [[ "$1" == "--clean-all" ]]; then
    echo "Cleaning all build directories and artifacts..."
    echo
    for d in build build_*; do
        if [[ -d "$d" ]]; then
            echo "  Removing $d/"
            rm -rf "$d"
        fi
    done
    for f in littleos_*.uf2 littleos.uf2; do
        if [[ -f "$f" ]]; then
            echo "  Removing $f"
            rm -f "$f"
        fi
    done
    echo
    echo "Clean complete."
    exit 0
fi

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo "Usage: $0 [option]"
    echo
    echo "Options:"
    echo "  (none)          Interactive build with board selection"
    echo "  --rp2040-all    Build all RP2040 boards (Pico, Pico W)"
    echo "  --rp2350-all    Build all RP2350 boards (Pico 2, Pico 2 W, Feather)"
    echo "  --all           Build all boards"
    echo "  --clean-all     Remove all build directories and firmware artifacts"
    echo "  --help          Show this help"
    echo
    echo "Boards:"
    for b in "${ALL_BOARDS[@]}"; do
        echo "  $b"
    done
    echo
    echo "RISC-V toolchain:"
    echo "  The Pico SDK expects riscv32-unknown-elf-gcc."
    echo "  If you have riscv32-unknown-linux-gnu-gcc (e.g. /opt/riscv32/bin),"
    echo "  the build script will create compatibility symlinks automatically."
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

# Set up RISC-V toolchain if needed
if is_riscv_board "$BOARD"; then
    if ! setup_riscv_toolchain; then
        echo "ERROR: RISC-V toolchain required for $BOARD but not found."
        echo "Install riscv32-unknown-elf-gcc or place riscv32-unknown-linux-gnu-gcc in /opt/riscv32/bin"
        exit 1
    fi
fi

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
