# SageLang Integration - Quick Start

## Setup

### 1. Add SageLang Submodule

```bash
cd littleOS
git submodule add https://github.com/Night-Traders-Dev/SageLang.git third_party/sagelang
git submodule update --init --recursive
```

### 2. Build littleOS with SageLang

**For RP2040:**
```bash
mkdir build
cd build
export PICO_SDK_PATH=/path/to/pico-sdk
cmake ..
make -j$(nproc)
```

**For PC Testing:**
```bash
mkdir build-pc
cd build-pc
cmake -DPICO_PLATFORM=host ..
make -j$(nproc)
./littleos
```

### 3. Flash to RP2040

```bash
# Put Pico into BOOTSEL mode (hold BOOTSEL, connect USB)
cp littleos.uf2 /media/$USER/RPI-RP2/
```

## Using SageLang

### Start Interactive REPL

```
littleOS> sage
SageLang REPL (embedded mode)
Type 'exit' to quit

sage> print "Hello, World!"
Hello, World!
sage> let x = 42
sage> print x * 2
84
sage> exit
littleOS>
```

### Run Inline Code

```
littleOS> sage -e "print 2 + 2"
4
```

### Check Memory Usage

```
littleOS> sage -m
Memory: 1024 bytes, 12 objects
```

### Get Help

```
littleOS> sage --help
Usage: sage [options] [script]

Options:
  (no args)        Start interactive REPL
  -e, --eval CODE  Evaluate inline code
  -m, --mem        Show memory statistics
  -h, --help       Show this help

Embedded mode (interpreter only)
```

## Example Programs

### Hello World

```sage
print "Hello from SageLang on littleOS!"
```

### Functions

```sage
proc fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

print fibonacci(10)
```

### Classes

```sage
class LED:
    proc init(self, pin):
        self.pin = pin
        self.state = false
    
    proc toggle(self):
        self.state = not self.state
        print "LED on pin " + str(self.pin) + ": " + str(self.state)

let led = LED(25)
led.toggle()
led.toggle()
```

### Exception Handling

```sage
proc safe_divide(a, b):
    try:
        if b == 0:
            raise "Division by zero!"
        return a / b
    catch e:
        print "Error: " + e
        return 0

print safe_divide(10, 2)  # 5
print safe_divide(10, 0)  # Error message, returns 0
```

### Generators

```sage
proc sensor_readings(count):
    let i = 0
    while i < count:
        # Simulate sensor reading
        yield i * 10
        i = i + 1

let readings = sensor_readings(5)
print next(readings)  # 0
print next(readings)  # 10
print next(readings)  # 20
```

## Memory Constraints (RP2040)

- **Total RAM**: 264 KB
- **SageLang Heap**: 64 KB (default)
- **Remaining**: ~200 KB for OS

### Tips for Embedded Development

1. **Keep scripts small** - Break large programs into smaller functions
2. **Manual GC** - Call `gc_collect()` periodically in loops
3. **Avoid deep recursion** - Use iterative approaches when possible
4. **Monitor memory** - Use `sage -m` to check usage
5. **Clean up** - Exit REPL to free all memory

## Platform Detection

SageLang automatically detects the platform:

- **Embedded (RP2040)**: Memory limits, no file I/O
- **PC**: Unlimited memory, full file I/O

Code runs identically on both platforms within these constraints.

## Next Steps

- 📚 Read full documentation: `docs/SAGELANG_INTEGRATION.md`
- 📝 Explore examples: `third_party/sagelang/examples/`
- 🚀 Check SageLang features: `third_party/sagelang/README.md`
- 🐛 Report issues: [GitHub Issues](https://github.com/Night-Traders-Dev/littleOS/issues)

## Troubleshooting

### "Unknown command: sage"

**Cause**: SageLang not compiled into build

**Fix**: Ensure submodule exists and rebuild:
```bash
git submodule update --init --recursive
rm -rf build
mkdir build && cd build
cmake .. && make
```

### "Memory allocation failed"

**Cause**: Exceeded 64KB heap limit

**Fix**: Use less memory or simplify your code

### Build errors

**Cause**: Missing PICO_SDK_PATH or submodule

**Fix**:
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
git submodule update --init --recursive
```

## License

MIT License - See LICENSE file in both repositories
