MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rw) : ORIGIN = 0x20000000, LENGTH = 64K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text : { *(.text*) *(.glue_7*) } > FLASH
    .rodata : { *(.rodata*) } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH
    _sidata = .;
    .data : AT(_sidata) { _sdata = .; *(.data*); _edata = .; } > RAM
    .noinit (NOLOAD) : { *(.noinit*) } > RAM
    .bss : { _sbss = .; *(.bss*) *(COMMON); _ebss = .; } > RAM
    .stack : { . = ALIGN(8); . = . + 0x400; } > RAM
    ASSERT(_ebss + 0x400 <= ORIGIN(RAM) + LENGTH(RAM), "RAM overflow")
}
