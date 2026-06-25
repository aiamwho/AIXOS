ENTRY(_start)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 128M
    RAM   (rw) : ORIGIN = 0x40000000, LENGTH = 2048M
}

__stack_top = ORIGIN(RAM) + 0x00100000;
__interrupt_stack_top = __stack_top;

SECTIONS
{
    . = ORIGIN(FLASH);
    .text : {
        KEEP(*(.text.startup))
        KEEP(*(.vectors))
        *(.text*)
    } > FLASH

    .rodata : { *(.rodata*) } > FLASH
    .eh_frame : { *(.eh_frame*) } > FLASH

    _sidata = .;
    .data : AT(_sidata) {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM

    .noinit (NOLOAD) : { *(.noinit*) } > RAM
    .bss : {
        __bss_start = .;
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
        __bss_end = .;
    } > RAM

    .stack (NOLOAD) : {
        . = ALIGN(16);
        . = . + 0x4000;
    } > RAM
}
