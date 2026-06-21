#ifndef AIXOS_TASK_H
#define AIXOS_TASK_H

#include "aixos/types.h"
#include "aixos/mpu.h"
#include "kernel/list.h"
#include "config/aixos_cfg.h"

/* 任务控制块 */
typedef struct aixos_tcb {
    /* Must stay first: architecture assembly accesses offset 0. */
    void             *stack_top;
    uint32_t          arch_control;

    /* 链表节点 */
    aixos_list_t      ready_node;       /* 就绪链表节点 */
    aixos_list_t      all_node;         /* 全局任务链表节点 */
    aixos_list_t      wait_node;        /* IPC 等待链表节点 */
    aixos_list_t      timeout_node;     /* 超时链表节点 */
    aixos_list_t      held_mutexes;     /* 当前持有的互斥量 */
    
    /* 任务基本信息 */
    char              name[16];         
    int               priority;
    aixos_task_state_t state;
    aixos_handle_t    handle;
    
    /* 栈 */
    void             *stack_base;       /* 栈基址 (用于 free) */
    void             *stack_alloc_base; /* Allocator-owned stack pointer */
    uint32_t          stack_size;       /* 栈大小 */
    
    /* 调度统计 */
    uint32_t          time_slice;       /* 剩余时间片 (ticks) */
    uint64_t          runtime_ticks;    /* 总运行 tick 数 */
    uint64_t          switch_count;     /* 被切换次数 */
    
    /* IPC 等待信息 */
    void             *wait_obj;         /* 等待的内核对象指针 */
    aixos_list_t     *wait_list;        /* 所属对象等待队列 */
    aixos_obj_type_t  wait_type;        /* 等待的对象类型 */
    int               wait_result;      /* 等待结果 */
    uint32_t          wake_tick;        /* 延时/超时到期 tick */
    
    /* 互斥量优先级继承 */
    int               base_priority;    /* 用户配置优先级 */
    
    /* 内核内部使用 */
    uint32_t          pend_mask;        /* 事件标志等待掩码 */
    uint32_t          pend_result;      /* 事件标志匹配结果 */
    uint8_t           pend_mode;        /* 事件标志等待模式 */
    uint8_t           owns_tcb;
    uint8_t           owns_stack;
    uint8_t           stack_guard_failed;
    uint8_t           notify_pending;
    uint8_t           domain;
    uint8_t           faulted;
    uint8_t           reserved;
    uint32_t          notify_value;
    struct {
        aixos_handle_t object;
        uint16_t rights;
        uint8_t type;
        uint8_t used;
    } capabilities[AIXOS_CFG_CAPS_PER_TASK];
    int               posix_errno;
    void             *posix_values[AIXOS_CFG_POSIX_KEYS];
    uint32_t          posix_key_generations[AIXOS_CFG_POSIX_KEYS];

    uint32_t          signal_pending;    // 待处理信号位图
    uint32_t          signal_mask;       // 信号屏蔽字
    void             *signal_handler[32]; // 信号处理函数
    void             *signal_arg[32];     // 信号处理函数参数
    int               sched_policy;      // AIXOS_SCHED_FIFO 或 AIXOS_SCHED_RR
    uint32_t          sched_priority;    // 运行时调度优先级(含继承)
    uint8_t           sched_boosted;     // 是否被优先级继承提升
    uint8_t           pending_signal;    // 信号需递送标志
    uint16_t          reserved2;
    void             *sched_wait_obj;    // 调度等待对象(用于继承追踪)
    uint8_t           mpu_region_count;
    uint8_t           mpu_reserved[3];
    aixos_mpu_region_t mpu_regions[AIXOS_CFG_MPU_REGIONS_PER_TASK];
} aixos_tcb_t;

/* 任务 API (POSIX 风格) */
aixos_handle_t aixos_task_create(
    const char *name,
    void (*entry)(void*),
    void *arg,
    size_t stack_size,
    int priority
);
aixos_handle_t aixos_user_task_create(
    const char *name,
    void (*entry)(void *),
    void *arg,
    size_t stack_size,
    int priority
);
aixos_handle_t aixos_task_create_static(
    const char *name,
    void (*entry)(void *),
    void *arg,
    void *stack,
    size_t stack_size,
    int priority,
    aixos_tcb_t *tcb
);

int  aixos_task_delete(aixos_handle_t task);
int  aixos_task_sleep(uint32_t ms);
int  aixos_task_yield(void);
aixos_handle_t aixos_task_self(void);
int  aixos_task_suspend(aixos_handle_t task);
int  aixos_task_resume(aixos_handle_t task);
int  aixos_task_set_priority(aixos_handle_t task, int priority);
int  aixos_task_get_info(aixos_handle_t task, aixos_task_info_t *info);
int  aixos_task_stack_check(aixos_handle_t task);
int  aixos_task_is_user(aixos_handle_t task);

int  aixos_sched_lock(void);
int  aixos_sched_unlock(void);
uint32_t aixos_sched_lock_count(void);
int  aixos_sched_is_locked(void);

/* 内部接口 (供调度器和架构层调用) */
int  aixos_task_init(void);
aixos_tcb_t *aixos_tcb_from_handle(aixos_handle_t h);
void aixos_task_tick(uint32_t now);
int aixos_task_block_current(aixos_list_t *wait_list, void *wait_obj,
                             aixos_obj_type_t wait_type, uint32_t timeout_ms,
                             uint32_t interrupt_flags);
void aixos_task_wake(aixos_tcb_t *tcb, int result);
void aixos_task_waiter_priority_changed(aixos_tcb_t *tcb);
void aixos_start(void) __attribute__((noreturn));
void aixos_task_return_trap(void) __attribute__((noreturn));
uint32_t aixos_task_count(void);
uint32_t aixos_ms_to_ticks(uint32_t ms);
uint32_t aixos_timeout_remaining_ms(uint32_t start_tick, uint32_t timeout_ms);

int  aixos_task_signal_send(aixos_handle_t task, uint32_t signo);
int  aixos_task_signal_handle(uint32_t signo, void (*handler)(void *), void *arg);
int  aixos_task_signal_mask(uint32_t mask);
int  aixos_task_signal_pending(aixos_handle_t task, uint32_t *pending);

int  aixos_task_signal_deliver(void);
void aixos_task_wake_recheck(aixos_tcb_t *tcb, int result);
void aixos_panic(const char *msg, uint32_t reason);
void aixos_system_reset(void);
int  aixos_pool_create(const char *name, void *pool_mem, size_t obj_size, int count);

#endif /* AIXOS_TASK_H */
