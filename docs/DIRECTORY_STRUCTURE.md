# littleOS Directory Structure

## Proposed Organized Structure

```
littleOS/
├── boot/
│   └── boot.c                    # System entry point
├── src/
│   ├── core/                     # Core kernel components
│   │   ├── kernel.c              # Main kernel logic
│   │   ├── dmesg.c               # Kernel message buffer
│   │   ├── supervisor.c          # Core 1 health monitoring
│   │   ├── watchdog.c            # Watchdog timer management
│   │   └── multicore.c           # Multi-core coordination
│   ├── drivers/                  # Hardware abstraction layer
│   │   ├── gpio.c                # GPIO driver
│   │   ├── uart.c                # UART driver
│   │   └── system_info.c         # System information
│   ├── fs/                       # File system / storage
│   │   ├── config_storage.c      # Configuration storage
│   │   └── script_storage.c      # Script storage
│   ├── sage/                     # SageLang integration
│   │   ├── sage_embed.c          # Embedding layer
│   │   ├── sage_config.c         # Config bindings
│   │   ├── sage_gpio.c           # GPIO bindings
│   │   ├── sage_multicore.c      # Multicore bindings
│   │   ├── sage_system.c         # System bindings
│   │   ├── sage_time.c           # Timing bindings
│   │   └── sage_watchdog.c       # Watchdog bindings
│   └── shell/                    # Shell and commands
│       ├── shell.c               # Main shell loop
│       ├── cmd_dmesg.c           # dmesg command
│       ├── cmd_sage.c            # sage command
│       ├── cmd_script.c          # script command
│       └── cmd_supervisor.c      # supervisor command
├── include/
│   ├── core/                     # Core kernel headers
│   │   ├── dmesg.h
│   │   ├── multicore.h
│   │   ├── supervisor.h
│   │   └── watchdog.h
│   ├── drivers/                  # Driver headers
│   │   ├── gpio.h
│   │   └── system_info.h
│   ├── fs/                       # Storage headers
│   │   ├── config_storage.h
│   │   └── script_storage.h
│   ├── sage/                     # SageLang headers
│   │   └── sage_embed.h
│   └── reg.h                     # Register definitions
├── docs/                         # Documentation
│   ├── BOOT_SEQUENCE.md
│   ├── DMESG.md
│   ├── GPIO_INTEGRATION.md
│   ├── QUICK_REFERENCE.md
│   ├── SAGELANG_INTEGRATION.md
│   ├── SCRIPT_STORAGE.md
│   ├── SHELL_FEATURES.md
│   └── SYSTEM_INFO.md
├── third_party/
│   └── sagelang/                 # SageLang submodule
├── CMakeLists.txt
├── README.md
├── CHANGELOG.md
└── LICENSE
```

## Current Structure (Before Reorganization)

```
src/
├── config_storage.c
├── dmesg.c
├── hal/
│   └── gpio.c
├── kernel.c
├── main.c
├── multicore.c
├── sage_config.c
├── sage_embed.c
├── sage_gpio.c
├── sage_multicore.c
├── sage_system.c
├── sage_time.c
├── sage_watchdog.c
├── script_storage.c
├── shell/
│   ├── cmd_dmesg.c
│   ├── cmd_sage.c
│   ├── cmd_script.c
│   ├── cmd_supervisor.c
│   └── shell.c
├── supervisor.c
├── system_info.c
├── uart.c
└── watchdog.c

include/
├── config_storage.h
├── dmesg.h
├── hal/
│   └── gpio.h
├── multicore.h
├── reg.h
├── sage_embed.h
├── script_storage.h
├── supervisor.h
├── system_info.h
└── watchdog.h
```

## Organization Benefits

### Current Issues
1. All source files in flat `src/` directory (hard to navigate)
2. Mixed concerns (kernel, drivers, sage, shell all at same level)
3. Inconsistent organization (`hal/` exists but incomplete)
4. Header files scattered without clear grouping

### Improvements with New Structure

1. **Clear Functional Separation**
   - `src/core/` - Kernel internals
   - `src/drivers/` - Hardware abstraction
   - `src/fs/` - Storage subsystems
   - `src/sage/` - SageLang integration
   - `src/shell/` - User interface (already good)

2. **Parallel Header Organization**
   - `include/core/` matches `src/core/`
   - `include/drivers/` matches `src/drivers/`
   - Easy to find corresponding header for any source file

3. **Scalability**
   - Easy to add new drivers to `src/drivers/`
   - Easy to add new SageLang bindings to `src/sage/`
   - Clear place for new subsystems

4. **Maintainability**
   - Related files grouped together
   - Easier to understand system architecture
   - Reduces cognitive load when navigating

## Migration Mapping

### Source Files

| Current Location | New Location | Category |
|-----------------|--------------|----------|
| `src/kernel.c` | `src/core/kernel.c` | Core |
| `src/dmesg.c` | `src/core/dmesg.c` | Core |
| `src/supervisor.c` | `src/core/supervisor.c` | Core |
| `src/watchdog.c` | `src/core/watchdog.c` | Core |
| `src/multicore.c` | `src/core/multicore.c` | Core |
| `src/hal/gpio.c` | `src/drivers/gpio.c` | Drivers |
| `src/uart.c` | `src/drivers/uart.c` | Drivers |
| `src/system_info.c` | `src/drivers/system_info.c` | Drivers |
| `src/config_storage.c` | `src/fs/config_storage.c` | Storage |
| `src/script_storage.c` | `src/fs/script_storage.c` | Storage |
| `src/sage_embed.c` | `src/sage/sage_embed.c` | SageLang |
| `src/sage_config.c` | `src/sage/sage_config.c` | SageLang |
| `src/sage_gpio.c` | `src/sage/sage_gpio.c` | SageLang |
| `src/sage_multicore.c` | `src/sage/sage_multicore.c` | SageLang |
| `src/sage_system.c` | `src/sage/sage_system.c` | SageLang |
| `src/sage_time.c` | `src/sage/sage_time.c` | SageLang |
| `src/sage_watchdog.c` | `src/sage/sage_watchdog.c` | SageLang |
| `src/shell/*` | `src/shell/*` | Shell (no change) |

### Header Files

| Current Location | New Location | Category |
|-----------------|--------------|----------|
| `include/dmesg.h` | `include/core/dmesg.h` | Core |
| `include/multicore.h` | `include/core/multicore.h` | Core |
| `include/supervisor.h` | `include/core/supervisor.h` | Core |
| `include/watchdog.h` | `include/core/watchdog.h` | Core |
| `include/hal/gpio.h` | `include/drivers/gpio.h` | Drivers |
| `include/system_info.h` | `include/drivers/system_info.h` | Drivers |
| `include/config_storage.h` | `include/fs/config_storage.h` | Storage |
| `include/script_storage.h` | `include/fs/script_storage.h` | Storage |
| `include/sage_embed.h` | `include/sage/sage_embed.h` | SageLang |
| `include/reg.h` | `include/reg.h` | Root (no change) |

## Implementation Steps

### Phase 1: Create New Directory Structure
1. Create `src/core/`, `src/drivers/`, `src/fs/`, `src/sage/`
2. Create `include/core/`, `include/drivers/`, `include/fs/`, `include/sage/`

### Phase 2: Move Files
1. Move source files to new locations (using git mv to preserve history)
2. Move header files to new locations

### Phase 3: Update References
1. Update all `#include` statements in source files
2. Update `CMakeLists.txt` with new paths
3. Update documentation references

### Phase 4: Clean Up
1. Remove empty `src/hal/` directory
2. Remove empty `include/hal/` directory
3. Update README.md with new structure

### Phase 5: Verification
1. Compile and test
2. Verify all includes resolve correctly
3. Run on hardware to confirm functionality

## Include Path Updates Needed

After reorganization, includes will change:

**Before:**
```c
#include "dmesg.h"
#include "supervisor.h"
#include "hal/gpio.h"
#include "sage_embed.h"
```

**After:**
```c
#include "core/dmesg.h"
#include "core/supervisor.h"
#include "drivers/gpio.h"
#include "sage/sage_embed.h"
```

## CMakeLists.txt Updates

**File paths will change from:**
```cmake
src/kernel.c
src/dmesg.c
src/hal/gpio.c
src/sage_embed.c
```

**To:**
```cmake
src/core/kernel.c
src/core/dmesg.c
src/drivers/gpio.c
src/sage/sage_embed.c
```

## Documentation Updates

The following docs will need path updates:
- README.md - Project structure section
- BOOT_SEQUENCE.md - File references
- GPIO_INTEGRATION.md - Include examples
- SAGELANG_INTEGRATION.md - File locations

## Notes

- Use `git mv` to preserve file history
- Test compilation after each phase
- Consider creating a migration branch first
- Update CHANGELOG.md with reorganization entry
