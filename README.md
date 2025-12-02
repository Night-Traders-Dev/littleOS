# littleOS for RP2040

A minimal, educational operating system for the Raspberry Pi Pico (RP2040) with an interactive shell interface.

## Features

- ✅ **Interactive Shell** - Command-line interface with command history and echo
- ✅ **Dual I/O Support** - Works with both USB serial and hardware UART
- ✅ **Minimal Design** - Lightweight kernel suitable for embedded learning
- ✅ **Built on Pico SDK** - Leverages official Raspberry Pi SDK for hardware abstraction
- 🚧 **SageLang Integration** (Coming Soon) - Embedded scripting language support

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

### 1. Clone the Repository

```bash
git clone https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS
```

### 2. Set Up Pico SDK

Make sure `PICO_SDK_PATH` is set to your Pico SDK installation:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

Or add it to your `~/.bashrc` or `~/.zshrc` for persistence.

### 3. Configure and Build

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

This will generate:
- `littleos.elf` - ELF binary
- `littleos.bin` - Raw binary
- `littleos.uf2` - UF2 format for drag-and-drop flashing
- `littleos.hex` - HEX format
- `littleos.map` - Linker map file

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

1. Comment out the USB wait loop in `boot/boot.c`:
   ```c
   // while (!stdio_usb_connected()) {
   //     sleep_ms(100);
   // }
   ```

2. Rebuild the project

3. Connect to GPIO 0 (TX) and GPIO 1 (RX) at 115200 baud

## Using the Shell

Once connected, you'll see:

```
RP2040 tiny OS kernel
> 
Welcome to littleOS Shell!
> 
```

### Available Commands

- `help` - Display available commands
- `version` - Show OS version and platform info
- `reboot` - Reboot the system (placeholder)

### Example Session

```
> help
Available commands: help, version, reboot
> version
littleOS v0.1.0 - RP2040
> 
```

## Project Structure

```
littleOS/
├── boot/
│   └── boot.c              # System entry point and initialization
├── src/
│   ├── kernel.c            # Core kernel logic
│   ├── uart.c              # Custom UART implementation (legacy)
│   └── shell/
│       └── shell.c         # Interactive shell implementation
├── include/
│   └── reg.h               # Hardware register definitions
├── CMakeLists.txt          # Build configuration
├── pico_sdk_import.cmake   # Pico SDK integration
└── README.md               # This file
```

## Development

### Adding New Shell Commands

Edit `src/shell/shell.c` and add new command handlers in the `shell_run()` function:

```c
if (strcmp(buffer, "mycommand") == 0) {
    printf("My command output\r\n");
}
```

### Debugging

The build includes debug symbols (`-g` flag). Use:

- **GDB with OpenOCD** for step debugging
- **printf debugging** via USB/UART serial
- **picotool info** to inspect the binary

### Clean Build

```bash
cd build
rm -rf *
cmake ..
make -j$(nproc)
```

## Roadmap

### Current Version: v0.1.0
- [x] Basic kernel initialization
- [x] Interactive shell with command parsing
- [x] USB and UART stdio support
- [x] Core commands (help, version)

### Upcoming: v0.2.0
- [ ] **SageLang Integration** - Embed the SageLang interpreter
- [ ] File system abstraction (LittleFS)
- [ ] Basic process/task management
- [ ] Memory management utilities
- [ ] Watchdog and system monitoring

### Future Enhancements
- [ ] Multi-core support (Core 1 utilization)
- [ ] Network stack (if using Pico W)
- [ ] Graphics support (for displays)
- [ ] Interrupt-driven I/O
- [ ] Power management

## SageLang Integration Plan

littleOS will integrate [SageLang](https://github.com/Night-Traders-Dev/SageLang), a lightweight scripting language designed for embedded systems. This will enable:

- **Dynamic Scripting** - Run SageLang scripts from the shell
- **System Control** - Script-based GPIO, UART, and peripheral control
- **Extensibility** - User-defined commands and automation
- **Educational** - Learn embedded programming with high-level scripting

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is open source. Please check the repository for license details.

## Resources

- [Raspberry Pi Pico Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang Repository](https://github.com/Night-Traders-Dev/SageLang)

## Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check existing issues and discussions
- Refer to the Pico SDK documentation for hardware-specific questions

---

**Built with ❤️ for embedded systems education and experimentation**
