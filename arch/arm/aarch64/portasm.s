    .section .vectors, "ax"
    .balign 2048
    .global aarch64_vectors
    .type aarch64_vectors, %function

aarch64_vectors:
    b       aarch64_sync_entry_sp0
    .balign 0x80
    b       aarch64_irq_entry_sp0
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_sync_entry_spx
    .balign 0x80
    b       aarch64_irq_entry_spx
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_sync_entry_lower_a64
    .balign 0x80
    b       aarch64_irq_entry_lower_a64
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry
    .balign 0x80
    b       aarch64_unhandled_entry

    .section .text, "ax"
    .balign 4

    .equ CTX_SIZE, 272
    .equ CTX_SP_OFF, 248
    .equ CTX_ELR_OFF, 256
    .equ CTX_SPSR_OFF, 264

    .global aixos_arch_int_disable
    .global aixos_arch_int_restore
    .global aixos_arch_start_first_task

    .extern g_cur_task
    .extern aixos_aarch64_sync_entry
    .extern aixos_aarch64_irq_entry

    .macro SAVE_CONTEXT
    sub     sp, sp, #CTX_SIZE
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]
    add     x9, sp, #CTX_SIZE
    str     x9, [sp, #CTX_SP_OFF]
    mrs     x9, elr_el1
    str     x9, [sp, #CTX_ELR_OFF]
    mrs     x9, spsr_el1
    str     x9, [sp, #CTX_SPSR_OFF]
    ldr     x9, =g_cur_task
    ldr     x10, [x9]
    cbz     x10, 1f
    mov     x11, sp
    str     x11, [x10]
1:
    .endm

    .macro RESTORE_CONTEXT
    ldr     x9, =g_cur_task
    ldr     x9, [x9]
    ldr     x9, [x9]
    mov     sp, x9
    ldr     x10, [sp, #CTX_ELR_OFF]
    msr     elr_el1, x10
    ldr     x10, [sp, #CTX_SPSR_OFF]
    msr     spsr_el1, x10
    msr     daif, x10
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]
    add     sp, sp, #CTX_SIZE
    eret
    .endm

aixos_arch_int_disable:
    mrs     x0, daif
    msr     daifset, #0xf
    isb
    ret

aixos_arch_int_restore:
    msr     daif, x0
    isb
    ret

aixos_arch_start_first_task:
    RESTORE_CONTEXT

aarch64_sync_entry_sp0:
aarch64_sync_entry_spx:
aarch64_sync_entry_lower_a64:
    SAVE_CONTEXT
    mov     x0, sp
    mrs     x1, esr_el1
    ldr     x2, =__interrupt_stack_top
    mov     sp, x2
    bl      aixos_aarch64_sync_entry
    RESTORE_CONTEXT

aarch64_irq_entry_sp0:
aarch64_irq_entry_spx:
aarch64_irq_entry_lower_a64:
    SAVE_CONTEXT
    mov     x0, sp
    ldr     x2, =__interrupt_stack_top
    mov     sp, x2
    bl      aixos_aarch64_irq_entry
    RESTORE_CONTEXT

aarch64_unhandled_entry:
    wfi
    b       aarch64_unhandled_entry
