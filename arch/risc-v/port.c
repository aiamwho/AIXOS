#include "aixos/arch/arch.h"
#include "aixos/task.h"
#include "aixos/mpu.h"
#include "aixos/microkernel.h"
#include "config/aixos_cfg.h"
#include "kernel/sched.h"
#include <stdint.h>

#ifndef AIXOS_RISCV_CLINT_BASE
#define AIXOS_RISCV_CLINT_BASE UINT32_C(0x02000000)
#endif

#ifndef AIXOS_RISCV_TIMEBASE_HZ
#define AIXOS_RISCV_TIMEBASE_HZ UINT32_C(10000000)
#endif

#define CLINT_MSIP          (*(volatile uint32_t *)(AIXOS_RISCV_CLINT_BASE))
#define CLINT_MTIMECMP_LO   (*(volatile uint32_t *)(AIXOS_RISCV_CLINT_BASE + 0x4000U))
#define CLINT_MTIMECMP_HI   (*(volatile uint32_t *)(AIXOS_RISCV_CLINT_BASE + 0x4004U))
#define CLINT_MTIME_LO      (*(volatile uint32_t *)(AIXOS_RISCV_CLINT_BASE + 0xBFF8U))
#define CLINT_MTIME_HI      (*(volatile uint32_t *)(AIXOS_RISCV_CLINT_BASE + 0xBFFCU))

#define MSTATUS_MPP_M   (UINT32_C(3) << 11)
#define MSTATUS_MPIE    (UINT32_C(1) << 7)
#define MIE_MSIE        (UINT32_C(1) << 3)
#define MIE_MTIE        (UINT32_C(1) << 7)
#define PMP_R           UINT32_C(0x01)
#define PMP_W           UINT32_C(0x02)
#define PMP_X           UINT32_C(0x04)
#define PMP_A_NAPOT     UINT32_C(0x18)

extern void trap_handler(void);
extern void aixos_task_return_trap(void);

volatile uint32_t aixos_riscv_timer_interrupts;
volatile uint32_t aixos_riscv_software_interrupts;
volatile uint32_t aixos_riscv_unhandled_mcause;
volatile uint32_t aixos_riscv_unhandled_mepc;
static uint64_t next_timer_deadline;

void aixos_riscv_syscall_entry(void)
{
    uint32_t *frame;
    const aixos_syscall_request_t *request;
    if (g_cur_task == NULL) {
        return;
    }
    frame = (uint32_t *)g_cur_task->stack_top;
    request = (const aixos_syscall_request_t *)(uintptr_t)frame[10];
    frame[10] = (uint32_t)aixos_syscall_dispatch(request);
    frame[32] += 4U;
}

typedef char aixos_riscv_stack_top_must_be_first[
    offsetof(aixos_tcb_t, stack_top) == 0 ? 1 : -1
];

static uint64_t clint_read_mtime(void)
{
    uint32_t high;
    uint32_t low;
    uint32_t high_check;
    do {
        high = CLINT_MTIME_HI;
        low = CLINT_MTIME_LO;
        high_check = CLINT_MTIME_HI;
    } while (high != high_check);
    return ((uint64_t)high << 32) | low;
}

static void clint_write_mtimecmp(uint64_t value)
{
    /*
     * The privileged specification recommends this sequence on RV32 to
     * avoid a transient compare value below mtime.
     */
    CLINT_MTIMECMP_LO = UINT32_MAX;
    CLINT_MTIMECMP_HI = (uint32_t)(value >> 32);
    CLINT_MTIMECMP_LO = (uint32_t)value;
}

static uint32_t pmp_napot(uintptr_t base, uint32_t size)
{
    return (uint32_t)((base >> 2) | (((uintptr_t)size - 1U) >> 3));
}

static uint32_t pmp_attr(uint32_t attr)
{
    uint32_t cfg = PMP_A_NAPOT;
    if ((attr & AIXOS_MPU_READ) != 0U) {
        cfg |= PMP_R;
    }
    if ((attr & AIXOS_MPU_WRITE) != 0U) {
        cfg |= PMP_W;
    }
    if ((attr & AIXOS_MPU_EXEC) != 0U) {
        cfg |= PMP_X;
    }
    return cfg;
}

void aixos_arch_mpu_configure_task(const aixos_tcb_t *task)
{
#if AIXOS_CFG_ENABLE_MPU
    uint32_t cfg = 0U;
    uint32_t i;
    uint32_t dynamic_regions;
    __asm volatile("csrw pmpcfg0, zero");
    __asm volatile("csrw pmpaddr0, zero");
    __asm volatile("csrw pmpaddr1, zero");
    __asm volatile("csrw pmpaddr2, zero");
    __asm volatile("csrw pmpaddr3, zero");

    dynamic_regions = task != NULL &&
                      task->domain == AIXOS_DOMAIN_USER ?
                      task->mpu_region_count : 0U;
    if (dynamic_regions > 3U) {
        dynamic_regions = 3U;
    }
    for (i = 0U; i < dynamic_regions; i++) {
        uint32_t encoded = pmp_napot(task->mpu_regions[i].base,
                                     task->mpu_regions[i].size);
        uint32_t region_cfg = pmp_attr(task->mpu_regions[i].attr);
        if (i == 0U) {
            __asm volatile("csrw pmpaddr0, %0" :: "r"(encoded));
            cfg |= region_cfg;
        } else if (i == 1U) {
            __asm volatile("csrw pmpaddr1, %0" :: "r"(encoded));
            cfg |= region_cfg << 8;
        } else {
            __asm volatile("csrw pmpaddr2, %0" :: "r"(encoded));
            cfg |= region_cfg << 16;
        }
    }
    __asm volatile("csrw pmpaddr3, %0" ::
                   "r"(pmp_napot(AIXOS_CFG_RISCV_RAM_BASE,
                                  AIXOS_CFG_RISCV_RAM_SIZE)));
    cfg |= pmp_attr(AIXOS_MPU_READ | AIXOS_MPU_EXEC) << 24;
    __asm volatile("csrw pmpcfg0, %0" :: "r"(cfg) : "memory");
#else
    (void)task;
#endif
}

void *aixos_arch_stack_init(void (*entry)(void *), void *stack_top, void *arg,
                            int user_mode)
{
    uintptr_t aligned_top = (uintptr_t)stack_top & ~(uintptr_t)0xFU;
    uint32_t *frame = (uint32_t *)(aligned_top - 36U * sizeof(uint32_t));
    unsigned int i;
    for (i = 0; i < 36U; i++) {
        frame[i] = 0;
    }
    frame[1] = (uint32_t)(uintptr_t)aixos_task_return_trap;
    frame[2] = (uint32_t)aligned_top;
    frame[10] = (uint32_t)(uintptr_t)arg;
    frame[32] = (uint32_t)(uintptr_t)entry;
    frame[33] = (user_mode ? 0U : MSTATUS_MPP_M) | MSTATUS_MPIE;
    return frame;
}

void aixos_arch_system_init(void)
{
    uintptr_t vector = (uintptr_t)trap_handler;
    uint32_t interrupts = MIE_MSIE | MIE_MTIE;
    uint32_t disabled = UINT32_C(8);
    __asm volatile("csrc mstatus, %0" :: "r"(disabled) : "memory");
    __asm volatile("csrw mtvec, %0" :: "r"(vector));
    aixos_arch_mpu_configure_task(NULL);
    __asm volatile("csrs mie, %0" :: "r"(interrupts));
    CLINT_MSIP = 0;
    aixos_riscv_timer_interrupts = 0;
    aixos_riscv_software_interrupts = 0;
    aixos_riscv_unhandled_mcause = 0;
    aixos_riscv_unhandled_mepc = 0;
    next_timer_deadline = 0U;
    aixos_arch_timer_ack();
}

void aixos_arch_systick_enable(void)
{
    uint32_t enabled = MIE_MSIE | MIE_MTIE;
    __asm volatile("csrs mie, %0" :: "r"(enabled) : "memory");
}

void aixos_arch_timer_ack(void)
{
    uint64_t interval = AIXOS_RISCV_TIMEBASE_HZ / AIXOS_CFG_SYSTICK_HZ;
    uint64_t now = clint_read_mtime();
    if (next_timer_deadline == 0U) {
        next_timer_deadline = now + interval;
    } else {
        next_timer_deadline += interval;
        if (next_timer_deadline <= now) {
            uint64_t missed = (now - next_timer_deadline) / interval + 1U;
            next_timer_deadline += missed * interval;
        }
    }
    clint_write_mtimecmp(next_timer_deadline);
}

void aixos_arch_software_ack(void)
{
    CLINT_MSIP = 0;
}

void aixos_arch_trigger_switch(void)
{
    CLINT_MSIP = 1;
}
