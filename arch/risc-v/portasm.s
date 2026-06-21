    .section .text
    .balign 4

    /* 34 saved words plus 8 bytes padding preserve the 16-byte ABI. */
    .equ FRAME_SIZE, 144
    .equ MEPC_OFF, 128
    .equ MSTATUS_OFF, 132

    .global aixos_arch_int_disable
    .global aixos_arch_int_restore
    .global aixos_arch_context_switch
    .global aixos_arch_start_first_task
    .global trap_handler

    .extern g_cur_task
    .extern aixos_schedule
    .extern aixos_tick_handler
    .extern aixos_arch_timer_ack
    .extern aixos_arch_software_ack
    .extern aixos_arch_trigger_switch
    .extern aixos_riscv_timer_interrupts
    .extern aixos_riscv_software_interrupts
    .extern aixos_riscv_unhandled_mcause
    .extern aixos_riscv_unhandled_mepc
    .extern aixos_isr_enter
    .extern aixos_isr_exit
    .extern aixos_crash_record_store
    .extern aixos_riscv_syscall_entry

aixos_arch_int_disable:
    csrrci  a0, mstatus, 8
    fence   rw, rw
    ret

aixos_arch_int_restore:
    fence   rw, rw
    csrw    mstatus, a0
    ret

aixos_arch_context_switch:
    tail    aixos_arch_trigger_switch

aixos_arch_start_first_task:
    la      t0, g_cur_task
    lw      t0, 0(t0)
    lw      sp, 0(t0)
    j       trap_restore

trap_handler:
    addi    sp, sp, -FRAME_SIZE
    sw      x1, 4(sp)
    sw      x3, 12(sp)
    sw      x4, 16(sp)
    sw      x5, 20(sp)
    sw      x6, 24(sp)
    sw      x7, 28(sp)
    sw      x8, 32(sp)
    sw      x9, 36(sp)
    sw      x10, 40(sp)
    sw      x11, 44(sp)
    sw      x12, 48(sp)
    sw      x13, 52(sp)
    sw      x14, 56(sp)
    sw      x15, 60(sp)
    sw      x16, 64(sp)
    sw      x17, 68(sp)
    sw      x18, 72(sp)
    sw      x19, 76(sp)
    sw      x20, 80(sp)
    sw      x21, 84(sp)
    sw      x22, 88(sp)
    sw      x23, 92(sp)
    sw      x24, 96(sp)
    sw      x25, 100(sp)
    sw      x26, 104(sp)
    sw      x27, 108(sp)
    sw      x28, 112(sp)
    sw      x29, 116(sp)
    sw      x30, 120(sp)
    sw      x31, 124(sp)
    addi    t0, sp, FRAME_SIZE
    sw      t0, 8(sp)
    csrr    t0, mepc
    sw      t0, MEPC_OFF(sp)
    csrr    t0, mstatus
    sw      t0, MSTATUS_OFF(sp)

    la      t0, g_cur_task
    lw      t0, 0(t0)
    beqz    t0, trap_cause
    sw      sp, 0(t0)

trap_cause:
    la      sp, __interrupt_stack_top
    csrr    t0, mcause
    li      t1, 8
    beq     t0, t1, trap_syscall
    call    aixos_isr_enter
    li      t1, 0x80000007
    beq     t0, t1, trap_timer
    li      t1, 0x80000003
    beq     t0, t1, trap_software
trap_unhandled:
    la      t1, aixos_riscv_unhandled_mcause
    sw      t0, 0(t1)
    csrr    t0, mepc
    la      t1, aixos_riscv_unhandled_mepc
    sw      t0, 0(t1)
    li      a0, 2
    csrr    a1, mcause
    csrr    a2, mepc
    csrr    a3, mtval
    la      t0, g_cur_task
    lw      t0, 0(t0)
    beqz    t0, 1f
    lw      a4, 0(t0)
    j       2f
1:
    li      a4, 0
2:
    call    aixos_crash_record_store
    wfi
    j       trap_unhandled

trap_syscall:
    call    aixos_riscv_syscall_entry
    j       trap_restore

trap_timer:
    la      t0, aixos_riscv_timer_interrupts
    lw      t1, 0(t0)
    addi    t1, t1, 1
    sw      t1, 0(t0)
    call    aixos_arch_timer_ack
    call    aixos_tick_handler
    call    aixos_isr_exit
    j       trap_restore

trap_software:
    la      t0, aixos_riscv_software_interrupts
    lw      t1, 0(t0)
    addi    t1, t1, 1
    sw      t1, 0(t0)
    call    aixos_arch_software_ack
    call    aixos_schedule
    call    aixos_isr_exit

trap_restore:
    la      t0, g_cur_task
    lw      t0, 0(t0)
    lw      sp, 0(t0)
    lw      t0, MEPC_OFF(sp)
    csrw    mepc, t0
    lw      t0, MSTATUS_OFF(sp)
    csrw    mstatus, t0
    lw      x1, 4(sp)
    lw      x3, 12(sp)
    lw      x4, 16(sp)
    lw      x5, 20(sp)
    lw      x6, 24(sp)
    lw      x7, 28(sp)
    lw      x8, 32(sp)
    lw      x9, 36(sp)
    lw      x10, 40(sp)
    lw      x11, 44(sp)
    lw      x12, 48(sp)
    lw      x13, 52(sp)
    lw      x14, 56(sp)
    lw      x15, 60(sp)
    lw      x16, 64(sp)
    lw      x17, 68(sp)
    lw      x18, 72(sp)
    lw      x19, 76(sp)
    lw      x20, 80(sp)
    lw      x21, 84(sp)
    lw      x22, 88(sp)
    lw      x23, 92(sp)
    lw      x24, 96(sp)
    lw      x25, 100(sp)
    lw      x26, 104(sp)
    lw      x27, 108(sp)
    lw      x28, 112(sp)
    lw      x29, 116(sp)
    lw      x30, 120(sp)
    lw      x31, 124(sp)
    lw      x2, 8(sp)
    mret
