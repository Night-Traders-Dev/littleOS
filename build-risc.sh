#!/usr/bin/env bash
#
# build-risc.sh — Clone and build the RISC-V bare-metal toolchain
# Installs riscv32-unknown-elf-gcc to /opt/risc-v-hazard
#

set -e

PREFIX="/opt/risc-v-hazard"
WORKDIR="/tmp/riscv-toolchain-build"
REPO="https://github.com/riscv-collab/riscv-gnu-toolchain.git"
JOBS="$(nproc)"

echo "========================================="
echo " RISC-V Bare-Metal Toolchain Builder"
echo "========================================="
echo
echo "  Target triple : riscv32-unknown-elf"
echo "  Install prefix: $PREFIX"
echo "  Build jobs    : $JOBS"
echo "  Work directory: $WORKDIR"
echo

# =========================================================================
# Prerequisites
# =========================================================================
echo "Checking build dependencies..."

MISSING=()
for pkg in autoconf automake autotools-dev curl python3 python3-pip \
           libmpc-dev libmpfr-dev libgmp-dev gawk build-essential \
           bison flex texinfo gperf libtool patchutils bc zlib1g-dev \
           libexpat-dev ninja-build git; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        MISSING+=("$pkg")
    fi
done

if [[ ${#MISSING[@]} -gt 0 ]]; then
    echo "Installing missing packages: ${MISSING[*]}"
    sudo apt-get update
    sudo apt-get install -y "${MISSING[@]}"
else
    echo "  All dependencies satisfied."
fi
echo

# =========================================================================
# Clone
# =========================================================================
if [[ -d "$WORKDIR/riscv-gnu-toolchain" ]]; then
    echo "Source tree already exists at $WORKDIR/riscv-gnu-toolchain"
    echo "Pulling latest changes..."
    cd "$WORKDIR/riscv-gnu-toolchain"
    git pull
else
    echo "Cloning riscv-gnu-toolchain..."
    mkdir -p "$WORKDIR"
    cd "$WORKDIR"
    git clone "$REPO"
    cd riscv-gnu-toolchain
fi
echo

# =========================================================================
# Configure
# =========================================================================
echo "Configuring for riscv32-unknown-elf (newlib bare-metal)..."
echo

sudo mkdir -p "$PREFIX"
sudo chown "$(whoami)" "$PREFIX"

mkdir -p build
cd build

../configure \
    --prefix="$PREFIX" \
    --with-arch=rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb \
    --with-abi=ilp32

# =========================================================================
# Build
# =========================================================================
echo
echo "Building toolchain with $JOBS parallel jobs..."
echo "  (This will take a while — typically 20-40 minutes)"
echo

make -j"$JOBS"

# =========================================================================
# Verify
# =========================================================================
echo
echo "========================================="
echo " Build complete!"
echo "========================================="
echo

if [[ -x "$PREFIX/bin/riscv32-unknown-elf-gcc" ]]; then
    echo "Installed toolchain:"
    "$PREFIX/bin/riscv32-unknown-elf-gcc" --version | head -1
    echo
    echo "Binaries in: $PREFIX/bin/"
    ls "$PREFIX/bin/" | head -20
    echo
    echo "To use with littleOS, either:"
    echo "  1) Add to PATH:  export PATH=\"$PREFIX/bin:\$PATH\""
    echo "  2) build.sh will auto-detect it if you add $PREFIX/bin"
    echo "     to the search list in setup_riscv_toolchain()"
else
    echo "ERROR: riscv32-unknown-elf-gcc not found at $PREFIX/bin"
    echo "Build may have failed — check output above."
    exit 1
fi
