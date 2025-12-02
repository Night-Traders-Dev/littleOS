# littleOS for RP2040

A minimal, educational operating system for the Raspberry Pi Pico (RP2040) with an interactive shell interface and embedded SageLang scripting support.

## Features

- ✅ **Interactive Shell** - Command-line interface with command history and echo
- ✅ **SageLang Integration** - Full-featured scripting language embedded in the OS
- ✅ **Dual I/O Support** - Works with both USB serial and hardware UART
- ✅ **Minimal Design** - Lightweight kernel suitable for embedded learning
- ✅ **Built on Pico SDK** - Leverages official Raspberry Pi SDK for hardware abstraction

## SageLang Features

- **Interpreter Mode** - Run scripts directly on RP2040
- **Interactive REPL** - Live coding and testing
- **Object-Oriented** - Classes, inheritance, methods
- **Exception Handling** - try/catch/finally blocks
- **Generators** - yield-based lazy evaluation
- **Garbage Collection** - Automatic memory management
- **Memory Optimized** - 64KB heap for embedded use

## Quick Start

### 1. Clone and Setup

```bash
git clone https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS

# Add SageLang submodule
git submodule add https://github.com/Night-Traders-Dev/SageLang.git third_party/sagelang
git submodule update --init --recursive
```

### 2. Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. Flash to Pico

```bash
# Hold BOOTSEL button and connect USB
cp littleos.uf2 /media/$USER/RPI-RP2/
```

### 4. Connect and Use

```bash
screen /dev/ttyACM0 115200
```

```
Welcome to littleOS Shell!
> sage
SageLang REPL (embedded mode)

sage> print "Hello, World!"
Hello, World!
sage> let x = 42
sage> print x * 2
84
sage> exit
> 
```

## Hardware Requirements

- Raspberry Pi Pico or any RP2040-based board
- USB cable for programming and serial communication
- (Optional) UART-to-USB adapter for hardware UART communication

## Software Requirements

- CMake (3.13 or higher)
- ARM GCC toolchain (`arm-none-eabi-gcc`)
- Pico SDK (automatically configured via `PICO_SDK_PATH`)
- Python 3.x (for SDK tools)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 build-essential libstdc++-arm-none-eabi-newlib python3
```

**macOS:**
```bash
brew install cmake
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc
```

**Arch Linux:**
```bash
sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib python
```

## Building

### With SageLang (Recommended)

```bash
git clone https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS
git submodule update --init --recursive

export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Without SageLang

If you don't initialize the submodule, littleOS will build without SageLang support (basic shell only).

## SageLang Usage

### Shell Commands

```bash
# Start REPL
sage

# Execute inline code
sage -e "print 2 + 2"

# Check memory
sage -m

# Get help
sage --help
```

### Example Programs

**Functions:**
```sage
proc fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

print fibonacci(10)
```

**Classes:**
```sage
class Counter:
    proc init(self, start):
        self.value = start
    
    proc increment(self):
        self.value = self.value + 1

let c = Counter(0)
c.increment()
print c.value
```

**Exception Handling:**
```sage
try:
    let result = 10 / 0
catch e:
    print "Error: " + e
finally:
    print "Done"
```

**Generators:**
```sage
proc count_up(n):
    let i = 0
    while i < n:
        yield i
        i = i + 1

let gen = count_up(5)
print next(gen)  # 0
print next(gen)  # 1
```

### Full Documentation

- **Quick Start**: [SAGELANG_QUICKSTART.md](SAGELANG_QUICKSTART.md)
- **Integration Guide**: [docs/SAGELANG_INTEGRATION.md](docs/SAGELANG_INTEGRATION.md)
- **SageLang Documentation**: [third_party/sagelang/README.md](third_party/sagelang/README.md)
- **Examples**: [third_party/sagelang/examples/](third_party/sagelang/examples/)

## Flashing to Pico

### Method 1: UF2 Drag-and-Drop (Easiest)

1. Hold the BOOTSEL button while plugging in your Pico
2. The Pico will appear as a USB mass storage device
3. Copy `littleos.uf2` to the mounted drive
4. The Pico will automatically reboot and run littleOS

### Method 2: Using picotool

```bash
picotool load littleos.elf
picotool reboot
```

### Method 3: Using OpenOCD (Advanced)

```bash
openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg \
        -c "program littleos.elf verify reset exit"
```

## Connecting to the Shell

### Via USB Serial (Recommended)

The OS waits for a USB connection before starting. Connect using any serial terminal:

**Using screen:**
```bash
screen /dev/ttyACM0 115200
```

**Using minicom:**
```bash
minicom -D /dev/ttyACM0 -b 115200
```

**Using PuTTY (Windows):**
- Connection type: Serial
- Serial line: COM3 (or appropriate COM port)
- Speed: 115200

### Via Hardware UART

To use hardware UART instead of USB:

1. Comment out the USB wait loop in `boot/boot.c`
2. Rebuild the project
3. Connect to GPIO 0 (TX) and GPIO 1 (RX) at 115200 baud

## Shell Commands

- `help` - Display available commands
- `version` - Show OS version and platform info
- `reboot` - Reboot the system (placeholder)
- `sage` - SageLang interpreter (see `sage --help`)

### Example Session

```
Welcome to littleOS Shell!
Type 'help' for available commands
> help
Available commands:
  help     - Show this help message
  version  - Show OS version
  reboot   - Reboot the system
  sage     - SageLang interpreter (type 'sage --help')
> version
littleOS v0.1.0 - RP2040
With SageLang v0.8.0
> sage -e "print 2 + 2"
4
> sage
SageLang REPL (embedded mode)
Type 'exit' to quit

sage> let greeting = "Hello from SageLang!"
sage> print greeting
Hello from SageLang!
sage> exit
> 
```

## Project Structure

```
littleOS/
├── boot/
│   └── boot.c                    # System entry point
├── src/
│   ├── kernel.c                  # Core kernel logic
│   ├── uart.c                    # UART implementation
│   ├── sage_embed.c              # SageLang embedding layer
│   └── shell/
│       ├── shell.c               # Interactive shell
│       └── cmd_sage.c            # Sage command handler
├── include/
│   ├── reg.h                     # Hardware registers
│   └── sage_embed.h              # SageLang API
├── third_party/
│   └── sagelang/                 # SageLang submodule
├── docs/
│   └── SAGELANG_INTEGRATION.md   # Integration guide
├── CMakeLists.txt                # Build configuration
├── SAGELANG_QUICKSTART.md        # Quick start guide
└── README.md                     # This file
```

## Development

### Adding New Shell Commands

Edit `src/shell/shell.c` and add new command handlers in the `shell_run()` function.

### Updating SageLang

```bash
cd third_party/sagelang
git pull origin main
cd ../..
git add third_party/sagelang
git commit -m "Update SageLang to latest version"
```

### Debugging

The build includes debug symbols (`-g` flag). Use:

- **GDB with OpenOCD** for step debugging
- **printf debugging** via USB/UART serial
- **picotool info** to inspect the binary

### Clean Build

```bash
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Roadmap

### Current Version: v0.2.0
- [x] Basic kernel initialization
- [x] Interactive shell with command parsing
- [x] USB and UART stdio support
- [x] Core commands (help, version)
- [x] **SageLang Integration** - Full interpreter embedded
- [x] **REPL Support** - Interactive programming
- [x] **Memory Management** - GC with 64KB heap

### Upcoming: v0.3.0
- [ ] File system abstraction (LittleFS)
- [ ] Script storage on flash
- [ ] Autorun scripts on boot
- [ ] Module system for SageLang

### Future Enhancements
- [ ] Hardware access from SageLang (GPIO, SPI, I2C)
- [ ] Multi-core support (Core 1 utilization)
- [ ] Network stack (if using Pico W)
- [ ] Graphics support (for displays)
- [ ] Power management
- [ ] SageLang compiler (PC → RP2040 cross-compilation)

## Platform Features

### Embedded Mode (RP2040)
- ✅ Interpreter-only execution
- ✅ Interactive REPL
- ✅ 64KB heap limit
- ✅ Inline code evaluation
- ⏳ File I/O (planned with LittleFS)
- ⏳ Persistent storage

### PC Mode (Testing)
- ✅ Full interpreter
- ✅ Unlimited heap
- ✅ File I/O support
- ✅ Script execution from files
- ⏳ Compiler (planned)

## Memory Constraints

**RP2040 RAM Layout:**
- Total RAM: 264 KB
- SageLang Heap: 64 KB (configurable)
- littleOS + Stack: ~200 KB

**Tips for Embedded:**
- Keep scripts small and modular
- Use `gc_collect()` in long-running loops
- Monitor memory with `sage -m`
- Avoid deep recursion

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Contributing to SageLang

For language features, contribute to [Night-Traders-Dev/SageLang](https://github.com/Night-Traders-Dev/SageLang).

## License

This project is open source under the MIT License.

## Resources

- [Raspberry Pi Pico Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)
- [SageLang Documentation](third_party/sagelang/README.md)

## Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check existing issues and discussions
- Refer to the Pico SDK documentation for hardware-specific questions

---

**Built with ❤️ for embedded systems education and experimentation**

**Powered by SageLang - A clean, indentation-based systems programming language**
