.syntax unified
.cpu cortex-m0plus
.thumb

.global Reset_Handler
.global __vectors

.section .vectors
__vectors:
    .word __stack_top      /* Top of Stack */
    .word Reset_Handler    /* Reset Handler */
    .word 0                /* NMI Handler */
    .word 0                /* Hard Fault Handler */

.section .text
.thumb_func
Reset_Handler:
    /* Copy .data section from Flash to RAM */
    ldr r0, =__data_start__
    ldr r1, =__data_end__
    ldr r2, =0x10000000    /* Flash start, simplified approximation for load addr */
    /* Note: Real copy loop needs proper Load Address symbol from linker */
    
    /* Zero out .bss section */
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    movs r2, #0
bss_loop:
    cmp r0, r1
    bge main_entry
    str r2, [r0]
    adds r0, #4
    b bss_loop

main_entry:
    bl main
    b .
