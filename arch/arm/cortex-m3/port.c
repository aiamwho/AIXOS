#include "aixos/arch/arch.h"
#include "aixos/types.h"
#include "aixos/task.h"
#include "aixos/mpu.h"
#include "aixos/microkernel.h"
#include "config/aixos_cfg.h"

extern void aixos_task_return_trap(void);

void aixos_arm_svc_handler(uint32_t *frame)
{
    const aixos_syscall_request_t *request;
    if (frame == NULL) {
        return;
    }
    request = (const aixos_syscall_request_t *)(uintptr_t)frame[0];
    frame[0] = (uint32_t)aixos_syscall_dispatch(request);
}

/* SysTick 寄存器 */
#define SYSTICK_CSR     (*(volatile uint32_t *)0xE000E010)
#define SYSTICK_RVR     (*(volatile uint32_t *)0xE000E014)
#define SYSTICK_CVR     (*(volatile uint32_t *)0xE000E018)
#define SYSTICK_CSR_EN     (1 << 0)
#define SYSTICK_CSR_TICK   (1 << 1)
#define SYSTICK_CSR_CLK    (1 << 2)

/* NVIC SHPR3: PendSV + SysTick 优先级 */
#define NVIC_SHPR3      (*(volatile uint32_t *)0xE000ED20)
#define SCB_SHCSR       (*(volatile uint32_t *)0xE000ED24)
#define SCB_SHCSR_FAULT_ENABLE ((1U << 16) | (1U << 17) | (1U << 18))

#define MPU_TYPE        (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL        (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR         (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR        (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR        (*(volatile uint32_t *)0xE000EDA0)
#define MPU_CTRL_ENABLE     (1U << 0)
#define MPU_CTRL_PRIVDEFENA (1U << 2)
#define MPU_RASR_ENABLE     (1U << 0)
#define MPU_RASR_XN         (1U << 28)
#define MPU_RASR_AP_RO      (6U << 24)
#define MPU_RASR_AP_RW      (3U << 24)
#define MPU_RASR_NORMAL     ((1U << 18) | (1U << 17) | (1U << 16))
#define MPU_RASR_DEVICE     (1U << 16)

/* 外部汇编函数 */
extern void aixos_asm_int_disable(void);
extern void aixos_asm_int_restore(uint32_t flags);

#if AIXOS_CFG_ENABLE_MPU && \
    (AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
     AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4)
static uint32_t mpu_log2(uint32_t value)
{
    uint32_t log = 0U;
    while (value > 1U) {
        value >>= 1;
        log++;
    }
    return log;
}

static uint32_t mpu_rasr_size(uint32_t size)
{
    return (mpu_log2(size) - 1U) << 1;
}

static void mpu_region_disable(uint32_t region)
{
    MPU_RNR = region;
    MPU_RASR = 0U;
}

static void mpu_region_configure(uint32_t region, uintptr_t base,
                                 uint32_t size, uint32_t attr)
{
    uint32_t rasr = MPU_RASR_ENABLE | mpu_rasr_size(size);
    MPU_RNR = region;
    MPU_RBAR = (uint32_t)base;
    if ((attr & AIXOS_MPU_WRITE) != 0U) {
        rasr |= MPU_RASR_AP_RW;
    } else {
        rasr |= MPU_RASR_AP_RO;
    }
    if ((attr & AIXOS_MPU_EXEC) == 0U) {
        rasr |= MPU_RASR_XN;
    }
    rasr |= (attr & AIXOS_MPU_DEVICE) != 0U ? MPU_RASR_DEVICE : MPU_RASR_NORMAL;
    MPU_RASR = rasr;
}
#endif

void aixos_arch_mpu_configure_task(const aixos_tcb_t *task)
{
#if AIXOS_CFG_ENABLE_MPU && \
    (AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
     AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4)
    uint32_t i;
    uint32_t dynamic_regions;
    if ((MPU_TYPE & 0xFF00U) == 0U) {
        return;
    }
    MPU_CTRL = 0U;
    for (i = 0U; i < 8U; i++) {
        mpu_region_disable(i);
    }
    mpu_region_configure(0U, AIXOS_CFG_FLASH_BASE, AIXOS_CFG_FLASH_SIZE,
                         AIXOS_MPU_READ | AIXOS_MPU_EXEC);
    dynamic_regions = task != NULL &&
                      task->domain == AIXOS_DOMAIN_USER ?
                      task->mpu_region_count : 0U;
    if (dynamic_regions > AIXOS_CFG_MPU_REGIONS_PER_TASK) {
        dynamic_regions = AIXOS_CFG_MPU_REGIONS_PER_TASK;
    }
    for (i = 0U; i < dynamic_regions; i++) {
        mpu_region_configure(i + 1U, task->mpu_regions[i].base,
                             task->mpu_regions[i].size,
                             task->mpu_regions[i].attr);
    }
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile("dsb\nisb" ::: "memory");
#else
    (void)task;
#endif
}

/*
 * Cortex-M 栈帧初始化
 * 模拟异常自动入栈 + 手动入栈 R4-R11
 * Cortex-M0 栈布局 (低→高): R8..R11, R4..R7, R0..xPSR
 * Cortex-M3+ 栈布局 (低→高): R4..R11, R0..xPSR
 * entry: 任务函数地址; stack_top: 栈顶(高地址); arg: 第一个参数(R0)
 * 返回: 栈指针(指向手动保存上下文的最低地址)
 */
void *aixos_arch_stack_init(void (*entry)(void*), void *stack_top, void *arg,
                            int user_mode)
{
    uint32_t *stk = (uint32_t *)stack_top;
    uint32_t task_addr = (uint32_t)entry;
    (void)user_mode;

    /* 异常自动入栈 (从高到低) */
    *(--stk) = 0x01000000;               /* xPSR (Thumb 位) */
    *(--stk) = (uint32_t)task_addr;      /* PC (任务入口) */
    *(--stk) = (uint32_t)aixos_task_return_trap; /* LR if task returns */
    *(--stk) = 0x0C;                     /* R12 */
    *(--stk) = 0x0B;                     /* R3 */
    *(--stk) = 0x0A;                     /* R2 */
    *(--stk) = 0x09;                     /* R1 */
    *(--stk) = (uint32_t)arg;            /* R0 (入口参数) */

#if AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M0
    /* Cortex-M0 restores high registers through low-register temporaries. */
    *(--stk) = 0x04;                     /* R7 */
    *(--stk) = 0x03;                     /* R6 */
    *(--stk) = 0x02;                     /* R5 */
    *(--stk) = 0x01;                     /* R4 */
    *(--stk) = 0x08;                     /* R11 */
    *(--stk) = 0x07;                     /* R10 */
    *(--stk) = 0x06;                     /* R9 */
    *(--stk) = 0x05;                     /* R8 */
#else
    /* 手动保存 R4-R11 */
    *(--stk) = 0x08;                     /* R11 */
    *(--stk) = 0x07;                     /* R10 */
    *(--stk) = 0x06;                     /* R9 */
    *(--stk) = 0x05;                     /* R8 */
    *(--stk) = 0x04;                     /* R7 */
    *(--stk) = 0x03;                     /* R6 */
    *(--stk) = 0x02;                     /* R5 */
    *(--stk) = 0x01;                     /* R4 */
#endif

    return (void *)stk;
}

void aixos_arch_system_init(void)
{
    SYSTICK_RVR = (AIXOS_CFG_CPU_CLOCK_HZ / AIXOS_CFG_SYSTICK_HZ) - 1U;
    SYSTICK_CVR = 0;
    SYSTICK_CSR = 0;

    /* PendSV lowest, SysTick remains maskable by the BASEPRI threshold. */
    NVIC_SHPR3 = (AIXOS_CFG_PENDSV_IRQ_PRIORITY << 16) |
                 (AIXOS_CFG_SYSTICK_IRQ_PRIORITY << 24);
#if AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
    AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4
    SCB_SHCSR |= SCB_SHCSR_FAULT_ENABLE;
#endif
    aixos_arch_mpu_configure_task(NULL);
}

void aixos_arch_systick_enable(void)
{
    SYSTICK_CSR = SYSTICK_CSR_EN | SYSTICK_CSR_TICK | SYSTICK_CSR_CLK;
}
