#include "config/aixos_cfg.h"

.syntax unified
.cpu    cortex-m3
.thumb

.equ KERNEL_PRIORITY_THRESHOLD, AIXOS_CFG_KERNEL_IRQ_PRIORITY

.global aixos_arch_int_disable
.global aixos_arch_int_restore
.global aixos_arch_context_switch
.global aixos_arch_start_first_task
.global aixos_arch_tick_handler
.global PendSV_Handler
.global HardFault_Handler
.global MemManage_Handler
.global BusFault_Handler
.global UsageFault_Handler
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
@ aixos_arch_flags_t aixos_arch_int_disable(void)
@ 使用 BASEPRI 实现临界区 (支持中断嵌套)
@
.type   aixos_arch_int_disable, %function
aixos_arch_int_disable:
    mrs     R0, BASEPRI
    mov     R1, #KERNEL_PRIORITY_THRESHOLD
    msr     BASEPRI, R1
    dsb
    isb
    bx      LR
.size   aixos_arch_int_disable, . - aixos_arch_int_disable

@
@ void aixos_arch_int_restore(aixos_arch_flags_t flags)
@
.type   aixos_arch_int_restore, %function
aixos_arch_int_restore:
    msr     BASEPRI, R0
    dsb
    bx      LR
.size   aixos_arch_int_restore, . - aixos_arch_int_restore

@
@ void aixos_arch_context_switch(void)
@ 触发 PendSV
@
.type   aixos_arch_context_switch, %function
aixos_arch_context_switch:
    ldr     R0, =0xE000ED04
    ldr     R1, =0x10000000
    str     R1, [R0]
    dsb
    isb
    bx      LR
.size   aixos_arch_context_switch, . - aixos_arch_context_switch

@
@ void aixos_arch_tick_handler(void) — SysTick_Handler
@
.type   aixos_arch_tick_handler, %function
aixos_arch_tick_handler:
    push    {R4, LR}
    bl      aixos_isr_enter
    bl      aixos_tick_handler
    bl      aixos_isr_exit
    pop     {R4, PC}
.size   aixos_arch_tick_handler, . - aixos_arch_tick_handler

@ SysTick_Handler 别名
.thumb_func
.global SysTick_Handler
SysTick_Handler:
    b       aixos_arch_tick_handler

@
@ void PendSV_Handler(void)
@ 首次启动: 直接加载第一个任务并返回
@ 后续切换: 保存上下文, 调度, 恢复新任务
@
.type   PendSV_Handler, %function
PendSV_Handler:
    ldr     R3, =aixos_first_start
    ldr     R2, [R3]
    cbz     R2, PendSV_Save

    @ First start: load the already selected task.
    mov     R2, #0
    str     R2, [R3]
    ldr     R1, =g_cur_task
    ldr     R1, [R1]
    ldr     R0, [R1, #0]        @ stack_top
    ldmia   R0!, {R4-R11}
    msr     PSP, R0
    ldr     R0, [R1, #4]        @ CONTROL: privileged/user PSP
    msr     CONTROL, R0
    isb
    mov     R0, #0xFFFFFFFD
    bx      R0

PendSV_Save:
    mrs     R2, BASEPRI
    push    {R2, LR}
    mov     R3, #KERNEL_PRIORITY_THRESHOLD
    msr     BASEPRI, R3
    dsb
    isb
    mrs     R0, PSP
    stmdb   R0!, {R4-R11}
    ldr     R1, =g_cur_task
    ldr     R1, [R1]
    str     R0, [R1, #0]        @ save current task stack_top

    bl      aixos_schedule

    ldr     R1, =g_cur_task
    ldr     R1, [R1]
    ldr     R0, [R1, #0]        @ load new task stack_top
    ldmia   R0!, {R4-R11}
    msr     PSP, R0
    ldr     R0, [R1, #4]
    msr     CONTROL, R0
    isb
    pop     {R2, LR}
    msr     BASEPRI, R2
    dsb
    isb
    bx      LR
.size   PendSV_Handler, . - PendSV_Handler

@
@ void aixos_arch_start_first_task(void)
@ PendSV 挂起 + 死循环等待 PendSV 切换上下文
@
.type   aixos_arch_start_first_task, %function
aixos_arch_start_first_task:
    ldr     R0, =0xE000ED04
    ldr     R1, =0x10000000
    str     R1, [R0]
    dsb
    isb
.Lwait:
    wfi
    b       .Lwait
.size   aixos_arch_start_first_task, . - aixos_arch_start_first_task

.thumb_func
SVC_Handler:
    tst     LR, #4
    ite     eq
    mrseq   R0, MSP
    mrsne   R0, PSP
    push    {R4, LR}
    bl      aixos_arm_svc_handler
    pop     {R4, PC}

.thumb_func
HardFault_Handler:
    movs    R1, #3
    b       aixos_fault_entry

.thumb_func
MemManage_Handler:
    movs    R1, #4
    b       aixos_fault_entry

.thumb_func
BusFault_Handler:
    movs    R1, #5
    b       aixos_fault_entry

.thumb_func
UsageFault_Handler:
    movs    R1, #6
.thumb_func
aixos_fault_entry:
    tst     LR, #4
    ite     eq
    mrseq   R0, MSP
    mrsne   R0, PSP
    b       aixos_arm_fault_handler
