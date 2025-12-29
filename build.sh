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

make clean
git pull origin main
git submodule update --init --recursive --remote


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
    )

    echo
    echo "Configured user:"
    echo "  Username    : ${username}"
    echo "  UID         : ${uid}"
    echo "  Capabilities: ${caps}"
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

cmake .. "${USER_CMAKE_OPTS[@]}"

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
echo "Build complete."
echo "Firmware (if generated): ./littleos.uf2"
