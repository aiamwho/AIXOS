OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}
PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS
{
    . = ORIGIN(RAM);
    .text : {
        KEEP(*(.text.startup))
        *(.text*)
    } > RAM :text
    .rodata : { *(.rodata*) } > RAM :text
    .data : {
        __data_start = .;
        *(.sdata*) *(.data*)
        __data_end = .;
    } > RAM :data
    .bss (NOLOAD) : {
        . = ALIGN(16);
        __bss_start = .;
        *(.sbss*) *(.bss*) *(COMMON)
        __bss_end = .;
    } > RAM :data
    . = ALIGN(16);
    __heap_start = .;
    . = ORIGIN(RAM) + LENGTH(RAM);
    __stack_top = .;
    PROVIDE(__global_pointer$ = __data_start + 0x800);
    ASSERT(__heap_start < __stack_top - 0x4000, "RISC-V RAM overflow")
}
