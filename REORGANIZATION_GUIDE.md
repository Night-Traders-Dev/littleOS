# littleOS Directory Reorganization - Implementation Guide

## Overview

This guide provides step-by-step instructions to reorganize the littleOS directory structure for better maintainability and clarity.

**IMPORTANT**: This reorganization should be done locally to preserve git history properly using `git mv` commands.

## Quick Summary

- **Goal**: Organize files by functional area (core, drivers, fs, sage)
- **Method**: Use `git mv` to preserve file history
- **Impact**: All `#include` paths and CMakeLists.txt need updates
- **Time**: ~30-45 minutes

## Prerequisites

```bash
# Ensure you're on main branch and up to date
git checkout main
git pull origin main

# Create a new branch for reorganization
git checkout -b feature/directory-reorganization

# Ensure working directory is clean
git status
```

## Phase 1: Create New Directory Structure

```bash
# Navigate to repository root
cd /path/to/littleOS

# Create new source directories
mkdir -p src/core
mkdir -p src/drivers
mkdir -p src/fs
mkdir -p src/sage

# Create new include directories
mkdir -p include/core
mkdir -p include/drivers
mkdir -p include/fs
mkdir -p include/sage
```

## Phase 2: Move Source Files

### Core Kernel Files

```bash
# Move core kernel components
git mv src/kernel.c src/core/
git mv src/dmesg.c src/core/
git mv src/supervisor.c src/core/
git mv src/watchdog.c src/core/
git mv src/multicore.c src/core/
```

### Driver Files

```bash
# Move HAL/driver files
git mv src/hal/gpio.c src/drivers/
git mv src/uart.c src/drivers/
git mv src/system_info.c src/drivers/
```

### Storage/Filesystem Files

```bash
# Move storage subsystem files
git mv src/config_storage.c src/fs/
git mv src/script_storage.c src/fs/
```

### SageLang Integration Files

```bash
# Move all SageLang binding files
git mv src/sage_embed.c src/sage/
git mv src/sage_config.c src/sage/
git mv src/sage_gpio.c src/sage/
git mv src/sage_multicore.c src/sage/
git mv src/sage_system.c src/sage/
git mv src/sage_time.c src/sage/
git mv src/sage_watchdog.c src/sage/
```

### Shell Files (Already Organized)

```bash
# Shell directory already well-organized, no changes needed
# src/shell/ stays as-is
```

## Phase 3: Move Header Files

### Core Headers

```bash
# Move core kernel headers
git mv include/dmesg.h include/core/
git mv include/multicore.h include/core/
git mv include/supervisor.h include/core/
git mv include/watchdog.h include/core/
```

### Driver Headers

```bash
# Move driver headers
git mv include/hal/gpio.h include/drivers/
git mv include/system_info.h include/drivers/
```

### Storage Headers

```bash
# Move storage headers
git mv include/config_storage.h include/fs/
git mv include/script_storage.h include/fs/
```

### SageLang Headers

```bash
# Move SageLang header
git mv include/sage_embed.h include/sage/
```

## Phase 4: Clean Up Empty Directories

```bash
# Remove now-empty hal directories
rm -rf src/hal
rm -rf include/hal

# Remove main.c if it exists in src (should be in boot/)
if [ -f src/main.c ]; then
    echo "Note: src/main.c found - review if needed"
fi
```

## Phase 5: Update CMakeLists.txt

Edit `CMakeLists.txt` and update the file paths:

```cmake
# Find the littleos_core library definition and update paths:

add_library(littleos_core STATIC
    # Core kernel files
    src/core/kernel.c
    src/core/dmesg.c
    src/core/supervisor.c
    src/core/watchdog.c
    src/core/multicore.c
    
    # Driver files
    src/drivers/gpio.c
    src/drivers/uart.c
    src/drivers/system_info.c
    
    # Storage files
    src/fs/config_storage.c
    src/fs/script_storage.c
    
    # Shell files (paths unchanged)
    src/shell/shell.c
    src/shell/cmd_supervisor.c
    src/shell/cmd_dmesg.c
)

# Update SageLang sources in the conditional block:

if(SAGELANG_AVAILABLE)
    target_sources(littleos_core PRIVATE
        # SageLang integration files
        src/sage/sage_embed.c
        src/sage/sage_config.c
        src/sage/sage_gpio.c
        src/sage/sage_multicore.c
        src/sage/sage_system.c
        src/sage/sage_time.c
        src/sage/sage_watchdog.c
        
        # SageLang shell commands
        src/shell/cmd_sage.c
        src/shell/cmd_script.c
    )
    # ... rest unchanged
endif()
```

## Phase 6: Update Include Statements in Source Files

Update all `#include` directives to reflect new paths:

### Pattern to Find and Replace

**In ALL `.c` files under `src/`:**

```bash
# Use find and sed to update includes (or do manually in editor)

# Update core includes
find src -name "*.c" -exec sed -i 's|#include "dmesg.h"|#include "core/dmesg.h"|g' {} +
find src -name "*.c" -exec sed -i 's|#include "multicore.h"|#include "core/multicore.h"|g' {} +
find src -name "*.c" -exec sed -i 's|#include "supervisor.h"|#include "core/supervisor.h"|g' {} +
find src -name "*.c" -exec sed -i 's|#include "watchdog.h"|#include "core/watchdog.h"|g' {} +

# Update driver includes
find src -name "*.c" -exec sed -i 's|#include "hal/gpio.h"|#include "drivers/gpio.h"|g' {} +
find src -name "*.c" -exec sed -i 's|#include "system_info.h"|#include "drivers/system_info.h"|g' {} +

# Update storage includes
find src -name "*.c" -exec sed -i 's|#include "config_storage.h"|#include "fs/config_storage.h"|g' {} +
find src -name "*.c" -exec sed -i 's|#include "script_storage.h"|#include "fs/script_storage.h"|g' {} +

# Update SageLang includes
find src -name "*.c" -exec sed -i 's|#include "sage_embed.h"|#include "sage/sage_embed.h"|g' {} +
```

**Note**: On macOS, use `sed -i ''` instead of `sed -i`

### Manual Include Updates

Alternatively, search and replace in your editor:

| Old Include | New Include |
|------------|-------------|
| `#include "dmesg.h"` | `#include "core/dmesg.h"` |
| `#include "multicore.h"` | `#include "core/multicore.h"` |
| `#include "supervisor.h"` | `#include "core/supervisor.h"` |
| `#include "watchdog.h"` | `#include "core/watchdog.h"` |
| `#include "hal/gpio.h"` | `#include "drivers/gpio.h"` |
| `#include "system_info.h"` | `#include "drivers/system_info.h"` |
| `#include "config_storage.h"` | `#include "fs/config_storage.h"` |
| `#include "script_storage.h"` | `#include "fs/script_storage.h"` |
| `#include "sage_embed.h"` | `#include "sage/sage_embed.h"` |

### Files That Need Include Updates

Check these files specifically:

- `boot/boot.c`
- `src/core/kernel.c`
- `src/core/supervisor.c`
- `src/core/watchdog.c`
- `src/core/multicore.c`
- `src/shell/shell.c`
- `src/shell/cmd_*.c`
- All `src/sage/*.c` files
- All `src/drivers/*.c` files
- All `src/fs/*.c` files

## Phase 7: Commit Changes

```bash
# Stage all changes
git add -A

# Review changes
git status
git diff --cached

# Commit
git commit -m "Reorganize directory structure for better maintainability

- Group core kernel files in src/core/
- Group drivers in src/drivers/
- Group storage in src/fs/
- Group SageLang bindings in src/sage/
- Mirror organization in include/
- Update all include paths
- Update CMakeLists.txt with new paths
- Remove empty hal/ directories

This improves code organization and makes the codebase easier
to navigate and maintain."
```

## Phase 8: Build and Test

```bash
# Clean build directory
rm -rf build
mkdir build
cd build

# Configure
cmake ..

# Build
make -j$(nproc)

# Check for compilation errors
# If successful, you should see littleos.uf2 created
ls -lh littleos.uf2
```

### Expected Output

Successful build should show:
```
=== Build Configuration ===
Board: pico
Platform: rp2040
SageLang: TRUE
Supervisor: Core 1 health monitoring
Kernel Messages: dmesg ring buffer
===========================

[100%] Built target littleos
```

## Phase 9: Flash and Hardware Test

```bash
# Flash to Pico
cp littleos.uf2 /media/$USER/RPI-RP2/

# Connect to serial
screen /dev/ttyACM0 115200
```

### Verification Checklist

- [ ] System boots successfully
- [ ] Shell prompt appears
- [ ] `help` command works
- [ ] `version` command works
- [ ] `stats` command shows system health
- [ ] `dmesg` command shows boot messages
- [ ] `sage` REPL works
- [ ] Watchdog is active
- [ ] Supervisor is running on Core 1
- [ ] No compilation warnings

## Phase 10: Push to GitHub

```bash
# Push branch
git push origin feature/directory-reorganization

# Create pull request on GitHub
# After review and approval, merge to main
```

## Rollback Procedure

If issues occur:

```bash
# Return to main branch
git checkout main

# Delete reorganization branch
git branch -D feature/directory-reorganization
```

## Common Issues and Solutions

### Issue: Compilation errors about missing headers

**Solution**: Check include paths in CMakeLists.txt and source files

```bash
# Verify include directories are correct
grep -r "#include" src/ | grep -v "pico/" | grep -v "hardware/"
```

### Issue: CMake can't find source files

**Solution**: Verify paths in CMakeLists.txt match actual file locations

```bash
# List all .c files in new structure
find src -name "*.c" | sort
```

### Issue: Linker errors

**Solution**: Ensure all source files are listed in CMakeLists.txt

## Post-Reorganization Updates

### Update README.md

Update the "Project Structure" section:

```markdown
## Project Structure

```
littleOS/
├── boot/                  # System boot code
├── src/
│   ├── core/             # Core kernel (kernel, dmesg, supervisor, watchdog)
│   ├── drivers/          # Hardware drivers (GPIO, UART, system info)
│   ├── fs/               # Storage (config, scripts)
│   ├── sage/             # SageLang integration
│   └── shell/            # Shell and commands
├── include/
│   ├── core/             # Core headers
│   ├── drivers/          # Driver headers
│   ├── fs/               # Storage headers
│   └── sage/             # SageLang headers
└── docs/                  # Documentation
```
```

### Update CHANGELOG.md

Add entry:

```markdown
## [Unreleased]

### Changed
- **Major directory reorganization** for better code organization
  - Grouped core kernel files in `src/core/`
  - Grouped hardware drivers in `src/drivers/`
  - Grouped storage subsystems in `src/fs/`
  - Grouped SageLang bindings in `src/sage/`
  - Mirrored organization in `include/` directory
  - Updated all include paths throughout codebase
  - No functional changes, improved maintainability
```

## Estimated Time

- **Phase 1-3** (File moves): 5 minutes
- **Phase 4-5** (CMakeLists): 10 minutes
- **Phase 6** (Include updates): 15 minutes
- **Phase 7-8** (Build & test): 10 minutes
- **Total**: ~40 minutes

## Notes

- This reorganization preserves all git history through `git mv`
- No functional code changes - only file locations
- All external APIs remain unchanged
- Documentation paths may need updates
- Consider this for next minor version (v0.4.0 → v0.5.0)

## Success Criteria

✅ All files successfully moved  
✅ CMakeLists.txt updated  
✅ All includes updated  
✅ Clean compilation with no warnings  
✅ Successful hardware test  
✅ All shell commands functional  
✅ Git history preserved  

## See Also

- [DIRECTORY_STRUCTURE.md](docs/DIRECTORY_STRUCTURE.md) - Detailed structure documentation
- [UPDATES.md](UPDATES.md) - Recent changes log
