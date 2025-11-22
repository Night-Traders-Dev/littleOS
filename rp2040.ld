ENTRY(Reset_Handler)

/* RP2040: 2 MiB external flash, 256 KiB SRAM. [web:6][web:18] */
MEMORY
{
    BOOT2 (rx)  : ORIGIN = 0x10000000, LENGTH = 0x100     /* 256 B second-stage */
    FLASH (rx)  : ORIGIN = 0x10000100, LENGTH = 2048K-0x100
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
    .boot2 ORIGIN(BOOT2) :
    {
        KEEP(*(.boot2*))
    } > BOOT2

    .isr_vector ORIGIN(FLASH) :
    {
        KEEP(*(.isr_vector*))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        *(.glue_7*)
        *(.glue_7t*)
    } > FLASH

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > FLASH

    .data : AT (ADDR(.text) + SIZEOF(.text))
    {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM

    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    _sidata = LOADADDR(.data);
    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
