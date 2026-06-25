    .section .text.startup, "ax"
    .balign 4
    .global _start
    .type _start, %function

_start:
    ldr     x0, =__stack_top
    and     sp, x0, #~0xf

    ldr     x0, =_sdata
    ldr     x1, =_edata
    ldr     x2, =_sidata
1:
    cmp     x0, x1
    b.hs    2f
    ldr     x3, [x2], #8
    str     x3, [x0], #8
    b       1b

2:
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    mov     x2, xzr
3:
    cmp     x0, x1
    b.hs    4f
    str     x2, [x0], #8
    b       3b

4:
    msr     daifset, #0xf
    bl      main
5:
    wfi
    b       5b
