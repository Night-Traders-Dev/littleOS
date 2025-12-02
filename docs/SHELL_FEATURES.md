# littleOS Shell Features

## Command History

### Overview

The littleOS shell maintains a command history buffer that stores up to 20 previously executed commands. You can navigate through this history using arrow keys.

### Usage

| Key | Action |
|-----|--------|
| **↑** (Up Arrow) | Navigate to previous command |
| **↓** (Down Arrow) | Navigate to next command |
| **Enter** | Execute current command |
| **Backspace** | Delete character |

### Features

✅ **Stores 20 commands** - Circular buffer maintains last 20 commands  
✅ **Duplicate filtering** - Consecutive duplicate commands are not stored  
✅ **Persistent navigation** - Browse up and down through entire history  
✅ **Empty command filtering** - Empty commands are not added to history  
✅ **ANSI escape support** - Works with all standard terminal emulators  

### Examples

#### Basic Usage

```bash
> version
littleOS v0.2.0 - RP2040
With SageLang v0.8.0

> help
Available commands:
  help     - Show this help message
  ...

> [Press ↑]  # Shows: help
> [Press ↑]  # Shows: version
> [Press ↓]  # Shows: help
> [Press ↓]  # Shows: (empty)
```

#### View History

```bash
> history
Command history:
  1: version
  2: help
  3: gpio_init(25, true)
  4: sys_temp()
  5: history
```

### Technical Implementation

#### ANSI Escape Sequences

Arrow keys send multi-byte ANSI escape sequences:

- **Up Arrow**: `ESC [ A` (0x1B 0x5B 0x41)
- **Down Arrow**: `ESC [ B` (0x1B 0x5B 0x42)
- **Right Arrow**: `ESC [ C` (0x1B 0x5B 0x43) - ignored
- **Left Arrow**: `ESC [ D` (0x1B 0x5B 0x44) - ignored

#### State Machine

The shell uses a 3-state parser:

```c
enum {
    STATE_NORMAL,   // Normal character input
    STATE_ESC,      // Received ESC (0x1B)
    STATE_CSI       // Received ESC [ (Control Sequence Introducer)
} escape_state;
```

**Sequence:**
1. Receive `ESC` → STATE_NORMAL → STATE_ESC
2. Receive `[` → STATE_ESC → STATE_CSI
3. Receive `A`/`B` → STATE_CSI → Handle arrow → STATE_NORMAL

#### History Buffer

```c
#define HISTORY_SIZE 20
#define MAX_CMD_LEN 512

static char history[HISTORY_SIZE][MAX_CMD_LEN];
static int history_count = 0;   // Total commands entered
static int history_pos = 0;     // Current position in history
```

**Circular buffer logic:**
- Commands stored at `history[history_count % HISTORY_SIZE]`
- When full, oldest commands are overwritten
- Navigation wraps around buffer boundaries

### Terminal Compatibility

#### Tested Terminal Emulators

| Terminal | USB Serial | UART | Arrow Keys | Notes |
|----------|-----------|------|------------|-------|
| **PuTTY** | ✅ | ✅ | ✅ | Recommended |
| **screen** | ✅ | ✅ | ✅ | Linux/Mac |
| **minicom** | ✅ | ✅ | ✅ | Linux |
| **TeraTerm** | ✅ | ✅ | ✅ | Windows |
| **CoolTerm** | ✅ | ✅ | ✅ | Cross-platform |
| **Arduino Serial Monitor** | ✅ | ❌ | ❌ | Limited ANSI support |
| **Windows Terminal** | ✅ | ✅ | ✅ | Modern Windows |

#### Configuration

**Recommended terminal settings:**

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None
- **Local Echo**: Off (shell echoes characters)
- **Line Mode**: Off (character mode)
- **ANSI/VT100**: Enabled

### Line Editing

#### Current Features

✅ **Backspace** - Delete last character  
✅ **Arrow navigation** - Browse command history  
✅ **Line clear** - When loading history command  
✅ **Character echo** - Immediate visual feedback  

#### Future Enhancements

⬜ **Left/Right arrows** - Cursor movement within line  
⬜ **Home/End keys** - Jump to start/end of line  
⬜ **Ctrl+A/E** - Emacs-style line navigation  
⬜ **Ctrl+K/U** - Kill line forward/backward  
⬜ **Tab completion** - Command/file completion  
⬜ **Search history** - Ctrl+R reverse search  

### Troubleshooting

#### Arrow keys print garbage characters

**Problem:** Terminal not sending ANSI escape sequences

**Solutions:**
- Enable "ANSI" or "VT100" mode in terminal settings
- Try different terminal emulator (PuTTY recommended)
- Check "Application Keypad" mode is disabled

#### History doesn't work

**Problem:** Commands not being stored

**Check:**
```bash
> version
> help
> history
```

Should show both commands. If not, debug info might reveal parsing issue.

#### Commands get corrupted when navigating

**Problem:** Line clearing not working properly

**Cause:** Terminal doesn't handle backspace correctly

**Solution:** Change terminal backspace mode:
- ASCII DEL (127) - recommended
- Control-H (8) - alternative

### Performance

- **Memory usage**: ~10KB (20 commands × 512 bytes)
- **Navigation speed**: Instant (<1ms)
- **History lookup**: O(1) constant time
- **No dynamic allocation**: Fixed buffers only

### Code Example

#### Adding a command to history

```c
static void add_to_history(const char* cmd) {
    // Don't add empty commands
    if (cmd[0] == '\0') return;
    
    // Don't add duplicate of last command
    if (history_count > 0 && 
        strcmp(history[(history_count - 1) % HISTORY_SIZE], cmd) == 0) {
        return;
    }
    
    // Add to circular buffer
    strncpy(history[history_count % HISTORY_SIZE], cmd, MAX_CMD_LEN - 1);
    history[history_count % HISTORY_SIZE][MAX_CMD_LEN - 1] = '\0';
    history_count++;
    
    // Reset position to end
    history_pos = history_count;
}
```

#### Navigating history

```c
if (c == 'A') {  // Up arrow
    const char* cmd = get_history(-1);
    if (cmd != NULL) {
        clear_line(idx);  // Erase current line
        strcpy(buffer, cmd);
        idx = strlen(buffer);
        printf("%s", buffer);
        fflush(stdout);
    }
}
```

### Best Practices

1. **Use descriptive commands** - Easier to find in history
2. **Check history periodically** - Use `history` command
3. **Navigate before editing** - Browse to similar command, then modify
4. **Test with multiple terminals** - Verify ANSI compatibility

### Shell Commands Reference

| Command | Description | Since |
|---------|-------------|-------|
| `help` | Show available commands | v0.1.0 |
| `version` | Show OS version | v0.1.0 |
| `clear` | Clear screen | v0.1.0 |
| `reboot` | Reboot system | v0.2.0 |
| `history` | Show command history | v0.2.0 |
| `sage` | SageLang REPL | v0.1.0 |
| `script` | Script management | v0.1.0 |

### Keyboard Shortcuts

| Shortcut | Action | Terminal |
|----------|--------|----------|
| `Ctrl+C` | Interrupt (if implemented) | All |
| `Ctrl+D` | EOF (if implemented) | All |
| `Ctrl+L` | Clear screen (alternative) | Most |
| `↑` | Previous command | All with ANSI |
| `↓` | Next command | All with ANSI |
| `Backspace` | Delete character | All |
| `Enter` | Execute command | All |

---

## Version Information

- **Feature Added**: v0.2.0
- **History Size**: 20 commands
- **Max Command Length**: 512 characters
- **ANSI Support**: Required for arrow keys
- **Last Updated**: December 2, 2025
