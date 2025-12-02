# littleOS Boot Sequence

## Boot Flow

```
┌─────────────────────────────────────┐
│  Pico SDK Boot (crt0.S)             │
│  - Initialize clocks                │
│  - Initialize memory                │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  boot/boot.c - main()               │
│  - stdio_init_all()                 │
│  - Wait for USB connection          │
│  - sleep_ms(1000)                   │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  src/kernel.c - kernel_main()       │
│  - Display "RP2040 littleOS kernel" │
│  - sage_init() → Initialize SageLang│
│  - script_storage_init()            │
│  - Display boot messages            │
│  - sleep_ms(3000) → 3 second delay  │
│  - Clear screen (ANSI codes)        │
│  - Display welcome message          │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  src/shell/shell.c - shell_run()    │
│  - Main command loop                │
│  - Process user commands            │
└─────────────────────────────────────┘
```

---

## Boot Messages

### Phase 1: Initialization (3 seconds)

```
RP2040 littleOS kernel
SageLang: Embedded mode (64KB heap)
GPIO: Registered 5 native functions
System: Registered 9 native functions
Time: Registered sleep() function
SageLang initialized
Script storage initialized
```

**Duration:** Displayed for 3 seconds

**Purpose:** 
- Show system initialization status
- Confirm all subsystems loaded
- Verify native function registration

---

### Phase 2: Welcome Screen (after clear)

```
Welcome to littleOS Shell!
Type 'help' for available commands
>
```

**What happens:**
1. Screen clears using ANSI escape codes (`\033[2J\033[H`)
2. Welcome message displayed
3. Shell prompt ready for input

---

## SageLang Initialization

During `sage_init()`, the following functions are registered:

### GPIO Functions (5)
- `gpio_init(pin, is_output)`
- `gpio_write(pin, value)`
- `gpio_read(pin)`
- `gpio_toggle(pin)`
- `gpio_set_pull(pin, mode)`

### System Functions (9)
- `sys_version()`
- `sys_uptime()`
- `sys_temp()`
- `sys_clock()`
- `sys_free_ram()`
- `sys_total_ram()`
- `sys_board_id()`
- `sys_info()`
- `sys_print()`

### Time Functions (1)
- `sleep(milliseconds)`

**Total:** 15 native functions registered

---

## Reboot Functionality

### Command
```bash
> reboot
```

### Implementation

Located in `src/shell/shell.c`:

```c
if (strcmp(argv[0], "reboot") == 0) {
    printf("Rebooting system...\r\n");
    sleep_ms(500);  // Give time for message to transmit
    
    // Use watchdog to trigger a reset
    watchdog_enable(1, 1);  // 1ms timeout, no pause on debug
    while(1) {
        // Wait for watchdog to trigger reset
    }
}
```

### How It Works

1. **Display message**: "Rebooting system..."
2. **Wait 500ms**: Ensures message is transmitted over UART/USB
3. **Enable watchdog**: 1ms timeout
4. **Trigger reset**: Watchdog fires and resets the RP2040
5. **Restart**: Boot sequence begins again from Phase 1

### Technical Details

- **Method**: Hardware watchdog timer reset
- **Delay**: 1ms timeout (minimum)
- **SDK Function**: `watchdog_enable(timeout_ms, pause_on_debug)`
- **Result**: Complete system restart (equivalent to power cycle)

**Note:** This is a hardware reset, not a software reset. All peripherals, GPIO states, and memory are reset to their default states.

---

## Available Shell Commands

After boot, the following commands are available:

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `version` | Show OS and SageLang version |
| `clear` | Clear the screen |
| `reboot` | Reboot the system |
| `sage` | Enter SageLang REPL |
| `script` | Manage stored scripts |

---

## Screen Clear Mechanism

### ANSI Escape Codes

```c
printf("\033[2J");   // Clear entire screen
printf("\033[H");    // Move cursor to home (1,1)
```

**Compatibility:**
- ✅ Works with most terminal emulators (PuTTY, screen, minicom)
- ✅ Works with USB serial terminals
- ✅ Works with UART terminals
- ❌ May not work with very basic terminals

---

## Boot Timing

| Phase | Duration | Description |
|-------|----------|-------------|
| SDK Boot | ~100ms | Hardware initialization |
| USB Wait | Variable | Wait for USB connection (if enabled) |
| Initial Delay | 1000ms | Allow USB enumeration |
| SageLang Init | ~50ms | Initialize interpreter and register functions |
| Boot Message Display | 3000ms | Show initialization status |
| **Total** | **~4.2s** | From power-on to shell ready |

---

## Customization

### Change Boot Delay

In `src/kernel.c`, modify:

```c
// Display boot log for 3 seconds
sleep_ms(3000);  // Change to desired milliseconds
```

### Disable Screen Clear

Comment out in `src/kernel.c`:

```c
// printf("\033[2J\033[H");  // Disabled
```

### Custom Welcome Message

Edit in `src/kernel.c`:

```c
printf("\r\n");
printf("Welcome to littleOS Shell!\r\n");  // Customize here
printf("Type 'help' for available commands\r\n");
```

---

## Troubleshooting

### Boot messages not visible
- Check USB connection
- Verify terminal baud rate (115200)
- Ensure `stdio_usb_connected()` returns true

### Screen doesn't clear
- Terminal may not support ANSI escape codes
- Try a different terminal emulator (PuTTY recommended)

### Reboot doesn't work
- Watchdog may not be enabled in SDK
- Check `hardware_watchdog` is linked in CMakeLists.txt
- Verify BOOTSEL button isn't held during reset

### Boot takes too long
- USB enumeration can take 1-2 seconds
- Disable USB stdio and use UART only for faster boot
- Reduce boot message delay in kernel.c

---

## Version Information

- **littleOS**: 0.2.0
- **SageLang**: 0.8.0
- **Target**: Raspberry Pi Pico (RP2040)
- **Last Updated**: December 2, 2025
