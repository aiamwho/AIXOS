#include "config/aixos_cfg.h"

.syntax unified
.thumb

.global aixos_arch_int_disable
.global aixos_arch_int_restore
.global aixos_arch_context_switch
.global aixos_arch_start_first_task
.global aixos_arch_tick_handler
.global PendSV_Handler
.global HardFault_Handler
.global SVC_Handler
.global aixos_fault_entry

.extern g_cur_task
.extern aixos_schedule
.extern aixos_first_start
.extern aixos_isr_enter
.extern aixos_isr_exit
.extern aixos_arm_fault_handler
.extern aixos_arm_svc_handler

@
@ Cortex-M0 has no BASEPRI, so the ARMv6-M port uses PRIMASK for
@ kernel critical sections.
@
.type   aixos_arch_int_disable, %function
aixos_arch_int_disable:
    mrs     R0, PRIMASK
    cpsid   i
    bx      LR
.size   aixos_arch_int_disable, . - aixos_arch_int_disable

.type   aixos_arch_int_restore, %function
aixos_arch_int_restore:
    msr     PRIMASK, R0
    bx      LR
.size   aixos_arch_int_restore, . - aixos_arch_int_restore

.type   aixos_arch_context_switch, %function
aixos_arch_context_switch:
    ldr     R0, =0xE000ED04
    ldr     R1, =0x10000000
    str     R1, [R0]
    bx      LR
.size   aixos_arch_context_switch, . - aixos_arch_context_switch

.type   aixos_arch_tick_handler, %function
aixos_arch_tick_handler:
    push    {R4, LR}
    bl      aixos_isr_enter
    bl      aixos_tick_handler
    bl      aixos_isr_exit
    pop     {R4, PC}
.size   aixos_arch_tick_handler, . - aixos_arch_tick_handler

.thumb_func
.global SysTick_Handler
SysTick_Handler:
    b       aixos_arch_tick_handler

.type   PendSV_Handler, %function
PendSV_Handler:
    ldr     R3, =aixos_first_start
    ldr     R2, [R3]
    cmp     R2, #0
    beq     PendSV_Save

    movs    R2, #0
    str     R2, [R3]
    ldr     R1, =g_cur_task
    ldr     R1, [R1]
    ldr     R0, [R1, #0]

    ldmia   R0!, {R4-R7}
    mov     R8, R4
    mov     R9, R5
    mov     R10, R6
    mov     R11, R7
    ldmia   R0!, {R4-R7}

    msr     PSP, R0
    ldr     R0, [R1, #4]
    msr     CONTROL, R0
    movs    R0, #0
    msr     PRIMASK, R0
    ldr     R0, =0xFFFFFFFD
    bx      R0

PendSV_Save:
    mrs     R2, PRIMASK
    cpsid   i
    push    {R2, LR}

    mrs     R0, PSP
    subs    R0, #16
    mov     R2, R0
    stmia   R2!, {R4-R7}
    subs    R0, #16
    mov     R4, R8
    mov     R5, R9
    mov     R6, R10
    mov     R7, R11
    mov     R2, R0
    stmia   R2!, {R4-R7}

    ldr     R1, =g_cur_task
    ldr     R1, [R1]
    str     R0, [R1, #0]

    bl      aixos_schedule

    ldr     R1, =g_cur_task
    ldr     R1, [R1]
    ldr     R0, [R1, #0]

    ldmia   R0!, {R4-R7}
    mov     R8, R4
    mov     R9, R5
    mov     R10, R6
    mov     R11, R7
    ldmia   R0!, {R4-R7}

    msr     PSP, R0
    ldr     R0, [R1, #4]
    msr     CONTROL, R0
    pop     {R2, R3}
    msr     PRIMASK, R2
    bx      R3
.size   PendSV_Handler, . - PendSV_Handler

.type   aixos_arch_start_first_task, %function
aixos_arch_start_first_task:
    ldr     R0, =0xE000ED04
    ldr     R1, =0x10000000
    str     R1, [R0]
.Lwait:
    wfi
    b       .Lwait
.size   aixos_arch_start_first_task, . - aixos_arch_start_first_task

.thumb_func
SVC_Handler:
    movs    R0, #4
    mov     R1, LR
    tst     R1, R0
    bne     .Lsvc_psp
    mrs     R0, MSP
    b       .Lsvc_call
.Lsvc_psp:
    mrs     R0, PSP
.Lsvc_call:
    push    {R4, LR}
    bl      aixos_arm_svc_handler
    pop     {R4, PC}

.thumb_func
HardFault_Handler:
    movs    R1, #3
    b       aixos_fault_entry

.thumb_func
aixos_fault_entry:
    movs    R0, #4
    mov     R2, LR
    tst     R2, R0
    bne     .Lfault_psp
    mrs     R0, MSP
    ldr     R3, =aixos_arm_fault_handler
    bx      R3
.Lfault_psp:
    mrs     R0, PSP
    ldr     R3, =aixos_arm_fault_handler
    bx      R3
