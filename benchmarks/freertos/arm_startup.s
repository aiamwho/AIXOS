.syntax unified
.cpu cortex-m3
.thumb

.global _estack
.global Reset_Handler
.extern main
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss
.extern SVC_Handler
.extern PendSV_Handler
.extern SysTick_Handler

.section .isr_vector, "a", %progbits
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word 0
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler
    .rept 80
    .word Default_Handler
    .endr

.section .text.Reset_Handler, "ax", %progbits
.thumb_func
Reset_Handler:
    ldr r1, =_sidata
    ldr r2, =_sdata
    ldr r3, =_edata
    subs r3, r2
    ble .Lbss
.Ldata:
    subs r3, #4
    ldr r0, [r1, r3]
    str r0, [r2, r3]
    bgt .Ldata
.Lbss:
    ldr r1, =_sbss
    ldr r2, =_ebss
    subs r2, r1
    ble .Lmain
    movs r0, #0
.Lfill:
    str r0, [r1], #4
    subs r2, #4
    bgt .Lfill
.Lmain:
    bl main
.Lstop:
    b .Lstop

.thumb_func
Default_Handler:
    b .
