# Contributing to littleOS

Thank you for your interest in contributing to littleOS! This document provides guidelines for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)
- [Testing](#testing)
- [Documentation](#documentation)

## Code of Conduct

This project follows a simple principle: **Be respectful and constructive**.

- Be welcoming to newcomers
- Respect differing viewpoints and experiences
- Accept constructive criticism gracefully
- Focus on what's best for the community and project

## Getting Started

### Prerequisites

1. **Development Environment**:
   - ARM GCC toolchain (`arm-none-eabi-gcc`)
   - CMake 3.13+
   - Pico SDK
   - Git

2. **Hardware** (for testing):
   - Raspberry Pi Pico or RP2040-based board
   - USB cable

3. **Recommended Tools**:
   - VS Code with C/C++ extensions
   - OpenOCD or picotool for debugging
   - Serial terminal (minicom, screen, PuTTY)

### Initial Setup

```bash
# Clone the repository
git clone https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS

# Set Pico SDK path
export PICO_SDK_PATH=/path/to/pico-sdk

# Build the project
mkdir -p build
cd build
cmake ..
make -j$(nproc)

# Flash to Pico (drag-and-drop)
cp littleos.uf2 /path/to/pico/mount
```

## Development Workflow

### Branch Strategy

- `main` - Stable releases only
- `develop` - Integration branch for features
- `feature/*` - New features (e.g., `feature/sagelang-integration`)
- `fix/*` - Bug fixes (e.g., `fix/boot-issues`)
- `docs/*` - Documentation updates

### Creating a Feature Branch

```bash
# Update your local repository
git checkout develop
git pull origin develop

# Create feature branch
git checkout -b feature/my-new-feature

# Make changes, commit, push
git add .
git commit -m "feat: add new feature"
git push origin feature/my-new-feature
```

### Working on an Issue

1. **Check existing issues** - Search for duplicates first
2. **Comment on the issue** - Let others know you're working on it
3. **Create a branch** - Reference the issue number
4. **Make changes** - Follow coding standards
5. **Test thoroughly** - On actual hardware if possible
6. **Submit PR** - Link to the original issue

## Coding Standards

### C Code Style

**Naming Conventions**:
```c
// Functions: snake_case
void initialize_uart(void);
void gpio_set_pin(uint8_t pin, bool value);

// Variables: snake_case
int counter = 0;
char buffer[64];

// Constants: UPPER_SNAKE_CASE
#define MAX_BUFFER_SIZE 256
#define UART_BAUD_RATE 115200

// Types: PascalCase with _t suffix
typedef struct {
    char name[32];
    uint32_t size;
} ScriptEntry_t;

// Enums: UPPER_SNAKE_CASE
typedef enum {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_ERROR
} SystemState_t;
```

**Formatting**:
```c
// Indentation: 4 spaces (no tabs)
// Braces: K&R style
if (condition) {
    do_something();
} else {
    do_something_else();
}

// Function definitions
void my_function(int param1, char *param2)
{
    // Function body
    return;
}

// Pointer declarations
char *ptr;      // NOT: char* ptr
int *array[10]; // NOT: int* array[10]
```

**Comments**:
```c
// Single-line comments for brief explanations
int count = 0;  // Inline comments when helpful

/**
 * Multi-line comments for function documentation
 * 
 * @param pin GPIO pin number
 * @param value Pin state (true = high, false = low)
 * @return 0 on success, -1 on error
 */
int gpio_set_pin(uint8_t pin, bool value)
{
    // Implementation
}
```

### Header Files

```c
#ifndef COMPONENT_H
#define COMPONENT_H

// Include guards always
// Use descriptive macro names

#include <stdint.h>
#include <stdbool.h>

// Public API declarations
void component_init(void);
int component_process(void);

#endif // COMPONENT_H
```

### Error Handling

```c
// Return error codes
#define SUCCESS 0
#define ERROR_INVALID_PARAM -1
#define ERROR_TIMEOUT -2

// Check return values
int result = do_something();
if (result != SUCCESS) {
    handle_error(result);
    return result;
}

// Use assertions for sanity checks (debug builds)
assert(ptr != NULL);
assert(size > 0);
```

### Memory Management

```c
// Always check allocation results
char *buffer = malloc(size);
if (buffer == NULL) {
    return ERROR_OUT_OF_MEMORY;
}

// Free memory when done
free(buffer);
buffer = NULL;  // Prevent use-after-free

// Prefer stack allocation when possible
char local_buffer[64];  // Better for embedded
```

## Commit Guidelines

### Commit Message Format

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**:
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation only
- `style` - Code style changes (formatting, no logic change)
- `refactor` - Code refactoring
- `perf` - Performance improvement
- `test` - Adding or updating tests
- `chore` - Build process, dependencies, etc.

**Examples**:
```
feat(shell): add history navigation with arrow keys

fix(boot): resolve UART initialization conflict with SDK

docs: update README with build instructions

refactor(kernel): simplify main loop logic

perf(gc): optimize mark phase for embedded systems
```

### Commit Best Practices

- **One logical change per commit**
- **Write descriptive messages** - Explain why, not just what
- **Keep commits small** - Easier to review and revert
- **Test before committing** - Ensure code builds and runs

## Pull Request Process

### Before Submitting

1. **Update from develop**:
   ```bash
   git checkout develop
   git pull origin develop
   git checkout feature/my-feature
   git rebase develop
   ```

2. **Build and test**:
   ```bash
   cd build
   rm -rf *
   cmake ..
   make -j$(nproc)
   # Flash and test on hardware
   ```

3. **Review your changes**:
   ```bash
   git diff develop
   git log develop..HEAD
   ```

### PR Checklist

- [ ] Code builds without warnings
- [ ] Tested on actual hardware
- [ ] No merge conflicts with develop
- [ ] Commit messages follow guidelines
- [ ] Code follows style guidelines
- [ ] Documentation updated (if needed)
- [ ] Added comments for complex logic
- [ ] Removed debug/temporary code

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Tested on Raspberry Pi Pico
- [ ] Shell commands work correctly
- [ ] No memory leaks detected
- [ ] Runs stable for 10+ minutes

## Related Issues
Fixes #123
Closes #456

## Screenshots (if applicable)
[Terminal output or oscilloscope captures]

## Additional Notes
Any special considerations or follow-up needed
```

### Review Process

1. **Automated checks** - CI/CD builds and tests
2. **Code review** - Maintainer reviews code quality
3. **Testing** - Verify on hardware if possible
4. **Discussion** - Address feedback and questions
5. **Approval** - Maintainer approves and merges

## Testing

### Manual Testing

**Basic Smoke Test**:
1. Flash firmware to Pico
2. Connect via USB serial
3. Verify boot message appears
4. Test each shell command
5. Check for memory leaks (run for 5+ minutes)

**Hardware Testing**:
- GPIO toggling (oscilloscope if available)
- UART communication
- LED blinking patterns
- Button input (if applicable)

### Automated Testing

```bash
# Unit tests (when available)
make test

# Integration tests on hardware
./scripts/run_hardware_tests.sh
```

### Performance Testing

- Monitor memory usage
- Check response times
- Measure power consumption (for battery devices)
- Test under load conditions

## Documentation

### Code Documentation

- **Document public APIs** - Every public function should have a comment
- **Explain complex logic** - Don't just restate what the code does
- **Use examples** - Show how to use new features
- **Keep it updated** - Update docs when changing code

### User Documentation

- **README.md** - Getting started, basic usage
- **docs/** - Detailed guides and references
- **Examples** - Working code samples
- **CHANGELOG.md** - Track changes between versions

### Documentation Style

- Use clear, simple language
- Include code examples
- Add diagrams for complex systems
- Link to related resources

## Areas Needing Contribution

### High Priority

- [ ] SageLang integration (see `docs/SAGELANG_INTEGRATION.md`)
- [ ] File system support (LittleFS)
- [ ] More shell commands
- [ ] Hardware abstraction layer improvements

### Medium Priority

- [ ] Multi-core support (Core 1 utilization)
- [ ] Power management features
- [ ] Network stack (for Pico W)
- [ ] Unit test framework

### Low Priority (Nice to Have)

- [ ] Graphics support for displays
- [ ] Audio output capabilities
- [ ] USB HID device support
- [ ] CI/CD pipeline setup

## Questions?

- **Issues**: [GitHub Issues](https://github.com/Night-Traders-Dev/littleOS/issues)
- **Discussions**: [GitHub Discussions](https://github.com/Night-Traders-Dev/littleOS/discussions)
- **Documentation**: Check `docs/` directory

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.

---

**Thank you for contributing to littleOS! 🚀**
