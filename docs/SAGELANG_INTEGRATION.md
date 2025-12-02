# SageLang Integration Plan for littleOS

This document outlines the plan for integrating the [SageLang](https://github.com/Night-Traders-Dev/SageLang) interpreter into littleOS for the RP2040.

## Overview

SageLang is a clean, indentation-based systems programming language with features including:
- Object-Oriented Programming (classes, inheritance)
- Exception handling (try/catch/finally/raise)
- Generators and lazy evaluation (yield/next)
- Garbage collection
- Rich data structures (arrays, dictionaries, tuples)
- 30+ native functions

Integrating SageLang into littleOS will enable:
- **Interactive scripting** from the shell ✅
- **Dynamic GPIO control** via scripts
- **System automation** without recompiling ✅
- **Educational platform** for learning embedded programming ✅

## Integration Status: Phase 3 Complete! 🎉

**Current Version**: littleOS v0.2.0 with SageLang v0.8.0

### ✅ Completed Phases
- **Phase 1**: Preparation and Analysis
- **Phase 2**: Minimal Interpreter Port
- **Phase 3**: Shell Integration

### 🚧 In Progress
- **Phase 4**: Hardware Bindings

### 📋 Upcoming
- **Phase 5**: Script Storage and Execution
- **Phase 6**: Advanced Features
- **Phase 7**: Optimization and Polish

---

## Integration Phases

### Phase 1: Preparation and Analysis ✅
**Goal**: Understand constraints and prepare the codebase

#### Tasks
- [x] Fix littleOS boot issues and stabilize shell
- [x] Document current littleOS architecture
- [x] Analyze SageLang memory requirements
- [x] Identify RP2040 memory constraints (264KB RAM, 2MB Flash)
- [x] Create integration architecture

#### Deliverables
- ✅ Stable littleOS v0.1.0 with working shell
- ✅ Memory profiling baseline
- ✅ Integration architecture document

---

### Phase 2: Minimal Interpreter Port ✅
**Goal**: Get a basic SageLang interpreter running on RP2040

#### Implementation Details

**Submodule Integration:**
```bash
git submodule add https://github.com/Night-Traders-Dev/SageLang.git third_party/sagelang
```

**CMake Build System:**
- Created `sagelang` static library target
- Linked against `littleos_core`
- Proper include directories configured
- Conditional compilation with `PICO_BUILD` macro

**Embedding Layer (`src/sage_embed.c`):**
```c
// API wrapper functions
sage_context_t* sage_init(void);
void sage_cleanup(sage_context_t* ctx);
sage_result_t sage_eval_string(sage_context_t* ctx, const char* source, size_t len);
sage_result_t sage_repl(sage_context_t* ctx);
```

**Memory Configuration:**
```c
#ifdef PICO_BUILD
    // RP2040: 64KB heap for SageLang
    printf("SageLang: Embedded mode (64KB heap)\r\n");
#else
    // PC: Unlimited heap for testing
    printf("SageLang: PC mode (unlimited heap)\n");
#endif
```

#### SageLang API Used
- `init_lexer(source)` - Initialize lexer
- `parser_init()` - Initialize parser
- `parse()` - Parse one statement at a time
- `interpret(stmt, env)` - Execute statement
- `env_create(parent)` - Create environment
- `init_stdlib(env)` - Register standard library
- `gc_init()` - Initialize garbage collector
- `gc_collect()` - Run garbage collection

#### Test Cases Passed
- ✅ Basic arithmetic: `2 + 2`, `10 * 5`
- ✅ Variable assignment: `let x = 42`
- ✅ Simple print: `print "Hello from Sage!"`
- ✅ Function definition and call
- ✅ Classes and objects
- ✅ Arrays, dictionaries, tuples
- ✅ Exception handling

#### Deliverables
- ✅ Working SageLang interpreter on RP2040
- ✅ Submodule-based integration
- ✅ Embedding API layer
- ✅ Memory usage benchmarks

**Build Output:**
```
-- === Build Configuration ===
-- Board: pico
-- Platform: rp2040
-- SageLang: TRUE
-- ===========================
```

---

### Phase 3: Shell Integration ✅
**Goal**: Execute Sage code from the littleOS shell

#### Implementation

**Shell Command Handler (`src/shell/cmd_sage.c`):**
```c
void cmd_sage(const char* args) {
    if (args && strlen(args) > 0) {
        if (strcmp(args, "--help") == 0) {
            // Show help
        } else if (strcmp(args, "-m") == 0) {
            // Show memory stats
        } else if (strncmp(args, "-e ", 3) == 0) {
            // Execute inline code
        } else {
            // Unknown option
        }
    } else {
        // Enter REPL
        sage_repl(sage_ctx);
    }
}
```

**REPL Features:**
- Interactive prompt: `sage>`
- Line-by-line execution
- Error handling and reporting
- Type `exit` to quit
- Persistent environment across statements

**Shell Commands Added:**
- `sage` - Enter REPL mode ✅
- `sage -e "code"` - Execute inline code ✅
- `sage -m` - Show memory statistics ✅
- `sage --help` - Display help ✅

#### Example Session
```
Welcome to littleOS Shell!
Type 'help' for available commands
> sage

SageLang REPL (embedded mode)
Type 'exit' to quit

sage> let x = 10
sage> let y = 20
sage> print x + y
30
sage> exit
> sage -e "print 2 + 2"
4
> sage -m
SageLang Memory:
  Allocated: 1024 bytes
  Objects: 12
> 
```

#### Error Handling
```sage
sage> let x = 10 / 0
Runtime error: Division by zero
sage> print undefined_var
Runtime error: Undefined variable 'undefined_var'
```

#### Deliverables
- ✅ Shell command integration
- ✅ REPL with persistent environment
- ✅ Inline code execution (`-e` flag)
- ✅ Memory inspection (`-m` flag)
- ✅ Error reporting system
- ✅ Help documentation (`--help`)

---

### Phase 4: Hardware Bindings 🚧
**Goal**: Enable Sage scripts to control RP2040 hardware

**Status**: Planning / Initial Development

#### Planned Tasks
- [ ] Implement native GPIO functions:
  ```sage
  # Set GPIO 25 (onboard LED) as output
  gpio_init(25, GPIO_OUT)
  gpio_set(25, 1)  # Turn on
  sleep_ms(1000)
  gpio_set(25, 0)  # Turn off
  ```
- [ ] UART control functions:
  ```sage
  uart_write("Hello UART\r\n")
  let data = uart_read()
  ```
- [ ] Timer and delay functions:
  ```sage
  let start = time_us()
  # ... do work ...
  let elapsed = time_us() - start
  ```
- [ ] ADC (analog input) functions:
  ```sage
  let voltage = adc_read(26)  # Read ADC0 on GPIO 26
  ```
- [ ] PWM for LED dimming/servo control:
  ```sage
  pwm_init(15, 1000)  # GPIO 15, 1kHz frequency
  pwm_duty(15, 512)   # 50% duty cycle
  ```

#### Native Function Registration Plan
```c
// In src/sagelang/native_rp2040.c
void register_rp2040_functions(Env* env) {
    // GPIO
    env_define_native(env, "gpio_init", native_gpio_init);
    env_define_native(env, "gpio_set", native_gpio_set);
    env_define_native(env, "gpio_get", native_gpio_get);
    
    // Timing
    env_define_native(env, "sleep_ms", native_sleep_ms);
    env_define_native(env, "time_us", native_time_us);
    
    // ADC
    env_define_native(env, "adc_init", native_adc_init);
    env_define_native(env, "adc_read", native_adc_read);
    
    // PWM
    env_define_native(env, "pwm_init", native_pwm_init);
    env_define_native(env, "pwm_duty", native_pwm_duty);
}
```

#### Example Scripts
**Blink LED:**
```sage
proc blink_led(pin, times):
    gpio_init(pin, GPIO_OUT)
    let i = 0
    while i < times:
        gpio_set(pin, 1)
        sleep_ms(500)
        gpio_set(pin, 0)
        sleep_ms(500)
        i = i + 1

blink_led(25, 10)  # Blink onboard LED 10 times
```

**ADC Logger:**
```sage
proc log_temperature(samples):
    adc_init(26)
    let i = 0
    while i < samples:
        let raw = adc_read(26)
        let voltage = raw * 3.3 / 4096
        print "Reading " + str(i) + ": " + str(voltage) + "V"
        sleep_ms(1000)
        i = i + 1

log_temperature(10)
```

#### Next Steps
1. Create `src/sage_native/` directory
2. Implement GPIO native functions
3. Add timing functions
4. Test with blink example
5. Document hardware API

---

### Phase 5: Script Storage and Execution
**Goal**: Store and run Sage scripts from flash memory

**Status**: Planned

#### Tasks
- [ ] Implement flash-based script storage using LittleFS
- [ ] Add shell commands for script management:
  ```
  > save_script blink "gpio_init(25,1);gpio_set(25,1)"
  > list_scripts
  > run_script blink
  > delete_script temp_log
  ```
- [ ] Implement autorun scripts on boot
- [ ] Background task scheduling

#### Flash Layout
```
0x000000 - 0x1FFFFF: Application Code (littleOS + SageLang)
0x100000 - 0x1F0000: Script Storage (960 KB)
0x1F0000 - 0x200000: Configuration (64 KB)
```

---

### Phase 6: Advanced Features
**Goal**: Full-featured embedded scripting environment

**Status**: Planned

#### Tasks
- [ ] Interrupt handlers in Sage
- [ ] Generators for state machines
- [ ] Exception handling for hardware errors
- [ ] Inter-script communication
- [ ] Debugging support (breakpoints, stepping)

---

### Phase 7: Optimization and Polish
**Goal**: Production-ready embedded scripting

**Status**: Planned

#### Tasks
- [ ] Performance optimization
- [ ] Memory optimization
- [ ] Power management
- [ ] Complete documentation
- [ ] Comprehensive test suite

---

## Current Memory Budget

### RP2040 Resources
- **SRAM**: 264 KB total
- **Flash**: 2 MB total
- **Cores**: 2x Cortex-M0+ @ 133 MHz

### Actual Allocation (Phase 3)
- **OS Kernel**: ~40 KB
- **Shell & I/O**: ~20 KB
- **Sage Interpreter Code**: ~80 KB
- **Sage Heap**: 64 KB (configurable)
- **Sage Stack**: ~8 KB
- **SDK/Drivers**: ~40 KB
- **Free/Buffers**: ~12 KB
- **Total**: ~264 KB

### Flash Usage
- **Bootloader**: 4 KB
- **OS + Sage**: ~180 KB (optimized build)
- **Available for Scripts**: 1.8+ MB
- **Reserved**: TBD

---

## Development Progress

### Milestones
1. ✅ **M1**: littleOS stable with working shell (Week 0)
2. ✅ **M2**: Basic Sage interpreter running on RP2040 (Week 2)
3. ✅ **M3**: REPL integrated into shell (Week 3)
4. 🚧 **M4**: GPIO control from Sage scripts (Week 6 - In Progress)
5. 📋 **M5**: Script storage system working (Week 8)
6. 📋 **M6**: Advanced features demo (Week 11)
7. 📋 **M7**: Production release (Week 14)

### Recent Achievements
- ✅ CMake build system with submodule support
- ✅ Sage embedding API layer
- ✅ Shell command integration
- ✅ Interactive REPL with persistent environment
- ✅ Inline code execution
- ✅ Memory statistics reporting
- ✅ Error handling and reporting
- ✅ Build tested and verified on RP2040

---

## Testing Strategy

### Current Tests ✅
- [x] Basic interpreter functionality
- [x] REPL interaction
- [x] Inline code execution
- [x] Error handling
- [x] Memory management

### Planned Tests 📋
- [ ] GPIO control accuracy
- [ ] ADC reading precision
- [ ] PWM waveform generation
- [ ] UART communication
- [ ] Long-running stability (24+ hours)
- [ ] Performance benchmarks
- [ ] Memory pressure tests

---

## Success Criteria

### Phase 2 Success ✅
- [x] Sage interpreter boots and runs basic scripts
- [x] Memory usage < 160 KB (code + heap)
- [x] Can execute 1000+ operations/second

### Phase 3 Success ✅
- [x] REPL responds within 100ms
- [x] Error messages are clear and helpful
- [x] Inline execution works reliably
- [x] Memory stats accessible

### Phase 4 Success (In Progress)
- [ ] Can toggle GPIO at 1+ kHz
- [ ] ADC reads within 1% accuracy
- [ ] PWM generates clean waveforms

---

## Getting Started

### Build with SageLang

```bash
# Clone repository
git clone https://github.com/Night-Traders-Dev/littleOS.git
cd littleOS

# Initialize submodule
git submodule update --init --recursive

# Build
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)

# Flash
cp littleos.uf2 /media/$USER/RPI-RP2/
```

### Using SageLang

```bash
# Connect to shell
screen /dev/ttyACM0 115200

# Try the REPL
> sage
sage> print "Hello, World!"
Hello, World!
sage> let x = 42
sage> print x * 2
84
sage> exit

# Execute inline code
> sage -e "print 2 + 2"
4

# Check memory
> sage -m
SageLang Memory:
  Allocated: 1024 bytes
  Objects: 12
```

---

## Resources

### Repositories
- [littleOS](https://github.com/Night-Traders-Dev/littleOS)
- [SageLang](https://github.com/Night-Traders-Dev/SageLang)

### Documentation
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang README](https://github.com/Night-Traders-Dev/SageLang/blob/main/README.md)
- [littleOS Quick Start](../SAGELANG_QUICKSTART.md)

---

## Contributing

We welcome contributions! Areas where help is needed:

### Phase 4 (Hardware Bindings)
- GPIO native functions
- ADC/PWM implementations
- Timer functions
- UART control

### Phase 5 (Script Storage)
- LittleFS integration
- Script management commands
- Autorun functionality

### Documentation
- Tutorial examples
- API reference
- Best practices guide

### Testing
- Hardware-in-loop tests
- Performance benchmarks
- Stress tests

---

## Next Steps

### Immediate (Phase 4)
1. Create `src/sage_native/` directory structure
2. Implement basic GPIO functions:
   - `gpio_init(pin, mode)`
   - `gpio_set(pin, value)`
   - `gpio_get(pin)`
3. Add timing functions:
   - `sleep_ms(ms)`
   - `sleep_us(us)`
   - `time_us()`
4. Test with LED blink example
5. Document hardware API

### Short Term (Weeks 4-6)
1. Implement ADC functions
2. Implement PWM functions
3. Add UART control
4. Create example scripts library
5. Write hardware binding documentation

### Medium Term (Weeks 6-10)
1. Implement LittleFS integration
2. Add script storage commands
3. Implement autorun functionality
4. Add background task support
5. Create script backup/restore

---

**Document Status**: Updated to reflect Phase 3 completion  
**Last Updated**: December 2, 2025  
**Current Phase**: Phase 4 (Hardware Bindings) - Planning  
**Next Milestone**: M4 - GPIO control from Sage scripts  
**Author**: Night-Traders-Dev
