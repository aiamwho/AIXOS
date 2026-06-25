#ifndef AIXOS_CFG_H
#define AIXOS_CFG_H

#include "config/aixos_user_cfg.h"

/*
 * AIXOS v1.0 system configuration.
 * 修改此文件以适配不同硬件和应用需求
 */

/* ── 优先级设置 ──────────────────────────────── */
#ifndef AIXOS_CFG_MAX_PRIORITY
#define AIXOS_CFG_MAX_PRIORITY          256      /* 优先级级数 (0=最低, 255=最高) */
#endif
#ifndef AIXOS_CFG_IDLE_PRIORITY
#define AIXOS_CFG_IDLE_PRIORITY         0        /* Idle 任务优先级 */
#endif
#ifndef AIXOS_CFG_TIMER_TASK_PRIORITY
#define AIXOS_CFG_TIMER_TASK_PRIORITY   63       /* 定时器服务任务优先级 */
#endif
#ifndef AIXOS_CFG_SYSTEM_TASKS_RESERVED
#define AIXOS_CFG_SYSTEM_TASKS_RESERVED 2        /* idle + timer service */
#endif
#ifndef AIXOS_CFG_CAPS_PER_TASK
#define AIXOS_CFG_CAPS_PER_TASK         16
#endif

#ifndef AIXOS_CFG_PROFILE_MINIMAL
#define AIXOS_CFG_PROFILE_MINIMAL       0
#endif

#define AIXOS_CFG_SCHED_BITMAP          1
#define AIXOS_CFG_SCHED_SIMPLE          2
#ifndef AIXOS_CFG_SCHEDULER
#define AIXOS_CFG_SCHEDULER             AIXOS_CFG_SCHED_BITMAP
#endif

#define AIXOS_CFG_PLATFORM_CORTEX_M0    1
#define AIXOS_CFG_PLATFORM_CORTEX_M3    2
#define AIXOS_CFG_PLATFORM_CORTEX_M4    3
#define AIXOS_CFG_PLATFORM_CORTEX_M33   4
#define AIXOS_CFG_PLATFORM_CORTEX_A55   5
#ifndef AIXOS_CFG_PLATFORM
#define AIXOS_CFG_PLATFORM              AIXOS_CFG_PLATFORM_CORTEX_M3
#endif

/* ── 对象池大小 ──────────────────────────────── */
#ifndef AIXOS_CFG_TASK_HANDLE_LIMIT
#define AIXOS_CFG_TASK_HANDLE_LIMIT     256      /* 32-bit handle index capacity */
#endif
#ifndef AIXOS_CFG_TASK_SLOT_PAGE_SIZE
#define AIXOS_CFG_TASK_SLOT_PAGE_SIZE   16       /* 动态任务注册页槽位数 */
#endif
#if AIXOS_CFG_PROFILE_MINIMAL
#ifndef AIXOS_CFG_MAX_SEM
#define AIXOS_CFG_MAX_SEM               4
#endif
#ifndef AIXOS_CFG_MAX_MUTEX
#define AIXOS_CFG_MAX_MUTEX             4
#endif
#ifndef AIXOS_CFG_MAX_MQ
#define AIXOS_CFG_MAX_MQ                4
#endif
#ifndef AIXOS_CFG_MAX_EVENT
#define AIXOS_CFG_MAX_EVENT             4
#endif
#ifndef AIXOS_CFG_MAX_PIPE
#define AIXOS_CFG_MAX_PIPE              2
#endif
#ifndef AIXOS_CFG_MAX_TIMER
#define AIXOS_CFG_MAX_TIMER             8
#endif
#else
#ifndef AIXOS_CFG_MAX_SEM
#define AIXOS_CFG_MAX_SEM               32       /* 最大信号量数 */
#endif
#ifndef AIXOS_CFG_MAX_MUTEX
#define AIXOS_CFG_MAX_MUTEX             16       /* 最大互斥量数 */
#endif
#ifndef AIXOS_CFG_MAX_MQ
#define AIXOS_CFG_MAX_MQ                16       /* 最大消息队列数 */
#endif
#ifndef AIXOS_CFG_MAX_EVENT
#define AIXOS_CFG_MAX_EVENT             16       /* 最大事件标志组数 */
#endif
#ifndef AIXOS_CFG_MAX_PIPE
#define AIXOS_CFG_MAX_PIPE              8        /* 最大管道数 */
#endif
#ifndef AIXOS_CFG_MAX_TIMER
#define AIXOS_CFG_MAX_TIMER             32       /* 最大软件定时器数 */
#endif
#endif

/* ── 调度与时间 ──────────────────────────────── */
#ifndef AIXOS_CFG_TIME_SLICE_TICKS
#define AIXOS_CFG_TIME_SLICE_TICKS      1        /* 时间片 (tick 数) */
#endif
#ifndef AIXOS_CFG_SYSTICK_HZ
#define AIXOS_CFG_SYSTICK_HZ            1000     /* 系统时钟频率 (Hz) */
#endif
#ifndef AIXOS_CFG_CPU_CLOCK_HZ
#define AIXOS_CFG_CPU_CLOCK_HZ           72000000U
#endif

/* ── 中断响应与嵌套 ──────────────────────────── */
#ifndef AIXOS_CFG_KERNEL_IRQ_PRIORITY
#define AIXOS_CFG_KERNEL_IRQ_PRIORITY    0x40U
#endif
#ifndef AIXOS_CFG_SYSTICK_IRQ_PRIORITY
#define AIXOS_CFG_SYSTICK_IRQ_PRIORITY   AIXOS_CFG_KERNEL_IRQ_PRIORITY
#endif
#ifndef AIXOS_CFG_PENDSV_IRQ_PRIORITY
#define AIXOS_CFG_PENDSV_IRQ_PRIORITY    0xF0U
#endif
#ifndef AIXOS_CFG_ISR_NESTING_MAX
#define AIXOS_CFG_ISR_NESTING_MAX        8U
#endif
#ifndef AIXOS_CFG_ISR_NESTING_PANIC
#define AIXOS_CFG_ISR_NESTING_PANIC      0
#endif

/* ── 栈大小 ──────────────────────────────────── */
#ifndef AIXOS_CFG_IDLE_STACK_SIZE
#define AIXOS_CFG_IDLE_STACK_SIZE       256      /* Idle 任务栈 (uint32_t 数量) */
#endif
#ifndef AIXOS_CFG_TIMER_STACK_SIZE
#define AIXOS_CFG_TIMER_STACK_SIZE      512      /* 定时器服务任务栈 */
#endif
#ifndef AIXOS_CFG_DEFAULT_STACK_SIZE
#define AIXOS_CFG_DEFAULT_STACK_SIZE    512      /* 默认用户任务栈 (bytes) */
#endif
#ifndef AIXOS_CFG_MIN_TASK_STACK_SIZE
#define AIXOS_CFG_MIN_TASK_STACK_SIZE   192      /* 覆盖所有支持架构的初始上下文 */
#endif
#ifndef AIXOS_CFG_STACK_GUARD_BYTES
#define AIXOS_CFG_STACK_GUARD_BYTES     16       /* 栈底保护区 */
#endif

/* ── 堆配置 ──────────────────────────────────── */
#ifndef AIXOS_CFG_HEAP_SIZE
#define AIXOS_CFG_HEAP_SIZE             (7 * 1024)      /* 堆大小 (bytes) */
#endif
#ifndef AIXOS_CFG_HEAP_MAGIC
#define AIXOS_CFG_HEAP_MAGIC            0xBE05U   /* 堆完整性魔数 */
#endif
#ifndef AIXOS_CFG_HEAP_LOCK_ON_START
#define AIXOS_CFG_HEAP_LOCK_ON_START    1        /* 启动后禁止新的动态分配 */
#endif

/* ── 跟踪配置 ──────────────────────────────── */
#ifndef AIXOS_CFG_TRACE_ENABLE
#define AIXOS_CFG_TRACE_ENABLE          (!AIXOS_CFG_PROFILE_MINIMAL)
#endif
#if AIXOS_CFG_PROFILE_MINIMAL
#ifndef AIXOS_CFG_TRACE_BUFFER_SIZE
#define AIXOS_CFG_TRACE_BUFFER_SIZE     32
#endif
#else
#ifndef AIXOS_CFG_TRACE_BUFFER_SIZE
#define AIXOS_CFG_TRACE_BUFFER_SIZE     256      /* 跟踪缓冲区条目数 */
#endif
#endif

/* ── CPU 统计 ────────────────────────────────── */
#ifndef AIXOS_CFG_CPU_USAGE_ENABLE
#define AIXOS_CFG_CPU_USAGE_ENABLE      1        /* 启用 CPU 使用统计 */
#endif

/* ── 内核对象命名 ──────────────────────────── */
#ifndef AIXOS_CFG_TASK_NAME_MAX
#define AIXOS_CFG_TASK_NAME_MAX         16       /* 任务名最大长度 */
#endif
#ifndef AIXOS_CFG_TIMER_NAME_MAX
#define AIXOS_CFG_TIMER_NAME_MAX        16       /* 定时器名最大长度 */
#endif

/* ── 分页大小 (用于 heap 内部对齐) ─────────────── */
#ifndef AIXOS_CFG_ALIGNMENT
#define AIXOS_CFG_ALIGNMENT             8        /* 内存分配对齐 */
#endif

/* ── 可靠性配置 ──────────────────────────────── */
#define AIXOS_CFG_SEM_MAX_COUNT         0x7FFFFFFF
#define AIXOS_CFG_TIMEOUT_MAX_TICKS     0x7FFFFFFFU
#define AIXOS_CFG_CRASH_MAGIC           0x42434F53U /* "BCOS" */
#define AIXOS_CFG_MAX_IPC_COPY_BYTES    1024U
#define AIXOS_CFG_ISR_COPY_MAX_BYTES    64U
#define AIXOS_CFG_ISR_MQ_SHIFT_MAX      2U
#define AIXOS_CFG_MQ_PRIORITY_MAX       31U
#if AIXOS_CFG_PROFILE_MINIMAL
#define AIXOS_CFG_POSIX_KEYS            4U
#define AIXOS_CFG_POSIX_RWLOCK_READERS  4U
#define AIXOS_CFG_POSIX_OPEN_MAX        8U
#define AIXOS_CFG_POSIX_TIMERS          4U
#else
#define AIXOS_CFG_POSIX_KEYS            16U
#define AIXOS_CFG_POSIX_RWLOCK_READERS  16U
#define AIXOS_CFG_POSIX_OPEN_MAX        32U
#define AIXOS_CFG_POSIX_TIMERS          16U
#endif
#define AIXOS_CFG_POSIX_PIPE_CAPACITY   256U

#if AIXOS_CFG_MAX_PRIORITY < 2 || AIXOS_CFG_MAX_PRIORITY > 256
#error "AIXOS_CFG_MAX_PRIORITY must be in [2, 256]"
#endif

#if AIXOS_CFG_SCHEDULER != AIXOS_CFG_SCHED_BITMAP && \
    AIXOS_CFG_SCHEDULER != AIXOS_CFG_SCHED_SIMPLE
#error "AIXOS_CFG_SCHEDULER must be AIXOS_CFG_SCHED_BITMAP or AIXOS_CFG_SCHED_SIMPLE"
#endif

#if AIXOS_CFG_PLATFORM != AIXOS_CFG_PLATFORM_CORTEX_M0 && \
    AIXOS_CFG_PLATFORM != AIXOS_CFG_PLATFORM_CORTEX_M3 && \
    AIXOS_CFG_PLATFORM != AIXOS_CFG_PLATFORM_CORTEX_M4 && \
    AIXOS_CFG_PLATFORM != AIXOS_CFG_PLATFORM_CORTEX_M33 && \
    AIXOS_CFG_PLATFORM != AIXOS_CFG_PLATFORM_CORTEX_A55
#error "AIXOS_CFG_PLATFORM must select one supported platform"
#endif

#if AIXOS_CFG_IDLE_PRIORITY != 0
#error "The current scheduler requires idle priority 0"
#endif

#if AIXOS_CFG_TIMER_TASK_PRIORITY >= AIXOS_CFG_MAX_PRIORITY
#error "Timer task priority is outside the configured range"
#endif

#if AIXOS_CFG_SYSTEM_TASKS_RESERVED < 2 || \
    AIXOS_CFG_SYSTEM_TASKS_RESERVED >= AIXOS_CFG_TASK_HANDLE_LIMIT
#error "Reserve at least idle and timer task slots"
#endif

#if AIXOS_CFG_TASK_HANDLE_LIMIT != 256
#error "Current 8-bit task handle index requires a capacity of 256"
#endif

#if AIXOS_CFG_TASK_SLOT_PAGE_SIZE == 0 || \
    AIXOS_CFG_TASK_SLOT_PAGE_SIZE > 32 || \
    (AIXOS_CFG_TASK_HANDLE_LIMIT % AIXOS_CFG_TASK_SLOT_PAGE_SIZE) != 0
#error "Task slot page size must divide the handle capacity and be <= 32"
#endif

#if AIXOS_CFG_SYSTICK_HZ == 0 || AIXOS_CFG_CPU_CLOCK_HZ == 0
#error "Clock frequencies must be non-zero"
#endif

#if AIXOS_CFG_TIME_SLICE_TICKS == 0
#error "Time slice must be at least one tick"
#endif

#if AIXOS_CFG_KERNEL_IRQ_PRIORITY == 0 || \
    AIXOS_CFG_KERNEL_IRQ_PRIORITY > 0xFFU
#error "Kernel IRQ priority threshold must be in [1, 255]"
#endif

#if AIXOS_CFG_SYSTICK_IRQ_PRIORITY > 0xFFU || \
    AIXOS_CFG_PENDSV_IRQ_PRIORITY > 0xFFU
#error "Cortex-M exception priorities must fit in 8 bits"
#endif

#if AIXOS_CFG_SYSTICK_IRQ_PRIORITY < AIXOS_CFG_KERNEL_IRQ_PRIORITY
#error "SysTick priority must be maskable by the kernel IRQ threshold"
#endif

#if AIXOS_CFG_PENDSV_IRQ_PRIORITY < AIXOS_CFG_SYSTICK_IRQ_PRIORITY
#error "PendSV priority must be no more urgent than SysTick"
#endif

#if AIXOS_CFG_ISR_NESTING_MAX == 0 || AIXOS_CFG_ISR_NESTING_MAX > 255U
#error "ISR nesting max must be in [1, 255]"
#endif

#if AIXOS_CFG_MIN_TASK_STACK_SIZE < 192
#error "AIXOS_CFG_MIN_TASK_STACK_SIZE cannot hold the largest initial context"
#endif

#if AIXOS_CFG_STACK_GUARD_BYTES < 8 || \
    (AIXOS_CFG_STACK_GUARD_BYTES % AIXOS_CFG_ALIGNMENT) != 0
#error "Stack guard must be at least 8 bytes and naturally aligned"
#endif

#if AIXOS_CFG_DEFAULT_STACK_SIZE < AIXOS_CFG_MIN_TASK_STACK_SIZE || \
    AIXOS_CFG_TIMER_STACK_SIZE < AIXOS_CFG_MIN_TASK_STACK_SIZE || \
    (AIXOS_CFG_IDLE_STACK_SIZE * 4U) < AIXOS_CFG_MIN_TASK_STACK_SIZE
#error "Configured system/default stacks are below the architecture minimum"
#endif

#if AIXOS_CFG_HEAP_SIZE < 1024 || \
    (AIXOS_CFG_HEAP_SIZE % AIXOS_CFG_ALIGNMENT) != 0
#error "Heap must be at least 1 KiB and naturally aligned"
#endif

#if AIXOS_CFG_TRACE_ENABLE && AIXOS_CFG_TRACE_BUFFER_SIZE == 0
#error "Trace buffer cannot be empty when trace is enabled"
#endif

#if AIXOS_CFG_MAX_IPC_COPY_BYTES == 0
#error "IPC copy limit must be non-zero"
#endif

#if AIXOS_CFG_ISR_COPY_MAX_BYTES == 0 || \
    AIXOS_CFG_ISR_COPY_MAX_BYTES > AIXOS_CFG_MAX_IPC_COPY_BYTES
#error "ISR copy limit must be in the regular IPC copy range"
#endif

#if AIXOS_CFG_ISR_MQ_SHIFT_MAX > AIXOS_CFG_MAX_MQ
#error "ISR priority queue shift limit cannot exceed queue capacity"
#endif

#if AIXOS_CFG_POSIX_RWLOCK_READERS == 0
#error "POSIX rwlock reader ownership table cannot be empty"
#endif

#if AIXOS_CFG_POSIX_TIMERS == 0 || AIXOS_CFG_POSIX_TIMERS > 255
#error "POSIX timer pool must contain between 1 and 255 timers"
#endif

#if AIXOS_CFG_CAPS_PER_TASK < 4 || AIXOS_CFG_CAPS_PER_TASK > 32
#error "Capability table must contain between 4 and 32 entries"
#endif

/* ── Optional subsystem configuration ───────── */
#ifndef AIXOS_CFG_ENABLE_SIGNALS
#define AIXOS_CFG_ENABLE_SIGNALS       1       /* 启用信号机制 */
#endif
#ifndef AIXOS_CFG_ENABLE_NAMESPACE
#define AIXOS_CFG_ENABLE_NAMESPACE     1       /* 启用资源管理器命名空间 */
#endif
#ifndef AIXOS_CFG_ENABLE_TIME_WHEEL
#define AIXOS_CFG_ENABLE_TIME_WHEEL    1       /* 启用时间轮 */
#endif
#ifndef AIXOS_CFG_ENABLE_PANIC_RESET
#define AIXOS_CFG_ENABLE_PANIC_RESET   1       /* 启用panic受控复位 */
#endif
#ifndef AIXOS_CFG_ENABLE_MPU
#define AIXOS_CFG_ENABLE_MPU           1       /* Enable user-task memory protection */
#endif
#ifndef AIXOS_CFG_MPU_REGIONS_PER_TASK
#define AIXOS_CFG_MPU_REGIONS_PER_TASK 3       /* Per-task user memory regions */
#endif
#ifndef AIXOS_CFG_MPU_MIN_REGION_SIZE
#define AIXOS_CFG_MPU_MIN_REGION_SIZE  32U     /* Cortex-M MPU minimum region */
#endif

#define AIXOS_CFG_FLASH_BASE           0x08000000U
#define AIXOS_CFG_FLASH_SIZE           (512U * 1024U)
#define AIXOS_CFG_RAM_BASE             0x20000000U
#define AIXOS_CFG_RAM_SIZE             (64U * 1024U)
#define AIXOS_CFG_RISCV_RAM_BASE       0x80000000U
#define AIXOS_CFG_RISCV_RAM_SIZE       (64U * 1024U * 1024U)

#if AIXOS_CFG_ENABLE_TIME_WHEEL
#define AIXOS_CFG_TW_BITS1             6       /* 时间轮第一层位宽 (64槽) */
#define AIXOS_CFG_TW_BITS2             6       /* 时间轮第二层位宽 (64槽) */
#define AIXOS_CFG_TW_SIZE1             (1U << AIXOS_CFG_TW_BITS1)
#define AIXOS_CFG_TW_SIZE2             (1U << AIXOS_CFG_TW_BITS2)
#endif

#define AIXOS_CFG_NAMESPACE_MAX        32      /* 命名空间最大条目 */
#define AIXOS_CFG_SIGNAL_QUEUE_DEPTH   8       /* 信号队列深度 */
#define AIXOS_CFG_MAX_SIGNAL_HANDLERS  32      /* 最大信号处理函数数 */

#if AIXOS_CFG_ENABLE_MPU && \
    (AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
     AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4) && \
    AIXOS_CFG_MPU_REGIONS_PER_TASK == 0
#error "MPU must provide at least one per-task region"
#endif

#if AIXOS_CFG_ENABLE_MPU && \
    (AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
     AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4) && \
    AIXOS_CFG_MPU_REGIONS_PER_TASK > 3
#error "Portable MPU profile supports at most three per-task regions"
#endif

#if (AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
     AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4) && \
    (AIXOS_CFG_MPU_MIN_REGION_SIZE < 32U || \
    (AIXOS_CFG_MPU_MIN_REGION_SIZE & (AIXOS_CFG_MPU_MIN_REGION_SIZE - 1U)) != 0U)
#error "MPU minimum region size must be a power of two and at least 32 bytes"
#endif

#endif /* AIXOS_CFG_H */
