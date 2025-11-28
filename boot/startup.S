.syntax unified
.cpu cortex-m0plus
.thumb

.section .vectors, "ax"
.align 2
.global __vectors
__vectors:
    .word __stack_top
    .word reset_handler
    .word 0  // NMI
    .word 0  // HardFault
    // ... additional vectors

.section .text
.thumb_func
.global reset_handler
reset_handler:
    // Set up clocks and PLL
    // Initialize data/bss sections
    // Call main
    bl main
    b .
