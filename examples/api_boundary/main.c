/*
 * Simulator-only API boundary test application.
 *
 * The volatile counters below are read by Renode after the OS has run for a
 * short window.  Keep symbol names stable; tools/run_api_boundary_renode.sh
 * depends on them.
 */

#include "aixos/aixos.h"
#include "aixos/mempool.h"
#include "aixos/namespace.h"
#include "kernel/timewheel.h"
#include "arch/include/aixos/arch/arch.h"

#define TASK_STACK_SIZE       512U
#define USER_STACK_SIZE       512U
#define MQ_MSG_SIZE           8U
#define MQ_DEPTH              2U

volatile uint32_t test_heartbeat = 0;
volatile uint32_t boundary_heartbeat = 0;
volatile uint32_t boundary_checks = 0;
volatile uint32_t boundary_failures = 0;
volatile uint32_t boundary_done = 0;
volatile uint32_t boundary_phase = 0;
volatile uint32_t boundary_last_failure = 0;
volatile uint32_t boundary_last_failure_phase = 0;
volatile uint32_t boundary_failure_bits0 = 0;
volatile uint32_t boundary_failure_bits1 = 0;
volatile uint32_t boundary_failure_bits2 = 0;
volatile uint32_t boundary_failure_bits3 = 0;
volatile uint32_t boundary_failure_bits4 = 0;
volatile uint32_t boundary_task_checks = 0;
volatile uint32_t boundary_ipc_checks = 0;
volatile uint32_t boundary_timer_checks = 0;
volatile uint32_t boundary_memory_checks = 0;
volatile uint32_t boundary_mpu_checks = 0;
volatile uint32_t boundary_user_checks = 0;
volatile uint32_t boundary_user_failures = 0;
volatile uint32_t boundary_user_heartbeat = 0;
volatile uint32_t boundary_user_done = 0;

enum {
    USER_METRIC_CHECKS = 0,
    USER_METRIC_FAILURES = 1,
    USER_METRIC_HEARTBEAT = 2,
    USER_METRIC_DONE = 3,
};

static volatile uint32_t user_metrics[8] __attribute__((aligned(32)));

static uint8_t tester_stack[TASK_STACK_SIZE] __attribute__((aligned(8)));
static uint8_t helper_stack[TASK_STACK_SIZE] __attribute__((aligned(8)));
static uint8_t user_stack[USER_STACK_SIZE] __attribute__((aligned(USER_STACK_SIZE)));
static aixos_tcb_t tester_tcb;
static aixos_tcb_t helper_tcb;
static aixos_tcb_t user_tcb;

static uint8_t mq_storage[MQ_DEPTH * MQ_MSG_SIZE] __attribute__((aligned(8)));
static size_t mq_lengths[MQ_DEPTH];
static uint8_t pipe_storage[4] __attribute__((aligned(8)));
static uintptr_t mempool_storage[8];
static volatile uint32_t timer_callbacks = 0;

static void record_check(int expression, volatile uint32_t *group)
{
    boundary_checks++;
    if (group != 0) {
        (*group)++;
    }
    if (!expression) {
        uint32_t index = boundary_checks - 1U;
        boundary_failures++;
        boundary_last_failure = boundary_checks;
        boundary_last_failure_phase = boundary_phase;
        if (index < 32U) {
            boundary_failure_bits0 |= UINT32_C(1) << index;
        } else if (index < 64U) {
            boundary_failure_bits1 |= UINT32_C(1) << (index - 32U);
        } else if (index < 96U) {
            boundary_failure_bits2 |= UINT32_C(1) << (index - 64U);
        } else if (index < 128U) {
            boundary_failure_bits3 |= UINT32_C(1) << (index - 96U);
        } else if (index < 160U) {
            boundary_failure_bits4 |= UINT32_C(1) << (index - 128U);
        }
    }
}

#define CHECK_GROUP(expr, group) record_check((expr), &(group))

static void helper_entry(void *arg)
{
    (void)arg;
    for (;;) {
        aixos_task_sleep(100U);
    }
}

static void timer_callback(void *arg)
{
    (void)arg;
    timer_callbacks++;
}

static void user_check(int expression)
{
    user_metrics[USER_METRIC_CHECKS]++;
    if (!expression) {
        user_metrics[USER_METRIC_FAILURES]++;
    }
}

static void user_entry(void *arg)
{
    uint32_t previous_tick = 0U;
    (void)arg;

    for (;;) {
        uint32_t tick = aixos_user_clock_get();
        user_check(aixos_user_task_self() != AIXOS_HANDLE_INVALID);
        user_check(tick >= previous_tick);
        previous_tick = tick;
        user_check(aixos_user_sleep(3U) == AIXOS_OK);
        user_metrics[USER_METRIC_DONE] = 1U;
        user_metrics[USER_METRIC_HEARTBEAT]++;
    }
}

static void publish_user_metrics(void)
{
    boundary_user_checks = user_metrics[USER_METRIC_CHECKS];
    boundary_user_failures = user_metrics[USER_METRIC_FAILURES];
    boundary_user_heartbeat = user_metrics[USER_METRIC_HEARTBEAT];
    boundary_user_done = user_metrics[USER_METRIC_DONE];
}

static void test_task_boundaries(void)
{
    aixos_handle_t helper;
    aixos_task_info_t info;

    boundary_phase = 1U;
    CHECK_GROUP(aixos_task_self() != AIXOS_HANDLE_INVALID,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_create_static("bad", 0, 0, helper_stack,
                                         sizeof(helper_stack), 1,
                                         &helper_tcb) == AIXOS_HANDLE_INVALID,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_create_static("small", helper_entry, 0,
                                         helper_stack,
                                         AIXOS_CFG_MIN_TASK_STACK_SIZE - 1U,
                                         1, &helper_tcb) ==
                AIXOS_HANDLE_INVALID, boundary_task_checks);
    CHECK_GROUP(aixos_task_create_static("prio", helper_entry, 0,
                                         helper_stack, sizeof(helper_stack),
                                         -1, &helper_tcb) ==
                AIXOS_HANDLE_INVALID, boundary_task_checks);
    CHECK_GROUP(aixos_task_create_static("prio", helper_entry, 0,
                                         helper_stack, sizeof(helper_stack),
                                         AIXOS_CFG_MAX_PRIORITY,
                                         &helper_tcb) ==
                AIXOS_HANDLE_INVALID, boundary_task_checks);

    helper = aixos_task_create_static("helper", helper_entry, 0,
                                      helper_stack, sizeof(helper_stack),
                                      1, &helper_tcb);
    CHECK_GROUP(helper != AIXOS_HANDLE_INVALID, boundary_task_checks);
    CHECK_GROUP(aixos_task_get_info(AIXOS_HANDLE_INVALID, &info) ==
                AIXOS_ERR_INVAL, boundary_task_checks);
    CHECK_GROUP(aixos_task_get_info(helper, 0) == AIXOS_ERR_INVAL,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_get_info(helper, &info) == AIXOS_OK,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_stack_check(helper) == AIXOS_OK,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_set_priority(helper, AIXOS_CFG_MAX_PRIORITY) ==
                AIXOS_ERR_INVAL, boundary_task_checks);
    CHECK_GROUP(aixos_task_set_priority(helper, 2) == AIXOS_OK,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_suspend(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_suspend(helper) == AIXOS_OK,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_suspend(helper) == AIXOS_ERR_INVAL,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_resume(helper) == AIXOS_OK,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_resume(helper) == AIXOS_ERR_INVAL,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_delete(helper) == AIXOS_OK,
                boundary_task_checks);
    CHECK_GROUP(aixos_task_delete(helper) == AIXOS_ERR_INVAL,
                boundary_task_checks);
}

static void test_semaphore_boundaries(void)
{
    aixos_handle_t sem;

    boundary_phase = 2U;
    CHECK_GROUP(aixos_sem_create(-1) == AIXOS_HANDLE_INVALID,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_create((int)UINT32_C(0x80000000)) ==
                AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_wait(AIXOS_HANDLE_INVALID, 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_post(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_post_from_isr(AIXOS_HANDLE_INVALID) ==
                AIXOS_ERR_CONTEXT, boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_get_count(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);

    sem = aixos_sem_create(0);
    CHECK_GROUP(sem != AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_wait(sem, 0U) == AIXOS_ERR_TIMEOUT,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_post(sem) == AIXOS_OK, boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_get_count(sem) == 1,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_wait(sem, 0U) == AIXOS_OK, boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_delete(sem) == AIXOS_OK, boundary_ipc_checks);

    sem = aixos_sem_create(AIXOS_CFG_SEM_MAX_COUNT);
    CHECK_GROUP(sem != AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_post(sem) == AIXOS_ERR_OVERFLOW,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_sem_delete(sem) == AIXOS_OK, boundary_ipc_checks);
}

static void test_mutex_boundaries(void)
{
    aixos_handle_t mutex;

    boundary_phase = 3U;
    CHECK_GROUP(aixos_mutex_lock(AIXOS_HANDLE_INVALID, 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_unlock(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_delete(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    mutex = aixos_mutex_create();
    CHECK_GROUP(mutex != AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_unlock(mutex) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_lock(mutex, 0U) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_lock(mutex, 0U) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_unlock(mutex) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_unlock(mutex) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mutex_delete(mutex) == AIXOS_OK,
                boundary_ipc_checks);
}

static void test_mq_boundaries(void)
{
    aixos_handle_t mq;
    uint8_t msg[MQ_MSG_SIZE] = { 1U, 2U, 3U, 4U };
    uint8_t out[MQ_MSG_SIZE] = { 0U };
    size_t received = 0U;
    uint32_t priority = 0U;
    aixos_mq_info_t info;

    boundary_phase = 4U;
    CHECK_GROUP(aixos_mq_create(0U, MQ_MSG_SIZE) == AIXOS_HANDLE_INVALID,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_create(1U, 0U) == AIXOS_HANDLE_INVALID,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_create(1U, AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U) ==
                AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_create_static(1U, MQ_MSG_SIZE, 0, mq_lengths) ==
                AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_create_static(1U, MQ_MSG_SIZE, mq_storage, 0) ==
                AIXOS_HANDLE_INVALID, boundary_ipc_checks);

    mq = aixos_mq_create_static(MQ_DEPTH, MQ_MSG_SIZE, mq_storage, mq_lengths);
    CHECK_GROUP(mq != AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_send(mq, 0, 1U, 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_send(mq, msg, MQ_MSG_SIZE + 1U, 0U) ==
                AIXOS_ERR_INVAL, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_send_priority(mq, msg, 1U, 1U, 0U) ==
                AIXOS_ERR_INVAL, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_recv(mq, 0, sizeof(out), &received, 0U) ==
                AIXOS_ERR_INVAL, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_recv(mq, out, sizeof(out), 0, 0U) ==
                AIXOS_ERR_INVAL, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_recv(mq, out, sizeof(out), &received, 0U) ==
                AIXOS_ERR_AGAIN, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_send(mq, msg, 4U, 0U) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_send(mq, msg, 4U, 0U) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_send(mq, msg, 1U, 0U) == AIXOS_ERR_BUSY,
                boundary_ipc_checks);
    received = 0U;
    CHECK_GROUP(aixos_mq_recv_priority(mq, out, 2U, &received, &priority,
                                       0U) == AIXOS_ERR_OVERFLOW &&
                received == 4U, boundary_ipc_checks);
    received = 0U;
    CHECK_GROUP(aixos_mq_recv_priority(mq, out, sizeof(out), &received,
                                       &priority, 0U) == AIXOS_OK &&
                received == 4U && priority == 0U,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_get_info(mq, &info) == AIXOS_OK &&
                info.current_messages == 1U, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_get_info(mq, 0) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_delete(mq) == AIXOS_OK, boundary_ipc_checks);
    CHECK_GROUP(aixos_mq_delete(mq) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
}

static void test_event_pipe_notify_boundaries(void)
{
    aixos_handle_t event;
    aixos_handle_t pipe;
    uint8_t data[4] = { 1U, 2U, 3U, 4U };
    uint8_t out[8] = { 0U };
    uint32_t matched = 0U;
    uint32_t notify_value = 0U;

    boundary_phase = 5U;
    CHECK_GROUP(aixos_event_wait(AIXOS_HANDLE_INVALID, 1U, AIXOS_EVENT_OR,
                                 0U, &matched) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_event_wait(AIXOS_HANDLE_INVALID, 0U, AIXOS_EVENT_OR,
                                 0U, &matched) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_event_wait(AIXOS_HANDLE_INVALID, 1U, 0U, 0U,
                                 &matched) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_event_wait(AIXOS_HANDLE_INVALID, 1U, AIXOS_EVENT_OR,
                                 0U, 0) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    event = aixos_event_create();
    CHECK_GROUP(event != AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_event_wait(event, 1U, AIXOS_EVENT_OR, 0U, &matched) ==
                AIXOS_ERR_AGAIN, boundary_ipc_checks);
    CHECK_GROUP(aixos_event_set(event, 0x3U) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_event_wait(event, 0x2U,
                                 AIXOS_EVENT_AND | AIXOS_EVENT_CLEAR,
                                 0U, &matched) == AIXOS_OK &&
                matched == 0x2U, boundary_ipc_checks);
    CHECK_GROUP(aixos_event_clear(event, 0x1U) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_event_delete(event) == AIXOS_OK,
                boundary_ipc_checks);

    CHECK_GROUP(aixos_pipe_create(0U) == AIXOS_HANDLE_INVALID,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_create_static(0, sizeof(pipe_storage)) ==
                AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_create_static(pipe_storage, 0U) ==
                AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    pipe = aixos_pipe_create_static(pipe_storage, sizeof(pipe_storage));
    CHECK_GROUP(pipe != AIXOS_HANDLE_INVALID, boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_write(pipe, 0, 1U, 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_write(pipe, data, AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U,
                                 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_read(pipe, 0, 1U, 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_read(pipe, out, AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U,
                                0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_write(pipe, data, 3U, 0U) == 3,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_write(pipe, data, 3U, 0U) == 1,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_read(pipe, out, 2U, 0U) == 2,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_read(pipe, out, sizeof(out), 0U) == 2,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_read(pipe, out, sizeof(out), 0U) == 0,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_write_from_isr(pipe, data, 1U) ==
                AIXOS_ERR_CONTEXT, boundary_ipc_checks);
    CHECK_GROUP(aixos_pipe_delete(pipe) == AIXOS_OK, boundary_ipc_checks);

    CHECK_GROUP(aixos_task_notify_wait(0U, 0U, 0, 0U) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_task_notify(AIXOS_HANDLE_INVALID, 1U,
                                  AIXOS_NOTIFY_SET_BITS) == AIXOS_ERR_INVAL,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_task_notify_from_isr(aixos_task_self(), 1U,
                                           AIXOS_NOTIFY_SET_BITS) ==
                AIXOS_ERR_CONTEXT, boundary_ipc_checks);
    CHECK_GROUP(aixos_task_notify_wait(0U, 0U, &notify_value, 0U) ==
                AIXOS_ERR_AGAIN, boundary_ipc_checks);
    CHECK_GROUP(aixos_task_notify(aixos_task_self(), 0x55U,
                                  AIXOS_NOTIFY_OVERWRITE) == AIXOS_OK,
                boundary_ipc_checks);
    CHECK_GROUP(aixos_task_notify(aixos_task_self(), 0xAAU,
                                  AIXOS_NOTIFY_NO_OVERWRITE) ==
                AIXOS_ERR_BUSY, boundary_ipc_checks);
    CHECK_GROUP(aixos_task_notify_wait(0U, UINT32_MAX, &notify_value, 0U) ==
                AIXOS_OK && notify_value == 0x55U, boundary_ipc_checks);
}

static void test_timer_boundaries(void)
{
    aixos_handle_t timer;

    boundary_phase = 6U;
    CHECK_GROUP(aixos_timer_create("bad", AIXOS_TIMER_PERIODIC, 0, 0) ==
                AIXOS_HANDLE_INVALID, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_create("bad", (aixos_timer_type_t)99,
                                   timer_callback, 0) ==
                AIXOS_HANDLE_INVALID, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_start(AIXOS_HANDLE_INVALID, 1U) ==
                AIXOS_ERR_INVAL, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_start(AIXOS_HANDLE_INVALID, 0U) ==
                AIXOS_ERR_INVAL, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_stop(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_timer_checks);
    CHECK_GROUP(aixos_timer_delete(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL,
                boundary_timer_checks);
    timer = aixos_timer_create("api", AIXOS_TIMER_PERIODIC, timer_callback, 0);
    CHECK_GROUP(timer != AIXOS_HANDLE_INVALID, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_start(timer, 2U) == AIXOS_OK,
                boundary_timer_checks);
    aixos_task_sleep(8U);
    CHECK_GROUP(timer_callbacks > 0U, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_stop(timer) == AIXOS_OK, boundary_timer_checks);
    CHECK_GROUP(aixos_timer_delete(timer) == AIXOS_OK, boundary_timer_checks);
}

static void test_memory_mpu_boundaries(void)
{
    aixos_mempool_t pool;
    void *blocks[5];
    unsigned int i;
    aixos_mem_info_t info;

    boundary_phase = 7U;
#if AIXOS_CFG_HEAP_LOCK_ON_START
    CHECK_GROUP(aixos_heap_is_locked() != 0, boundary_memory_checks);
    CHECK_GROUP(aixos_malloc(8U) == 0, boundary_memory_checks);
#else
    CHECK_GROUP(aixos_heap_check() == AIXOS_OK, boundary_memory_checks);
#endif
    aixos_mem_info(&info);
    CHECK_GROUP(info.total_bytes > 0U, boundary_memory_checks);
    CHECK_GROUP(aixos_heap_check() == AIXOS_OK, boundary_memory_checks);
    CHECK_GROUP(aixos_calloc((size_t)-1, 2U) == 0, boundary_memory_checks);
    CHECK_GROUP(aixos_realloc((void *)1, 8U) == 0, boundary_memory_checks);

    CHECK_GROUP(aixos_mempool_init(0, mempool_storage, sizeof(mempool_storage),
                                   8U, 2U) == AIXOS_ERR_INVAL,
                boundary_memory_checks);
    CHECK_GROUP(aixos_mempool_init(&pool, 0, sizeof(mempool_storage), 8U,
                                   2U) == AIXOS_ERR_INVAL,
                boundary_memory_checks);
    CHECK_GROUP(aixos_mempool_init(&pool, mempool_storage,
                                   sizeof(mempool_storage), 0U, 2U) ==
                AIXOS_ERR_INVAL, boundary_memory_checks);
    CHECK_GROUP(aixos_mempool_init(&pool, mempool_storage,
                                   sizeof(mempool_storage), 8U, 4U) ==
                AIXOS_OK, boundary_memory_checks);
    for (i = 0U; i < 4U; i++) {
        blocks[i] = aixos_mempool_alloc(&pool);
        CHECK_GROUP(blocks[i] != 0, boundary_memory_checks);
    }
    blocks[4] = aixos_mempool_alloc(&pool);
    CHECK_GROUP(blocks[4] == 0 && pool.alloc_failures == 1U,
                boundary_memory_checks);
    CHECK_GROUP(aixos_mempool_free(&pool, blocks[1]) == AIXOS_OK,
                boundary_memory_checks);
    CHECK_GROUP(aixos_mempool_free(&pool, blocks[1]) == AIXOS_ERR_INVAL,
                boundary_memory_checks);
    CHECK_GROUP(aixos_mempool_free(&pool, (uint8_t *)blocks[0] + 1U) ==
                AIXOS_ERR_INVAL, boundary_memory_checks);

    boundary_phase = 8U;
    CHECK_GROUP(aixos_mpu_region_valid(0U, AIXOS_CFG_MPU_MIN_REGION_SIZE,
                                       0U) == 0, boundary_mpu_checks);
    CHECK_GROUP(aixos_mpu_region_valid(1U, AIXOS_CFG_MPU_MIN_REGION_SIZE,
                                       AIXOS_MPU_READ) == 0,
                boundary_mpu_checks);
    CHECK_GROUP(aixos_mpu_region_valid((uintptr_t)pipe_storage, 24U,
                                       AIXOS_MPU_READ) == 0,
                boundary_mpu_checks);
    CHECK_GROUP(aixos_mpu_region_valid((uintptr_t)pipe_storage,
                                       sizeof(pipe_storage),
                                       AIXOS_MPU_WRITE) == 0,
                boundary_mpu_checks);
#if AIXOS_CFG_ENABLE_MPU
    CHECK_GROUP(aixos_task_mpu_region_add(AIXOS_HANDLE_INVALID,
                                          (uintptr_t)&boundary_heartbeat,
                                          32U, AIXOS_MPU_READ) ==
                AIXOS_ERR_INVAL, boundary_mpu_checks);
    CHECK_GROUP(aixos_task_mpu_region_add(aixos_task_self(),
                                          (uintptr_t)&boundary_heartbeat,
                                          32U, AIXOS_MPU_READ) ==
                AIXOS_ERR_INVAL, boundary_mpu_checks);
#else
    CHECK_GROUP(aixos_task_mpu_region_add(AIXOS_HANDLE_INVALID,
                                          (uintptr_t)&boundary_heartbeat,
                                          32U, AIXOS_MPU_READ) == AIXOS_OK,
                boundary_mpu_checks);
    CHECK_GROUP(aixos_task_mpu_region_add(aixos_task_self(),
                                          (uintptr_t)&boundary_heartbeat,
                                          32U, AIXOS_MPU_READ) == AIXOS_OK,
                boundary_mpu_checks);
#endif
}

static void tester_entry(void *arg)
{
    (void)arg;

    test_task_boundaries();
    test_semaphore_boundaries();
    test_mutex_boundaries();
    test_mq_boundaries();
    test_event_pipe_notify_boundaries();
    test_timer_boundaries();
    test_memory_mpu_boundaries();

    boundary_done = 1U;
    boundary_phase = 9U;
    for (;;) {
        publish_user_metrics();
        test_heartbeat++;
        boundary_heartbeat++;
        aixos_task_sleep(10U);
    }
}

int main(void)
{
    aixos_handle_t tester;
    aixos_handle_t user;

    aixos_heap_init(NULL, 0U);
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

    tester = aixos_task_create_static("api-boundary", tester_entry, 0,
                                      tester_stack, sizeof(tester_stack),
                                      3, &tester_tcb);
    if (tester == AIXOS_HANDLE_INVALID) {
        boundary_failures++;
    }

    user = aixos_user_task_create_static("user", user_entry, 0,
                                         user_stack, sizeof(user_stack),
                                         1, &user_tcb);
    if (user == AIXOS_HANDLE_INVALID) {
        boundary_failures++;
    } else {
        (void)aixos_task_mpu_region_add(
            user, (uintptr_t)user_metrics, sizeof(user_metrics),
            AIXOS_MPU_READ | AIXOS_MPU_WRITE);
    }

    aixos_arch_system_init();
    aixos_start();
}
