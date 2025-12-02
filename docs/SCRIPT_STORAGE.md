# Script Storage System

RAM-based script storage for littleOS with SageLang integration.

## Overview

The script storage system allows you to save, load, and manage SageLang scripts in RAM. Scripts persist across REPL sessions but are lost on reboot.

**Features:**
- ✅ Save scripts with friendly names
- ✅ Run saved scripts instantly
- ✅ List all stored scripts
- ✅ View script contents
- ✅ Update existing scripts
- ✅ Delete scripts
- ✅ Memory usage tracking

**Storage:**
- Location: RAM (linked list)
- Persistence: Until reboot
- Max name length: 32 characters
- Max code size: Limited by available RAM

## Commands

### Save a Script

```
script save <name> <code>
```

**Examples:**
```
> script save hello "print 'Hello, World!'"
Script 'hello' saved (22 bytes)

> script save countdown "let i = 5; while i > 0: print i; i = i - 1"
Script 'countdown' saved (48 bytes)

> script save fibonacci "proc fib(n): if n <= 1: return n; return fib(n-1) + fib(n-2)"
Script 'fibonacci' saved (68 bytes)
```

**Notes:**
- If script name exists, it will be updated
- Code should be enclosed in quotes if it contains spaces
- Multi-line scripts can be saved as single-line with semicolons

### Run a Script

```
script run <name>
```

**Examples:**
```
> script run hello
Running 'hello'...
Hello, World!

> script run countdown
Running 'countdown'...
5
4
3
2
1
```

### List All Scripts

```
script list
```

**Example:**
```
> script list
Saved scripts (3 total, 138 bytes):
  - hello (22 bytes)
  - countdown (48 bytes)
  - fibonacci (68 bytes)
```

### Show Script Contents

```
script show <name>
```

**Example:**
```
> script show hello
Script 'hello':
print 'Hello, World!'
```

### Delete a Script

```
script delete <name>
```

**Example:**
```
> script delete hello
Script 'hello' deleted

> script list
Saved scripts (2 total, 116 bytes):
  - countdown (48 bytes)
  - fibonacci (68 bytes)
```

### Clear All Scripts

```
script clear-scripts
```

**Example:**
```
> script clear-scripts
Cleared 2 script(s)

> script list
No scripts saved
```

### Help

```
script
```

**Example:**
```
> script
Script commands:
  save <name> <code>  - Save a script
  run <name>          - Run a saved script
  list                - List all scripts
  show <name>         - Show script contents
  delete <name>       - Delete a script
  clear-scripts       - Delete all scripts
```

## Example Workflow

### 1. Create a Utility Script

```
> script save range_test "for i in range(5): print i"
Script 'range_test' saved (31 bytes)
```

### 2. Test It

```
> script run range_test
Running 'range_test'...
0
1
2
3
4
```

### 3. Create a Function

```
> script save sum "proc sum(a, b): return a + b; print sum(10, 20)"
Script 'sum' saved (45 bytes)

> script run sum
Running 'sum'...
30
```

### 4. Build a Script Library

```
> script save greet "proc greet(name): print 'Hello, ' + name + '!'"
> script save add "proc add(a, b): return a + b"
> script save mul "proc mul(a, b): return a * b"

> script list
Saved scripts (3 total, 142 bytes):
  - greet (47 bytes)
  - add (30 bytes)
  - mul (30 bytes)
```

### 5. Update a Script

```
> script save greet "proc greet(name): print 'Hi, ' + name + '! Welcome!'"
Script 'greet' saved (54 bytes)

> script show greet
Script 'greet':
proc greet(name): print 'Hi, ' + name + '! Welcome!'
```

## Advanced Examples

### Classes

```
> script save counter "class Counter: proc init(self): self.n = 0; proc inc(self): self.n = self.n + 1; let c = Counter(); c.inc(); c.inc(); print c.n"
Script 'counter' saved (134 bytes)

> script run counter
Running 'counter'...
2
```

### Arrays and Loops

```
> script save squares "let nums = [1, 2, 3, 4, 5]; for n in nums: print n * n"
Script 'squares' saved (61 bytes)

> script run squares
Running 'squares'...
1
4
9
16
25
```

### Exception Handling

```
> script save safe_div "proc div(a, b): try: if b == 0: raise 'Div by zero'; return a / b; catch e: print 'Error: ' + e; return 0; print div(10, 2); print div(10, 0)"
Script 'safe_div' saved (151 bytes)

> script run safe_div
Running 'safe_div'...
5
Error: Div by zero
0
```

## Tips and Best Practices

### Writing Multi-Line Scripts

Use semicolons to separate statements:

```
> script save demo "let x = 10; let y = 20; print x + y"
```

Or use the REPL to develop, then save:

```
> sage
sage> proc test(): print 'working'
sage> test()
working
sage> exit

> script save test "proc test(): print 'working'; test()"
```

### Organizing Scripts

Use descriptive names:

```
utils_math_add
utils_string_trim  
test_feature_x
demo_classes
```

### Memory Management

Monitor script memory usage:

```
> script list
Saved scripts (10 total, 1.2 KB):
  ...
```

Delete unused scripts:

```
> script delete old_test
```

### Common Patterns

**Initialization script:**
```
> script save init "print 'System ready'; let version = '1.0'; print 'Version: ' + version"
> script run init
```

**Test suite:**
```
> script save test_all "print 'Running tests...'; script run test_math; script run test_strings; print 'Tests complete'"
```

## Limitations

### Current Limitations

1. **RAM Only** - Scripts are lost on reboot
   - Future: Flash storage planned

2. **No Multi-Line Editing** - Scripts must be single-line
   - Workaround: Use semicolons or develop in REPL

3. **No Script Chaining** - Can't call other saved scripts
   - Future: Script imports planned

4. **No Autorun** - Scripts don't run on boot
   - Future: Autorun feature planned

### Memory Constraints

**RP2040 RAM:**
- Total: 264 KB
- Available for scripts: ~50-100 KB
- Monitor with `script list`

**Recommendations:**
- Keep scripts under 1 KB each
- Use < 20 scripts total
- Delete unused scripts regularly

## API Reference

For developers integrating script storage:

```c
#include "script_storage.h"

// Initialize (call once at boot)
void script_storage_init(void);

// Save/update script
bool script_save(const char* name, const char* code);

// Load script code
const char* script_load(const char* name);

// Delete script
bool script_delete(const char* name);

// List all scripts
void script_list(void (*callback)(const char* name, size_t size));

// Get stats
int script_count(void);
size_t script_memory_used(void);

// Utilities
bool script_exists(const char* name);
void script_clear_all(void);
```

## Future Enhancements

### Planned Features (Phase 5)

- [ ] **Flash Storage** - Persistent scripts across reboots
- [ ] **Autorun** - Execute script on boot
- [ ] **Script Imports** - Load functions from other scripts
- [ ] **Multi-line Editor** - Better editing experience
- [ ] **Script Categories** - Organize into folders
- [ ] **Export/Import** - Transfer scripts between devices
- [ ] **Script Scheduling** - Run scripts at intervals

### Ideas for Community

- Web-based script editor
- Script sharing/marketplace
- Script debugger
- Version control for scripts
- Script testing framework

## Troubleshooting

### Script Won't Save

**Problem**: `Error: Failed to save script`

**Causes:**
- Out of memory
- Script name too long (>32 chars)
- Invalid characters in name

**Solutions:**
```
> script clear-scripts  # Free memory
> script delete unused  # Remove old scripts
```

### Script Won't Run

**Problem**: `Error: Script not found`

**Solution:**
```
> script list  # Check if script exists
> script show myScript  # Verify contents
```

### Syntax Errors

**Problem**: Script runs but throws error

**Solution:**
Test in REPL first:
```
> sage
sage> [test your code here]
sage> exit
> script save working_version "[tested code]"
```

## Contributing

Want to improve script storage? See [CONTRIBUTING.md](../CONTRIBUTING.md)

**Areas for contribution:**
- Flash storage implementation
- Script editor improvements
- Test suite
- Documentation
- Example scripts

---

**Built for littleOS with ❤️**

**Version**: littleOS v0.2.0  
**Feature**: Script Storage (RAM-based)  
**Status**: ✅ Functional
