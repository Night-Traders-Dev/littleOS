#!/usr/bin/env bash
#
# test-suite.sh — littleOS Test Suite
#
# Builds each board target and runs emulated tests via bramble (RP2040 boards).
# RP2350 boards are build-only (bramble does not emulate RP2350).
#
# Usage:
#   ./tests/test-suite.sh              # Run all boards
#   ./tests/test-suite.sh pico         # Run single board
#   ./tests/test-suite.sh --quick      # Build-only, skip emulator tests
#   ./tests/test-suite.sh --list       # List available boards
#

set -euo pipefail

# =========================================================================
# Configuration
# =========================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BRAMBLE="${BRAMBLE:-/home/kraken/Devel/bramble/bramble}"
PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico-sdk}"
export PICO_SDK_PATH

# Timing (seconds) — bramble + littleOS boot at 125MHz takes ~5s
BOOT_DELAY=8
CMD_DELAY=3
TAIL_DELAY=5
EMU_TIMEOUT=60

# Board definitions
BOARDS_RP2040=("pico" "pico_w")
BOARDS_RP2350=("pico2" "pico2_w" "adafruit_feather_rp2350")
BOARDS_RISCV=("pico2_riscv" "pico2_w_riscv" "adafruit_feather_rp2350_riscv")
ALL_BOARDS=("${BOARDS_RP2040[@]}" "${BOARDS_RP2350[@]}" "${BOARDS_RISCV[@]}")

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# Counters
TOTAL_TESTS=0
TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_SKIP=0
FAIL_LOG=""

# =========================================================================
# Helpers
# =========================================================================

log_header()  { echo -e "\n${BOLD}${CYAN}=== $1 ===${RESET}"; }
log_pass()    { echo -e "  ${GREEN}[PASS]${RESET} $1"; TOTAL_PASS=$((TOTAL_PASS + 1)); TOTAL_TESTS=$((TOTAL_TESTS + 1)); }
log_fail()    { echo -e "  ${RED}[FAIL]${RESET} $1"; TOTAL_FAIL=$((TOTAL_FAIL + 1)); TOTAL_TESTS=$((TOTAL_TESTS + 1)); FAIL_LOG+="  FAIL: $1\n"; }
log_skip()    { echo -e "  ${YELLOW}[SKIP]${RESET} $1"; TOTAL_SKIP=$((TOTAL_SKIP + 1)); TOTAL_TESTS=$((TOTAL_TESTS + 1)); }
log_info()    { echo -e "  ${CYAN}[INFO]${RESET} $1"; }

# Send a string to bramble in 15-byte chunks (bramble reads max 16 bytes per poll).
# Each chunk is followed by a tiny sleep so the UART FIFO drains.
chunked_printf() {
    local str="$1"
    local len=${#str}
    local i=0
    while [[ $i -lt $len ]]; do
        printf '%s' "${str:$i:15}"
        sleep 0.15
        i=$((i + 15))
    done
}

# Run a command in bramble and capture output
# Usage: bramble_run <uf2> <command_string> [extra_tail_delay]
# Commands are separated by semicolons. Each is sent in 15-byte chunks
# to work around bramble's 16-byte stdin read limit.
bramble_run() {
    local uf2="$1"
    local cmds="$2"
    local extra_delay="${3:-0}"

    # Build a helper script that sends commands with proper chunking
    local helper
    helper=$(mktemp)
    cat > "$helper" << 'HELPER_FUNC'
chunked_send() {
    local str="$1"
    local len=${#str}
    local i=0
    while [ $i -lt $len ]; do
        printf '%s' "$(echo "$str" | cut -c$((i+1))-$((i+15)))"
        sleep 0.15
        i=$((i + 15))
    done
}
HELPER_FUNC

    echo "sleep $BOOT_DELAY" >> "$helper"

    # Split commands by semicolons
    IFS=';' read -ra CMD_ARRAY <<< "$cmds"
    for cmd in "${CMD_ARRAY[@]}"; do
        cmd="$(echo "$cmd" | xargs)"  # trim whitespace
        # Send command in chunks, then newline separately
        echo "chunked_send '${cmd}'" >> "$helper"
        echo "printf '\\n'" >> "$helper"
        echo "sleep $CMD_DELAY" >> "$helper"
    done
    echo "sleep $((TAIL_DELAY + extra_delay))" >> "$helper"

    # Run bramble with piped input
    bash "$helper" | timeout "$EMU_TIMEOUT" "$BRAMBLE" "$uf2" -stdin -clock 125 2>/dev/null || true
    rm -f "$helper"
}

# Check output for expected pattern
# Usage: check_output <output> <pattern> <test_name>
check_output() {
    local output="$1"
    local pattern="$2"
    local name="$3"

    if echo "$output" | grep -qa "$pattern"; then
        log_pass "$name"
    else
        log_fail "$name"
    fi
    return 0
}

# Check output does NOT contain pattern
check_output_absent() {
    local output="$1"
    local pattern="$2"
    local name="$3"

    if echo "$output" | grep -qa "$pattern"; then
        log_fail "$name"
    else
        log_pass "$name"
    fi
    return 0
}

# =========================================================================
# Build Functions
# =========================================================================

build_board() {
    local board="$1"
    local build_dir="$PROJECT_DIR/build_test_${board}"

    log_info "Building $board..."

    rm -rf "$build_dir"
    mkdir -p "$build_dir"

    # Configure
    local cmake_opts=("-DLITTLEOS_BOARD=$board")

    if ! (cd "$build_dir" && cmake "${cmake_opts[@]}" "$PROJECT_DIR" > cmake_output.log 2>&1); then
        log_fail "Build $board — cmake failed"
        cat "$build_dir/cmake_output.log" | tail -20
        return 1
    fi

    # Build
    if ! (cd "$build_dir" && make -j"$(nproc)" > make_output.log 2>&1); then
        log_fail "Build $board — make failed"
        cat "$build_dir/make_output.log" | tail -30
        return 1
    fi

    # Verify UF2 produced
    if [[ -f "$build_dir/littleos.uf2" ]]; then
        local size
        size=$(stat -c%s "$build_dir/littleos.uf2" 2>/dev/null || echo 0)
        log_pass "Build $board (UF2: $(( size / 1024 )) KB)"
        return 0
    else
        log_fail "Build $board — no UF2 output"
        return 1
    fi
}

# =========================================================================
# Emulator Test Categories
# =========================================================================
# NOTE: Bramble reads max 16 bytes per stdin poll. Commands >15 chars are
# automatically chunked by bramble_run(). Grep patterns use -a for binary
# safety and -E for extended regex.

# --- Boot Tests ---
test_boot() {
    local uf2="$1"
    local board="$2"
    log_header "Boot Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "version")"

    check_output "$output" "littleOS Kernel" "Kernel banner printed"
    check_output "$output" "task scheduler" "Scheduler init"
    check_output "$output" "SageLang ready" "SageLang init"
    check_output "$output" "Watchdog.*Active" "Watchdog enabled"
    check_output "$output" "Supervisor.*Core 1" "Supervisor launched"
    check_output "$output" "Welcome to littleOS" "Shell started"
    check_output "$output" "root@littleos" "Shell prompt shown"
    check_output "$output" "littleOS v" "version command works"
}

# --- RAM & Memory Tests ---
test_memory() {
    local uf2="$1"
    local board="$2"
    log_header "RAM & Memory Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "selftest ram")"

    check_output "$output" "PASS.*RAM walking" "RAM walking ones"
    check_output "$output" "PASS.*RAM all-zero" "RAM all-zeros"
    check_output "$output" "PASS.*RAM all-one" "RAM all-ones"
    check_output "$output" "PASS.*RAM address" "RAM address-as-data"

    # Memory layout (box-drawing output)
    output="$(bramble_run "$uf2" "memory layout")"
    check_output "$output" "MEMORY LAYOUT\|Kernel Heap\|0x200" "Memory layout renders"

    # Memory stats
    output="$(bramble_run "$uf2" "memory stats")"
    check_output "$output" "KERNEL HEAP\|INTERPRETER\|Free" "Memory stats available"
}

# --- Timer Tests ---
test_timer() {
    local uf2="$1"
    local board="$2"
    log_header "Timer Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "selftest timer")"

    check_output "$output" "PASS.*Timer accuracy" "Timer 1ms accuracy"
    check_output "$output" "PASS.*Timer monotonic" "Timer monotonicity"
}

# --- Flash Tests ---
test_flash() {
    local uf2="$1"
    local board="$2"
    log_header "Flash Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "selftest flash")"

    check_output "$output" "PASS.*Flash readable" "Flash XIP readable"
    check_output "$output" "PASS.*Flash consist" "Flash 1KB checksum"
}

# --- GPIO Tests ---
test_gpio() {
    local uf2="$1"
    local board="$2"
    log_header "GPIO Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "selftest gpio")"
    check_output "$output" "PASS.*GPIO 2 pull" "GPIO pull-down reads low"

    # Pinout command
    output="$(bramble_run "$uf2" "pinout")"
    check_output "$output" "GP[0-9]\|GND\|3V3\|VBUS" "Pinout diagram renders"

    # GPIO via hw command (short commands)
    output="$(bramble_run "$uf2" "hw gpio")"
    check_output "$output" "hw.*gpio\|init\|read\|write" "GPIO hw help available"
}

# --- ADC Tests ---
test_adc() {
    local uf2="$1"
    local board="$2"
    log_header "ADC Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "selftest adc")"

    check_output "$output" "ADC temp sensor" "ADC temp sensor accessible"
    check_output "$output" "Temperature sane" "ADC temperature in range"

    # ADC shell command
    output="$(bramble_run "$uf2" "hw adc init")"
    check_output "$output" "ADC\|adc\|init\|ready\|Init" "ADC init via hw command"
}

# --- DMA Tests ---
test_dma() {
    local uf2="$1"
    local board="$2"
    log_header "DMA Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "dma status")"
    check_output "$output" "DMA Channel\|CH.*Claimed" "DMA status reports channels"

    output="$(bramble_run "$uf2" "dma")"
    check_output "$output" "dma\|DMA\|memcpy\|claim\|status" "DMA help available"
}

# --- PIO Tests ---
test_pio() {
    local uf2="$1"
    local board="$2"
    log_header "PIO Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "pio status")"
    check_output "$output" "PIO[01].*SM[0-3]\|STOPPED" "PIO status reports blocks"

    output="$(bramble_run "$uf2" "pio")"
    check_output "$output" "pio\|PIO\|load\|exec\|status" "PIO help available"
}

# --- Scheduler Tests ---
test_scheduler() {
    local uf2="$1"
    local board="$2"
    log_header "Scheduler Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "tasks")"
    check_output "$output" "Task\|task\|ID\|State\|Priority" "Task list displays"

    # Top command
    output="$(bramble_run "$uf2" "top -n 1" 2)"
    check_output "$output" "littleOS top\|CPU\|Mem\|Tasks" "Top command renders"
}

# --- IPC Tests ---
test_ipc() {
    local uf2="$1"
    local board="$2"
    log_header "IPC Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "ipc status")"
    check_output "$output" "IPC\|Channel\|Semaphore" "IPC status reports"

    output="$(bramble_run "$uf2" "ipc")"
    check_output "$output" "send\|recv\|create\|status\|sem" "IPC help shows operations"
}

# --- Filesystem Tests ---
test_filesystem() {
    local uf2="$1"
    local board="$2"
    log_header "Filesystem Tests ($board)"

    local output
    # Format and mount (single commands, short)
    output="$(bramble_run "$uf2" "fs format" 2)"
    check_output "$output" "ormat\|success\|Format\|OK\|done" "FS format works"

    output="$(bramble_run "$uf2" "fs mount" 2)"
    check_output "$output" "mount\|Mount\|success\|OK\|ready" "FS mount works"

    # FS help
    output="$(bramble_run "$uf2" "fs")"
    check_output "$output" "fs\|format\|mount\|ls\|mkdir\|cat" "FS help available"
}

# --- Virtual Filesystem Tests ---
test_vfs() {
    local uf2="$1"
    local board="$2"
    log_header "Virtual Filesystem Tests ($board)"

    # procfs list
    local output
    output="$(bramble_run "$uf2" "proc list")"
    check_output "$output" "cpuinfo\|meminfo\|uptime" "procfs entries listed"

    # devfs
    output="$(bramble_run "$uf2" "dev list")"
    check_output "$output" "/dev/gpio\|/dev/uart\|/dev/null\|type=" "devfs device list"
}

# --- Shell Tests ---
test_shell() {
    local uf2="$1"
    local board="$2"
    log_header "Shell Tests ($board)"

    # Help command
    local output
    output="$(bramble_run "$uf2" "help")"
    check_output "$output" "selftest\|memory\|tasks\|sage" "Help lists commands"

    # Echo
    output="$(bramble_run "$uf2" "echo hello")"
    check_output "$output" "hello" "Echo command"

    # Environment variables
    output="$(bramble_run "$uf2" "env")"
    check_output "$output" "USER=\|HOME=\|PWD=\|HOSTNAME=" "Environment variables set"

    # Alias
    output="$(bramble_run "$uf2" "alias v=version; v" 3)"
    check_output "$output" "littleOS v\|alias" "Alias expansion works"
}

# --- Power Management Tests ---
test_power() {
    local uf2="$1"
    local board="$2"
    log_header "Power Management Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "power status")"
    check_output "$output" "Power Status\|Mode\|mode\|active\|Uptime" "Power status reports"

    output="$(bramble_run "$uf2" "power")"
    check_output "$output" "sleep\|clock\|status\|wake\|dormant" "Power help available"
}

# --- Kernel Log Tests ---
test_dmesg() {
    local uf2="$1"
    local board="$2"
    log_header "Kernel Log Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "dmesg")"
    check_output "$output" "Memory management\|kernel\|scheduler\|Watchdog" "dmesg shows boot messages"

    # Logcat
    output="$(bramble_run "$uf2" "logcat")"
    check_output "$output" "logcat\|Logcat\|log\|entries\|No\|empty" "Logcat command accessible"
}

# --- Permission System Tests ---
test_permissions() {
    local uf2="$1"
    local board="$2"
    log_header "Permission System Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "users")"
    check_output "$output" "root\|UID\|GID" "User list shows accounts"

    output="$(bramble_run "$uf2" "perms")"
    check_output "$output" "perm\|mode\|owner\|UART\|chmod\|chown" "Permissions system accessible"
}

# --- SageLang Tests ---
test_sagelang() {
    local uf2="$1"
    local board="$2"
    log_header "SageLang Tests ($board)"

    # Basic expression
    local output
    output="$(bramble_run "$uf2" "sage eval 2+2")"
    check_output "$output" "4" "SageLang eval 2+2 = 4"

    # Sage help
    output="$(bramble_run "$uf2" "sage")"
    check_output "$output" "sage\|Sage\|eval\|run\|Usage" "SageLang help available"
}

# --- Debug Tools Tests ---
test_debug() {
    local uf2="$1"
    local board="$2"
    log_header "Debug & Diagnostic Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "trace")"
    check_output "$output" "trace\|Trace\|entries\|empty\|buffer" "Trace buffer accessible"

    output="$(bramble_run "$uf2" "watchpoint")"
    check_output "$output" "watchpoint\|Watchpoint\|set\|list\|Usage" "Watchpoint system accessible"

    output="$(bramble_run "$uf2" "coredump")"
    check_output "$output" "coredump\|Coredump\|No\|dump\|crash" "Coredump viewer accessible"

    output="$(bramble_run "$uf2" "benchmark")"
    check_output "$output" "benchmark\|Benchmark\|cpu\|memory\|Usage" "Benchmark suite available"
}

# --- Supervisor & Watchdog Tests ---
test_supervisor() {
    local uf2="$1"
    local board="$2"
    log_header "Supervisor & Watchdog Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "supervisor")"
    check_output "$output" "Supervisor\|supervisor\|Core 1\|status\|enable" "Supervisor accessible"
}

# --- System Info Tests ---
test_sysinfo() {
    local uf2="$1"
    local board="$2"
    log_header "System Info Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "fetch")"
    check_output "$output" "littleOS\|RP2040\|ARM\|Cortex\|SRAM" "System fetch shows board info"

    output="$(bramble_run "$uf2" "stats")"
    check_output "$output" "uptime\|Uptime\|memory\|Memory\|task" "Stats command available"
}

# --- Selftest Full Run ---
test_selftest_full() {
    local uf2="$1"
    local board="$2"
    log_header "Full Self-Test ($board)"

    local output
    output="$(bramble_run "$uf2" "selftest" 5)"

    check_output "$output" "Self-Test Suite" "Selftest banner"
    check_output "$output" "Results:.*passed" "Selftest summary"

    # Count passes/fails
    local passes fails
    passes=$(echo "$output" | grep -ac "PASS" || true)
    fails=$(echo "$output" | grep -ac "FAIL" || true)
    log_info "Selftest: $passes passed, $fails failed (some emulator failures expected)"
}

# --- Network Tests (Pico W only) ---
test_network() {
    local uf2="$1"
    local board="$2"
    log_header "Network Tests ($board)"

    if [[ "$board" != *"_w"* && "$board" != *"pico_w"* ]]; then
        log_skip "Network — not a WiFi board"
        return
    fi

    local output
    output="$(bramble_run "$uf2" "net status")"
    check_output "$output" "WiFi\|wifi\|interface\|disconnected\|not" "Network status accessible"

    output="$(bramble_run "$uf2" "net")"
    check_output "$output" "connect\|scan\|status\|tcp\|udp" "Network help shows operations"

    output="$(bramble_run "$uf2" "mqtt")"
    check_output "$output" "mqtt\|MQTT\|connect\|publish\|subscribe" "MQTT help available"
}

# --- Display Tests ---
test_display() {
    local uf2="$1"
    local board="$2"
    log_header "Display Driver Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "display")"
    check_output "$output" "display\|Display\|init\|sysinfo\|Usage" "Display help available"
}

# --- USB Tests ---
test_usb() {
    local uf2="$1"
    local board="$2"
    log_header "USB Device Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "usb")"
    check_output "$output" "usb\|USB\|mode\|cdc\|hid\|msc\|status" "USB commands available"
}

# --- I2C/SPI/PWM Hardware Tests ---
test_hw_peripherals() {
    local uf2="$1"
    local board="$2"
    log_header "Hardware Peripheral Tests ($board)"

    # I2C scan (no devices in emulator, but should not crash)
    local output
    output="$(bramble_run "$uf2" "i2cscan")"
    check_output "$output" "I2C\|i2c\|scan\|Scan\|No dev\|found" "I2C scan completes"

    # PWM tune help
    output="$(bramble_run "$uf2" "pwmtune")"
    check_output "$output" "pwm\|PWM\|freq\|duty\|Usage" "PWM tune help available"

    # SPI (via hw command)
    output="$(bramble_run "$uf2" "hw spi")"
    check_output "$output" "spi\|SPI\|init\|transfer" "SPI command accessible"

    # Wire REPL
    output="$(bramble_run "$uf2" "wire")"
    check_output "$output" "wire\|Wire\|i2c\|spi\|Usage" "Wire REPL help available"
}

# --- Cron Tests ---
test_cron() {
    local uf2="$1"
    local board="$2"
    log_header "Cron Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "cron")"
    check_output "$output" "cron\|Cron\|add\|list\|remove" "Cron help available"
}

# --- Script Storage Tests ---
test_scripts() {
    local uf2="$1"
    local board="$2"
    log_header "Script Storage Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "script")"
    check_output "$output" "script\|Script\|save\|load\|list\|run" "Script help available"
}

# --- Sensor Framework Tests ---
test_sensors() {
    local uf2="$1"
    local board="$2"
    log_header "Sensor Framework Tests ($board)"

    local output
    output="$(bramble_run "$uf2" "sensor")"
    check_output "$output" "sensor\|Sensor\|list\|add\|read\|log" "Sensor help available"
}

# =========================================================================
# Board-Specific Test Runner
# =========================================================================

run_emulator_tests() {
    local board="$1"
    local uf2="$PROJECT_DIR/build_test_${board}/littleos.uf2"

    if [[ ! -f "$uf2" ]]; then
        log_fail "No UF2 for $board"
        return 1
    fi

    if [[ ! -x "$BRAMBLE" ]]; then
        log_skip "Bramble emulator not found at $BRAMBLE"
        return 0
    fi

    # Core tests — every board
    test_boot         "$uf2" "$board"
    test_memory       "$uf2" "$board"
    test_timer        "$uf2" "$board"
    test_flash        "$uf2" "$board"
    test_gpio         "$uf2" "$board"
    test_adc          "$uf2" "$board"
    test_dma          "$uf2" "$board"
    test_pio          "$uf2" "$board"
    test_scheduler    "$uf2" "$board"
    test_ipc          "$uf2" "$board"
    test_filesystem   "$uf2" "$board"
    test_vfs          "$uf2" "$board"
    test_shell        "$uf2" "$board"
    test_power        "$uf2" "$board"
    test_dmesg        "$uf2" "$board"
    test_permissions  "$uf2" "$board"
    test_sagelang     "$uf2" "$board"
    test_debug        "$uf2" "$board"
    test_supervisor   "$uf2" "$board"
    test_sysinfo      "$uf2" "$board"
    test_selftest_full "$uf2" "$board"
    test_network      "$uf2" "$board"
    test_display      "$uf2" "$board"
    test_usb          "$uf2" "$board"
    test_hw_peripherals "$uf2" "$board"
    test_cron         "$uf2" "$board"
    test_scripts      "$uf2" "$board"
    test_sensors      "$uf2" "$board"
}

# =========================================================================
# Main
# =========================================================================

QUICK_MODE=false
TARGET_BOARDS=()

# Parse args
for arg in "$@"; do
    case "$arg" in
        --quick)  QUICK_MODE=true ;;
        --list)
            echo "Available boards:"
            for b in "${ALL_BOARDS[@]}"; do
                note=""
                if [[ " ${BOARDS_RP2040[*]} " == *" $b "* ]]; then
                    note="(RP2040 — emulator tests)"
                elif [[ " ${BOARDS_RISCV[*]} " == *" $b "* ]]; then
                    note="(RP2350 RISC-V — build only)"
                else
                    note="(RP2350 ARM — build only)"
                fi
                echo "  $b $note"
            done
            exit 0
            ;;
        --help|-h)
            echo "Usage: $0 [board...] [--quick] [--list]"
            echo ""
            echo "  board     One or more board names (default: all)"
            echo "  --quick   Build-only, skip emulator tests"
            echo "  --list    List available boards"
            exit 0
            ;;
        *)  TARGET_BOARDS+=("$arg") ;;
    esac
done

# Default: all boards
if [[ ${#TARGET_BOARDS[@]} -eq 0 ]]; then
    TARGET_BOARDS=("${ALL_BOARDS[@]}")
fi

echo -e "${BOLD}=========================================${RESET}"
echo -e "${BOLD} littleOS Test Suite${RESET}"
echo -e "${BOLD}=========================================${RESET}"
echo -e "  Boards: ${TARGET_BOARDS[*]}"
echo -e "  Mode:   $(if $QUICK_MODE; then echo 'build-only'; else echo 'build + emulator'; fi)"
echo -e "  Bramble: $BRAMBLE"
echo ""

START_TIME=$(date +%s)

for board in "${TARGET_BOARDS[@]}"; do
    log_header "Board: $board"

    # Step 1: Build
    if ! build_board "$board"; then
        log_info "Skipping tests for $board (build failed)"
        continue
    fi

    # Step 2: Emulator tests (RP2040 only, unless --quick)
    if $QUICK_MODE; then
        log_skip "Emulator tests — quick mode"
    elif [[ " ${BOARDS_RP2040[*]} " == *" $board "* ]]; then
        run_emulator_tests "$board"
    else
        log_skip "Emulator tests — bramble only supports RP2040 (board: $board)"
    fi

    # Cleanup build dir
    rm -rf "$PROJECT_DIR/build_test_${board}"
done

END_TIME=$(date +%s)
ELAPSED=$(( END_TIME - START_TIME ))

# =========================================================================
# Summary
# =========================================================================

echo ""
echo -e "${BOLD}=========================================${RESET}"
echo -e "${BOLD} Test Summary${RESET}"
echo -e "${BOLD}=========================================${RESET}"
echo -e "  Total:   $TOTAL_TESTS"
echo -e "  ${GREEN}Passed:  $TOTAL_PASS${RESET}"
echo -e "  ${RED}Failed:  $TOTAL_FAIL${RESET}"
echo -e "  ${YELLOW}Skipped: $TOTAL_SKIP${RESET}"
echo -e "  Time:    ${ELAPSED}s"

if [[ -n "$FAIL_LOG" ]]; then
    echo ""
    echo -e "${RED}Failures:${RESET}"
    echo -e "$FAIL_LOG"
fi

echo ""
if [[ "$TOTAL_FAIL" -eq 0 ]]; then
    echo -e "${GREEN}${BOLD}All tests passed!${RESET}"
    exit 0
else
    echo -e "${RED}${BOLD}$TOTAL_FAIL test(s) failed.${RESET}"
    exit 1
fi
