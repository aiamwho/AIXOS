.syntax unified
.cpu    cortex-m3
.thumb

.global _estack
.global Reset_Handler

.weak   NMI_Handler
.weak   HardFault_Handler
.weak   MemManage_Handler
.weak   BusFault_Handler
.weak   UsageFault_Handler
.weak   SVC_Handler
.global Default_Handler

.extern main
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss
.extern aixos_arm_fault_handler

.section .isr_vector, "a", %progbits
.type     g_pfnVectors, %object
g_pfnVectors:
    .word   _estack
    .word   Reset_Handler
    .word   NMI_Handler
    .word   HardFault_Handler
    .word   MemManage_Handler
    .word   BusFault_Handler
    .word   UsageFault_Handler
    .word   0
    .word   0
    .word   0
    .word   0
    .word   SVC_Handler
    .word   0
    .word   0
    .word   PendSV_Handler
    .word   SysTick_Handler
    .rept   80
    .word   Default_Handler
    .endr
.size   g_pfnVectors, . - g_pfnVectors

.section .text.Reset_Handler, "ax", %progbits
.type   Reset_Handler, %function
.thumb_func
Reset_Handler:
    ldr     r1, =_sidata
    ldr     r2, =_sdata
    ldr     r3, =_edata
    subs    r3, r2
    ble     .Lbss
.Ldata:
    subs    r3, #4
    ldr     r0, [r1, r3]
    str     r0, [r2, r3]
    bgt     .Ldata
.Lbss:
    ldr     r1, =_sbss
    ldr     r2, =_ebss
    subs    r2, r1
    ble     .Lmain
    movs    r0, #0
.Lfill:
    str     r0, [r1], #4
    subs    r2, #4
    bgt     .Lfill
.Lmain:
    bl      main
.Lloop:
    b       .Lloop

.macro  def_weak  name
.section .text.Default_Handler, "ax", %progbits
.thumb_func
.type    \name, %function
\name:
    b       .
.endm

Default_Handler:
    movs    r1, #16
    mov     r0, sp
    bl      aixos_arm_fault_handler
    b       .

def_weak NMI_Handler
