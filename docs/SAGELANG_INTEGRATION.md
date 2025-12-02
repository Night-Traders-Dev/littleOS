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
- **Interactive scripting** from the shell
- **Dynamic GPIO control** via scripts
- **System automation** without recompiling
- **Educational platform** for learning embedded programming

## Integration Phases

### Phase 1: Preparation and Analysis
**Goal**: Understand constraints and prepare the codebase

#### Tasks
- [x] Fix littleOS boot issues and stabilize shell
- [x] Document current littleOS architecture
- [x] Analyze SageLang memory requirements
- [ ] Profile SageLang on desktop to establish baseline
- [ ] Identify RP2040 memory constraints (264KB RAM, 2MB Flash)
- [ ] Create integration branch in both repositories

#### Deliverables
- Stable littleOS v0.1.0 with working shell ✅
- Memory profiling report
- Integration architecture document

---

### Phase 2: Minimal Interpreter Port
**Goal**: Get a basic SageLang interpreter running on RP2040

#### Tasks
- [ ] Create `src/sagelang/` directory structure in littleOS
- [ ] Port core SageLang files (simplified subset):
  - `lexer.c` - Tokenization
  - `parser.c` - Parsing (basic expressions first)
  - `interpreter.c` - Evaluation
  - `value.c` - Type system (start with primitives)
  - `ast.c` - AST nodes
  - `env.c` - Scoping
- [ ] Remove or stub desktop-specific dependencies:
  - File I/O (replace with embedded alternatives)
  - Standard library functions not available on RP2040
- [ ] Create embedded-friendly allocators:
  - Replace `malloc/free` with arena allocator
  - Fixed-size memory pools for common objects
- [ ] Minimal GC configuration:
  - Aggressive collection for limited RAM
  - Configurable heap size (start with 32-64KB)

#### Configuration Macros
```c
#define SAGE_EMBEDDED_MODE 1
#define SAGE_MAX_HEAP_SIZE (64 * 1024)  // 64KB for interpreter
#define SAGE_STACK_SIZE 256             // Call stack depth
#define SAGE_GC_THRESHOLD 4096          // Trigger GC at 4KB growth
#define SAGE_DISABLE_FILE_IO 1          // No filesystem yet
```

#### Test Cases
- Basic arithmetic: `2 + 2`, `10 * 5`
- Variable assignment: `let x = 42`
- Simple print: `print "Hello from Sage!"`
- Function definition and call

#### Deliverables
- Working minimal SageLang interpreter on RP2040
- Memory usage benchmarks
- Basic shell integration

---

### Phase 3: Shell Integration
**Goal**: Execute Sage code from the littleOS shell

#### Tasks
- [ ] Add `sage` shell command to execute inline code
- [ ] Implement REPL mode:
  ```
  > sage
  Sage REPL v0.8.0 (type 'exit' to quit)
  sage> let x = 10
  sage> print x
  10
  sage> exit
  >
  ```
- [ ] Add multiline input support (detect incomplete statements)
- [ ] Implement script execution from string buffers:
  ```
  > sage "print 'Hello World'"
  Hello World
  ```
- [ ] Error handling and display:
  - Catch Sage exceptions
  - Display friendly error messages in shell
- [ ] Add memory inspection commands:
  ```
  > sage_mem
  Heap: 12.5 KB / 64 KB (19%)
  Objects: 47
  GC Collections: 3
  ```

#### New Shell Commands
- `sage [code]` - Execute inline Sage code
- `sage` - Enter REPL mode
- `sage_mem` - Show Sage memory statistics
- `sage_gc` - Trigger garbage collection
- `sage_reset` - Reset Sage interpreter state

#### Deliverables
- Shell command integration
- REPL with multiline support
- Error reporting system

---

### Phase 4: Hardware Bindings
**Goal**: Enable Sage scripts to control RP2040 hardware

#### Tasks
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

#### Native Function Registration
```c
// In src/sagelang/native_rp2040.c
void register_rp2040_functions(Environment* env) {
    env_define_native(env, "gpio_init", native_gpio_init);
    env_define_native(env, "gpio_set", native_gpio_set);
    env_define_native(env, "gpio_get", native_gpio_get);
    env_define_native(env, "sleep_ms", native_sleep_ms);
    env_define_native(env, "time_us", native_time_us);
    // ... more functions
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
    let i = 0
    while i < samples:
        let raw = adc_read(26)
        let voltage = raw * 3.3 / 4096
        print "Reading " + str(i) + ": " + str(voltage) + "V"
        sleep_ms(1000)
        i = i + 1

log_temperature(10)
```

#### Deliverables
- Hardware native function library
- GPIO control examples
- ADC/PWM/Timer examples
- Documentation of all hardware APIs

---

### Phase 5: Script Storage and Execution
**Goal**: Store and run Sage scripts from flash memory

#### Tasks
- [ ] Implement simple flash-based script storage:
  - Reserve flash sectors for scripts
  - Simple key-value store (script name → code)
- [ ] Add shell commands for script management:
  ```
  > save_script blink "gpio_init(25,1);gpio_set(25,1)"
  Script 'blink' saved (32 bytes)
  
  > list_scripts
  Scripts:
    - blink (32 bytes)
    - temp_log (156 bytes)
  
  > run_script blink
  [LED blinks]
  
  > delete_script temp_log
  Script 'temp_log' deleted
  ```
- [ ] Implement autorun scripts:
  - Execute startup.sage on boot
  - Background task scheduling

#### Flash Layout
```
0x000000 - 0x1FFFFF: Application Code (littleOS + SageLang)
0x100000 - 0x1F0000: Script Storage (960 KB)
0x1F0000 - 0x200000: Configuration (64 KB)
```

#### Script Format
```c
typedef struct {
    char name[32];
    uint32_t size;
    uint32_t crc32;
    uint8_t data[];
} ScriptEntry;
```

#### Deliverables
- Flash-based script storage system
- Script management shell commands
- Autorun capability
- Script backup/restore utilities

---

### Phase 6: Advanced Features
**Goal**: Full-featured embedded scripting environment

#### Tasks
- [ ] **Interrupt handlers in Sage:**
  ```sage
  proc button_handler():
      print "Button pressed!"
  
  gpio_set_irq(14, button_handler, IRQ_EDGE_FALL)
  ```
- [ ] **Generators for state machines:**
  ```sage
  proc traffic_light():
      while true:
          yield "RED"
          sleep_ms(5000)
          yield "YELLOW"
          sleep_ms(2000)
          yield "GREEN"
          sleep_ms(5000)
  
  let light = traffic_light()
  while true:
      let state = next(light)
      print "Light: " + state
  ```
- [ ] **Exception handling for hardware errors:**
  ```sage
  try:
      let temp = adc_read(26)
      if temp > 3.0:
          raise "Temperature too high!"
  catch e:
      gpio_set(25, 1)  # Turn on warning LED
      print "Error: " + e
  ```
- [ ] **Inter-script communication:**
  - Global variables accessible across scripts
  - Event system for script-to-script messaging
- [ ] **Debugging support:**
  - Step-through execution
  - Breakpoints
  - Variable inspection

#### Deliverables
- Interrupt-driven Sage code
- Advanced examples (state machines, protocols)
- Debugging tools

---

### Phase 7: Optimization and Polish
**Goal**: Production-ready embedded scripting

#### Tasks
- [ ] Performance optimization:
  - Bytecode compilation (optional)
  - Inline native functions
  - Optimize hot paths
- [ ] Memory optimization:
  - Object pooling
  - String interning
  - Compact value representation
- [ ] Power management:
  - Sleep modes during idle
  - Low-power script execution
- [ ] Documentation:
  - API reference for all native functions
  - Tutorial examples
  - Best practices guide
- [ ] Testing:
  - Unit tests for all components
  - Hardware-in-loop tests
  - Stress tests (memory, performance)

#### Deliverables
- Optimized interpreter (< 128 KB code size)
- Complete documentation
- Test suite with >80% coverage
- Performance benchmarks

---

## Memory Budget

### RP2040 Resources
- **SRAM**: 264 KB total
- **Flash**: 2 MB total
- **Cores**: 2x Cortex-M0+ @ 133 MHz

### Allocation Plan
- **OS Kernel**: 32 KB
- **Shell & I/O**: 16 KB
- **Sage Interpreter Code**: 96 KB
- **Sage Heap**: 64 KB
- **Sage Stack**: 8 KB
- **SDK/Drivers**: 32 KB
- **Free/Buffers**: 16 KB
- **Total**: 264 KB

### Flash Usage
- **Bootloader**: 4 KB
- **OS + Sage**: ~200 KB
- **Scripts**: 960 KB
- **Config**: 64 KB
- **Reserved**: ~768 KB

---

## Development Timeline

### Estimated Effort
- **Phase 1**: 1 week
- **Phase 2**: 2-3 weeks
- **Phase 3**: 1 week
- **Phase 4**: 2 weeks
- **Phase 5**: 1-2 weeks
- **Phase 6**: 2-3 weeks
- **Phase 7**: 1-2 weeks

**Total**: ~10-14 weeks for full integration

### Milestones
1. ✅ **M1**: littleOS stable with working shell (Week 0) - **COMPLETE**
2. **M2**: Basic Sage interpreter running on RP2040 (Week 3)
3. **M3**: REPL integrated into shell (Week 4)
4. **M4**: GPIO control from Sage scripts (Week 6)
5. **M5**: Script storage system working (Week 8)
6. **M6**: Advanced features demo (Week 11)
7. **M7**: Production release (Week 14)

---

## Testing Strategy

### Unit Tests
- Lexer/Parser correctness
- Interpreter semantics
- GC under memory pressure
- Native function bindings

### Integration Tests
- Shell + Sage interaction
- Script storage reliability
- Hardware control accuracy
- Error recovery

### Hardware Tests
- GPIO toggling at various frequencies
- ADC accuracy and noise
- PWM waveform generation
- UART communication reliability
- Long-running stability (24+ hours)

### Performance Tests
- Execution speed benchmarks
- Memory allocation patterns
- GC pause times
- Worst-case response latency

---

## Risks and Mitigations

### Risk 1: Memory Constraints
**Impact**: High - May prevent full feature set
**Mitigation**:
- Start with minimal subset of Sage features
- Aggressive GC tuning
- Consider bytecode compilation for space efficiency
- Use flash for rarely-used code (XIP)

### Risk 2: Performance
**Impact**: Medium - Scripts may be too slow for real-time control
**Mitigation**:
- Profile early and often
- Optimize interpreter hot paths
- Provide native function wrappers for critical operations
- Consider JIT compilation (long-term)

### Risk 3: Complexity
**Impact**: Medium - Integration may be more complex than anticipated
**Mitigation**:
- Incremental development with frequent testing
- Maintain separate branches for experimentation
- Document architectural decisions
- Seek community feedback

---

## Success Criteria

### Phase 2 Success
- [ ] Sage interpreter boots and runs basic scripts
- [ ] Memory usage < 96 KB (code + heap)
- [ ] Can execute 1000 operations/second

### Phase 3 Success
- [ ] REPL responds within 100ms
- [ ] Multiline input works reliably
- [ ] Error messages are clear and helpful

### Phase 4 Success
- [ ] Can toggle GPIO at 1+ kHz
- [ ] ADC reads within 1% accuracy
- [ ] PWM generates clean waveforms

### Phase 7 Success
- [ ] Complete API documentation
- [ ] 10+ example scripts
- [ ] Passes all integration tests
- [ ] Runs stably for 24+ hours
- [ ] Community adoption and positive feedback

---

## Resources

### Repositories
- [littleOS](https://github.com/Night-Traders-Dev/littleOS)
- [SageLang](https://github.com/Night-Traders-Dev/SageLang)

### Documentation
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [SageLang README](https://github.com/Night-Traders-Dev/SageLang/blob/main/README.md)
- [SageLang Roadmap](https://github.com/Night-Traders-Dev/SageLang/blob/main/ROADMAP.md)

### Community
- GitHub Issues for bug reports
- GitHub Discussions for design decisions
- Discord (if available) for real-time help

---

## Next Steps

1. ✅ Stabilize littleOS (boot fixes, shell working)
2. ✅ Create documentation structure
3. **Create integration branch**: `feature/sagelang-integration`
4. **Profile SageLang**: Measure memory and performance on desktop
5. **Start Phase 2**: Begin porting core interpreter files
6. **Set up CI/CD**: Automated builds and tests for both repos

---

**Document Status**: Draft v1.0  
**Last Updated**: December 1, 2025  
**Author**: Night-Traders-Dev  
**Next Review**: Start of Phase 2
