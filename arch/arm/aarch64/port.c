#include "aixos/arch/arch.h"
#include "aixos/microkernel.h"
#include "aixos/task.h"
#include "config/aixos_cfg.h"
#include "kernel/sched.h"
#include <stdint.h>

#define A55_TIMER_HZ UINT32_C(62500000)
#define AARCH64_SVC_SWITCH UINT32_C(0xA1)
#define A55_CNTP_IRQ UINT32_C(30)
#define A55_GICD_BASE UINT64_C(0x08000000)
#define A55_GICR_BASE UINT64_C(0x080A0000)
#define A55_GICR_SGI_BASE (A55_GICR_BASE + UINT64_C(0x10000))
#define GICD_CTLR UINT32_C(0x0000)
#define GICR_CTLR UINT32_C(0x0000)
#define GICR_IGROUPR0 UINT32_C(0x0080)
#define GICR_ISENABLER0 UINT32_C(0x0100)
#define GICR_ICPENDR0 UINT32_C(0x0280)
#define GICR_IPRIORITYR UINT32_C(0x0400)
#define GIC_CTLR_RWP UINT32_C(1U << 31)
#define GICD_CTLR_ENABLE_GRP0 UINT32_C(1U << 0)
#define GICD_CTLR_ENABLE_GRP1NS UINT32_C(1U << 1)
#define GIC_SPURIOUS_INTID UINT32_C(1020)

extern void aarch64_vectors(void);
extern void aixos_task_return_trap(void);

volatile uint32_t aixos_aarch64_timer_interrupts;
volatile uint32_t aixos_aarch64_last_intid;
volatile uint32_t aixos_aarch64_sync_exceptions;
volatile uint32_t aixos_aarch64_last_esr;
volatile uint32_t aixos_aarch64_schedule_requested;
volatile uint32_t aixos_aarch64_exception_depth;

enum {
    CTX_X0 = 0,
    CTX_X30 = 30,
    CTX_SP = 31,
    CTX_ELR = 32,
    CTX_SPSR = 33,
    CTX_WORDS = 34,
};

static inline uint64_t cntpct_read(void)
{
    uint64_t value;
    __asm volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
}

static inline uint32_t mmio_read32(uint64_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static inline void mmio_write32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline void dsb_sy(void)
{
    __asm volatile("dsb sy" ::: "memory");
}

static inline uint32_t icc_iar1_read(void)
{
    uint64_t value;
    __asm volatile("mrs %0, icc_iar1_el1" : "=r"(value));
    return (uint32_t)value;
}

static inline uint32_t icc_iar0_read(void)
{
    uint64_t value;
    __asm volatile("mrs %0, icc_iar0_el1" : "=r"(value));
    return (uint32_t)value;
}

static inline void icc_eoir1_write(uint32_t intid)
{
    uint64_t value = intid;
    __asm volatile("msr icc_eoir1_el1, %0" :: "r"(value) : "memory");
}

static inline void icc_eoir0_write(uint32_t intid)
{
    uint64_t value = intid;
    __asm volatile("msr icc_eoir0_el1, %0" :: "r"(value) : "memory");
}

static void gic_wait_rwp(uint64_t ctlr_address)
{
    uint32_t i;
    for (i = 0U; i < 100000U; i++) {
        if ((mmio_read32(ctlr_address) & GIC_CTLR_RWP) == 0U) {
            break;
        }
    }
}

static void gicv3_init(void)
{
    uint32_t irq_bit = UINT32_C(1) << A55_CNTP_IRQ;
    uint64_t sre = UINT64_C(7);
    uint64_t priority_mask = UINT64_C(0xFF);
    uint64_t group_enable = UINT64_C(1);
    uint32_t priority_shift = (A55_CNTP_IRQ & UINT32_C(3)) * UINT32_C(8);
    uint32_t priority_offset = GICR_IPRIORITYR + (A55_CNTP_IRQ & ~UINT32_C(3));
    uint32_t priority_word = mmio_read32(A55_GICR_SGI_BASE + priority_offset);

    mmio_write32(A55_GICD_BASE + GICD_CTLR, 0U);
    gic_wait_rwp(A55_GICD_BASE + GICD_CTLR);
    mmio_write32(A55_GICD_BASE + GICD_CTLR,
                 GICD_CTLR_ENABLE_GRP0 | GICD_CTLR_ENABLE_GRP1NS);
    gic_wait_rwp(A55_GICD_BASE + GICD_CTLR);

    mmio_write32(A55_GICR_SGI_BASE + GICR_IGROUPR0, irq_bit);
    mmio_write32(A55_GICR_SGI_BASE + GICR_ICPENDR0, irq_bit);
    priority_word &= ~(UINT32_C(0xFF) << priority_shift);
    priority_word |= UINT32_C(0x80) << priority_shift;
    mmio_write32(A55_GICR_SGI_BASE + priority_offset, priority_word);
    mmio_write32(A55_GICR_SGI_BASE + GICR_ISENABLER0, irq_bit);
    gic_wait_rwp(A55_GICR_BASE + GICR_CTLR);

    __asm volatile("msr icc_sre_el1, %0" :: "r"(sre) : "memory");
    __asm volatile("isb");
    __asm volatile("msr icc_pmr_el1, %0" :: "r"(priority_mask) : "memory");
    __asm volatile("msr icc_igrpen0_el1, %0" :: "r"(group_enable) : "memory");
    __asm volatile("msr icc_igrpen1_el1, %0" :: "r"(group_enable) : "memory");
    __asm volatile("isb");
    dsb_sy();
}

static void timer_program_next(void)
{
    uint64_t interval = A55_TIMER_HZ / AIXOS_CFG_SYSTICK_HZ;
    (void)cntpct_read();
    __asm volatile("msr cntp_tval_el0, %0" :: "r"(interval));
    __asm volatile("msr cntp_ctl_el0, %0" :: "r"(UINT64_C(1)));
}

void aixos_aarch64_irq_entry(uint64_t *frame)
{
    uint32_t intid;
    uint32_t group = 1U;
    (void)frame;
    aixos_aarch64_exception_depth++;
    intid = icc_iar1_read();
    if (intid >= GIC_SPURIOUS_INTID) {
        intid = icc_iar0_read();
        group = 0U;
    }
    aixos_aarch64_last_intid = intid;
    if (intid >= GIC_SPURIOUS_INTID) {
        aixos_aarch64_exception_depth--;
        return;
    }
    aixos_isr_enter();
    if (intid == A55_CNTP_IRQ) {
        aixos_aarch64_timer_interrupts++;
        timer_program_next();
        aixos_tick_handler();
    }
    aixos_isr_exit();
    if (aixos_aarch64_schedule_requested != 0U) {
        aixos_aarch64_schedule_requested = 0U;
        aixos_schedule();
    }
    if (group == 0U) {
        icc_eoir0_write(intid);
    } else {
        icc_eoir1_write(intid);
    }
    aixos_aarch64_exception_depth--;
}

void aixos_aarch64_sync_entry(uint64_t *frame, uint64_t esr)
{
    uint32_t ec = (uint32_t)((esr >> 26) & 0x3FU);
    uint32_t iss = (uint32_t)(esr & 0xFFFFU);
    aixos_aarch64_exception_depth++;
    aixos_aarch64_sync_exceptions++;
    aixos_aarch64_last_esr = (uint32_t)esr;

    if (ec == UINT32_C(0x15)) {
        if (iss == AARCH64_SVC_SWITCH) {
            aixos_aarch64_schedule_requested = 0U;
            aixos_schedule();
        } else {
            const aixos_syscall_request_t *request =
                (const aixos_syscall_request_t *)(uintptr_t)frame[CTX_X0];
            frame[CTX_X0] = (uint64_t)(uint32_t)aixos_syscall_dispatch(request);
        }
    }

    if (aixos_aarch64_schedule_requested != 0U) {
        aixos_aarch64_schedule_requested = 0U;
        aixos_schedule();
    }
    aixos_aarch64_exception_depth--;
}

void aixos_arch_mpu_configure_task(const aixos_tcb_t *task)
{
    (void)task;
}

void *aixos_arch_stack_init(void (*entry)(void *), void *stack_top, void *arg,
                            int user_mode)
{
    uintptr_t aligned_top = (uintptr_t)stack_top & ~(uintptr_t)0xFU;
    uint64_t *frame = (uint64_t *)(aligned_top - CTX_WORDS * sizeof(uint64_t));
    uint32_t i;
    (void)user_mode;

    for (i = 0U; i < CTX_WORDS; i++) {
        frame[i] = 0U;
    }
    frame[CTX_X0] = (uint64_t)(uintptr_t)arg;
    frame[CTX_X30] = (uint64_t)(uintptr_t)aixos_task_return_trap;
    frame[CTX_SP] = (uint64_t)aligned_top;
    frame[CTX_ELR] = (uint64_t)(uintptr_t)entry;
    frame[CTX_SPSR] = UINT64_C(0x5); /* EL1h, interrupts unmasked on return. */
    return frame;
}

void aixos_arch_system_init(void)
{
    uint64_t vector = (uint64_t)(uintptr_t)aarch64_vectors;
    __asm volatile("msr vbar_el1, %0" :: "r"(vector) : "memory");
    __asm volatile("isb");
    aixos_aarch64_timer_interrupts = 0U;
    aixos_aarch64_last_intid = UINT32_MAX;
    aixos_aarch64_sync_exceptions = 0U;
    aixos_aarch64_last_esr = 0U;
    aixos_aarch64_schedule_requested = 0U;
    aixos_aarch64_exception_depth = 0U;
    gicv3_init();
    timer_program_next();
}

void aixos_arch_systick_enable(void)
{
    uint64_t unmasked = 0U;
    __asm volatile("msr daif, %0" :: "r"(unmasked) : "memory");
    __asm volatile("isb");
}

void aixos_arch_tick_handler(void)
{
    aixos_tick_handler();
}

void aixos_arch_context_switch(void)
{
    aixos_aarch64_schedule_requested = 1U;
    if (aixos_aarch64_exception_depth == 0U) {
        __asm volatile("svc %0" :: "I"(AARCH64_SVC_SWITCH) : "memory");
    }
}
