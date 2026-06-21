/*
 * AIXOS smoke-test application.
 * Demonstrates task creation, semaphore, message queue, event flag, pipe,
 * software timer, and user-mode syscall paths.
 */

#include "aixos/aixos.h"
#include "aixos/namespace.h"
#include "kernel/timewheel.h"
#include "arch/include/aixos/arch/arch.h"


/* ── 任务栈 ──────────────────────────────────── */
#define TASK_STACK_SIZE  512

/* ── 测试用 IPC 句柄 ──────────────────────────── */
static aixos_handle_t g_sem;
static aixos_handle_t g_mq;
static aixos_handle_t g_evt;
static aixos_handle_t g_mutex;
static aixos_handle_t g_pipe;
static aixos_handle_t g_timer;

/* ── 定时器回调 ──────────────────────────────── */
/* Timer callbacks execute in timer service task context, not ISR context. */
static void timer1_callback(void *arg)
{
    (void)arg;
    /* 在 timer_service 任务上下文中执行，可以安全调用阻塞 API */
}

/* ── Task 1: 信号量发信者 ────────────────────── */
/* 任务运行验证计数器 */
volatile uint32_t test_heartbeat = 0;
volatile uint32_t test_riscv_register_errors = 0;
volatile uint32_t test_user_heartbeat __attribute__((aligned(32))) = 0;
volatile uint32_t test_user_syscall_errors __attribute__((aligned(32))) = 0;
static uint8_t task_stacks[5][TASK_STACK_SIZE] __attribute__((aligned(8)));
static aixos_tcb_t task_tcbs[5];
static uint8_t user_task_stack[512] __attribute__((aligned(512)));
static aixos_tcb_t user_task_tcb;

static void user_task_entry(void *arg)
{
    uint32_t previous = 0U;
    (void)arg;
    for (;;) {
        uint32_t now = aixos_user_clock_get();
        if (now < previous || aixos_user_task_self() == AIXOS_HANDLE_INVALID) {
            test_user_syscall_errors++;
        }
        previous = now;
        test_user_heartbeat++;
        if (aixos_user_sleep(7U) != AIXOS_OK) {
            test_user_syscall_errors++;
        }
    }
}

#if defined(__riscv)
static void riscv_register_stress(void)
{
    register uint32_t s2 __asm("s2") = UINT32_C(0x11223344);
    register uint32_t s3 __asm("s3") = UINT32_C(0x55667788);
    register uint32_t s4 __asm("s4") = UINT32_C(0xA5A55A5A);
    __asm volatile("" : "+r"(s2), "+r"(s3), "+r"(s4));
    aixos_task_sleep(2);
    __asm volatile("" : "+r"(s2), "+r"(s3), "+r"(s4));
    if (s2 != UINT32_C(0x11223344) ||
        s3 != UINT32_C(0x55667788) ||
        s4 != UINT32_C(0xA5A55A5A)) {
        test_riscv_register_errors++;
    }
}
#endif

static void task1_entry(void *arg)
{
    (void)arg;
    while (1) {
        test_heartbeat++;
        aixos_sem_post(g_sem);
#if defined(__riscv)
        riscv_register_stress();
#endif
        aixos_task_sleep(10);
    }
}

/* ── Task 2: 信号量接收者 ────────────────────── */
static void task2_entry(void *arg)
{
    (void)arg;
    while (1) {
        aixos_sem_wait(g_sem, 0xFFFFFFFF);
        aixos_task_sleep(50);
    }
}

/* ── Task 3: 消息队列接收 + 事件等待 ──────────── */
static void task3_entry(void *arg)
{
    (void)arg;
    char buf[32];
    size_t sz;

    while (1) {
        if (aixos_mq_recv(g_mq, buf, sizeof(buf), &sz, 0) == AIXOS_OK) {
            /* 收到消息 */
        }

        uint32_t set = 0;
        if (aixos_event_wait(g_evt, 0x01,
                             AIXOS_EVENT_AND | AIXOS_EVENT_CLEAR,
                             0, &set) == AIXOS_OK && set != 0U) {
            /* 事件到达 */
        }

        aixos_task_sleep(100);
    }
}

/* ── Task 4: 管道测试 ────────────────────────── */
static void task4_entry(void *arg)
{
    (void)arg;
    uint8_t buf[64];

    while (1) {
        aixos_pipe_write(g_pipe, "ping", 4, 0);
        aixos_pipe_read(g_pipe, buf, sizeof(buf), 0);
        aixos_task_sleep(200);
    }
}

/* ── 系统信息打印任务 ────────────────────────── */
static void monitor_entry(void *arg)
{
    (void)arg;
    while (1) {
        aixos_task_sleep(3000);

        aixos_sys_info_t sys;
        aixos_sys_info(&sys);

        aixos_task_info_t info;
        /* 打印当前任务信息 */
        if (aixos_task_get_info(aixos_task_self(), &info) == AIXOS_OK) {
            /* info available */
        }

        aixos_mem_info_t mem;
        aixos_mem_info(&mem);
    }
}

/* ── main ──────────────────────────────────────── */
int main(void)
{
    aixos_handle_t user_task;
    /* 初始化 AIXOS 内核 */
    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_trace_init();
    aixos_timer_init();
#if AIXOS_CFG_ENABLE_NAMESPACE
    aixos_namespace_init();
#endif
#if AIXOS_CFG_ENABLE_TIME_WHEEL
    aixos_timing_wheel_init();
#endif
    aixos_sched_init();

    /* 创建 IPC 对象 */
    g_sem   = aixos_sem_create(0);
    g_mq    = aixos_mq_create(4, 32);
    g_evt   = aixos_event_create();
    g_mutex = aixos_mutex_create();
    g_pipe  = aixos_pipe_create(256);
    g_timer = aixos_timer_create("timer1", AIXOS_TIMER_PERIODIC,
                                 timer1_callback, NULL);
    if (g_timer != AIXOS_HANDLE_INVALID) {
        aixos_timer_start(g_timer, 1000);
    }

    /* 创建任务 (示例应用使用静态栈，避免占用启动后系统堆预算) */
    aixos_task_create_static("task1", task1_entry, NULL,
                             task_stacks[0], sizeof(task_stacks[0]),
                             3, &task_tcbs[0]);
    aixos_task_create_static("task2", task2_entry, NULL,
                             task_stacks[1], sizeof(task_stacks[1]),
                             2, &task_tcbs[1]);
    aixos_task_create_static("task3", task3_entry, NULL,
                             task_stacks[2], sizeof(task_stacks[2]),
                             1, &task_tcbs[2]);
    aixos_task_create_static("task4", task4_entry, NULL,
                             task_stacks[3], sizeof(task_stacks[3]),
                             1, &task_tcbs[3]);
    aixos_task_create_static("mon", monitor_entry, NULL,
                             task_stacks[4], sizeof(task_stacks[4]),
                             1, &task_tcbs[4]);
    user_task = aixos_user_task_create_static("user", user_task_entry, NULL,
                                              user_task_stack,
                                              sizeof(user_task_stack),
                                              1, &user_task_tcb);
    if (user_task != AIXOS_HANDLE_INVALID) {
        (void)aixos_task_mpu_region_add(
            user_task, (uintptr_t)&test_user_heartbeat, 32U,
            AIXOS_MPU_READ | AIXOS_MPU_WRITE);
        (void)aixos_task_mpu_region_add(
            user_task, (uintptr_t)&test_user_syscall_errors, 32U,
            AIXOS_MPU_READ | AIXOS_MPU_WRITE);
    }

    /* 架构层初始化 (SysTick + NVIC) */
    aixos_arch_system_init();

    /* 启动 OS */
    aixos_start();

    /* 不会到达这里 */
    return 0;
}
