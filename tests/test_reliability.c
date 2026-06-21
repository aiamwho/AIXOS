#include "test.h"
#include "aixos/aixos.h"
#include "aixos/arch/arch.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

static void dummy_task(void *arg)
{
    (void)arg;
}

static unsigned int deferred_timer_fires;
static unsigned int timer_seen_in_isr;

static void deferred_timer_callback(void *arg)
{
    (void)arg;
    deferred_timer_fires++;
    timer_seen_in_isr += (unsigned int)aixos_in_isr();
}

extern void aixos_test_switch_requests_reset(void);
extern unsigned int aixos_test_switch_requests_get(void);

void test_reliability_guards(void)
{
    static unsigned char mq_storage[2][8];
    static size_t mq_lengths[2];
    static unsigned char large_mq_storage[AIXOS_CFG_ISR_COPY_MAX_BYTES + 1U];
    static size_t large_mq_lengths[1];
    static unsigned char pipe_storage[8];
    static unsigned char large_pipe_storage[
        AIXOS_CFG_ISR_COPY_MAX_BYTES + 1U];
    aixos_handle_t sem;
    aixos_handle_t mq;
    aixos_handle_t large_mq;
    aixos_handle_t pipe;
    aixos_handle_t large_pipe;
    aixos_handle_t timer;
    aixos_handle_t static_task;
    char small[2];
    char output[8];
    size_t output_size = 0U;
    char formatted[64];
    const aixos_crash_record_t *crash;
    uint32_t i;

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    CHECK(aixos_ms_to_ticks(0U) == 0U);
    CHECK(aixos_ms_to_ticks(1U) >= 1U);
    CHECK(aixos_ms_to_ticks(UINT32_MAX) == UINT32_MAX);
    CHECK(aixos_snprintf(formatted, sizeof(formatted),
                         "[%u] ev=%d d0=0x%X", 12U, -3, 0xABU) > 0);
    CHECK(formatted[0] == '[' && formatted[1] == '1' &&
          formatted[2] == '2');

    aixos_crash_record_clear();
    CHECK(aixos_crash_record_get() == NULL);
    aixos_crash_record_store(2U, 7U, 0x1234U, 0x5678U, 0x9ABCU);
    crash = aixos_crash_record_get();
    CHECK(crash != NULL);
    if (crash != NULL) {
        CHECK(crash->architecture == 2U);
        CHECK(crash->reason == 7U);
        CHECK(crash->program_counter == 0x1234U);
        CHECK(crash->fault_address == 0x5678U);
    }
    aixos_crash_record_clear();
    CHECK(aixos_crash_record_get() == NULL);

    aixos_isr_stats_reset();
    CHECK(aixos_isr_nesting_level() == 0U);
    CHECK(aixos_isr_nesting_high_watermark() == 0U);
    CHECK(aixos_isr_nesting_overflow_count() == 0U);
    aixos_isr_enter();
    CHECK(aixos_in_isr());
    CHECK(aixos_isr_nesting_level() == 1U);
    aixos_isr_enter();
    CHECK(aixos_isr_nesting_level() == 2U);
    CHECK(aixos_isr_nesting_high_watermark() == 2U);
    aixos_test_switch_requests_reset();
    aixos_reschedule_request();
    CHECK(aixos_test_switch_requests_get() == 0U);
    aixos_isr_exit();
    CHECK(aixos_test_switch_requests_get() == 0U);
    aixos_isr_exit();
    CHECK(aixos_test_switch_requests_get() == 1U);
    CHECK(aixos_isr_nesting_level() == 0U);
#if !AIXOS_CFG_ISR_NESTING_PANIC && AIXOS_CFG_ISR_NESTING_MAX <= 32U
    aixos_crash_record_clear();
    aixos_isr_stats_reset();
    for (i = 0U; i <= AIXOS_CFG_ISR_NESTING_MAX; i++) {
        aixos_isr_enter();
    }
    CHECK(aixos_isr_nesting_level() == AIXOS_CFG_ISR_NESTING_MAX + 1U);
    CHECK(aixos_isr_nesting_high_watermark() ==
          AIXOS_CFG_ISR_NESTING_MAX + 1U);
    CHECK(aixos_isr_nesting_overflow_count() == 1U);
    crash = aixos_crash_record_get();
    CHECK(crash != NULL);
    if (crash != NULL) {
        CHECK(crash->reason == AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW);
        CHECK(crash->fault_status == AIXOS_CFG_ISR_NESTING_MAX + 1U);
        CHECK(crash->fault_status2 == AIXOS_CFG_ISR_NESTING_MAX);
    }
    for (i = 0U; i <= AIXOS_CFG_ISR_NESTING_MAX; i++) {
        aixos_isr_exit();
    }
    CHECK(aixos_isr_nesting_level() == 0U);
    aixos_crash_record_clear();
    aixos_isr_stats_reset();
#endif

    static_task = aixos_task_create("static", dummy_task, NULL, 256, 3);
    CHECK(static_task != AIXOS_HANDLE_INVALID);

    sem = aixos_sem_create(0);
    CHECK(sem != AIXOS_HANDLE_INVALID);
    CHECK(aixos_sem_post_from_isr(sem) == AIXOS_ERR_CONTEXT);
    aixos_isr_enter();
    CHECK(aixos_task_sleep(1) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_sem_post(sem) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_sem_post_from_isr(sem) == AIXOS_OK);
    CHECK(aixos_in_isr());
    aixos_isr_exit();
    CHECK(!aixos_in_isr());
    CHECK(aixos_sem_get_count(sem) == 1);
    CHECK(aixos_sem_delete(sem) == AIXOS_OK);

    mq = aixos_mq_create_static(2U, 8U, mq_storage, mq_lengths);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_send(mq, "abcd", 5U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_recv(mq, small, sizeof(small), &output_size, 0U) ==
          AIXOS_ERR_OVERFLOW);
    CHECK(output_size == 5U);
    CHECK(aixos_mq_recv(mq, output, sizeof(output), &output_size, 0U) ==
          AIXOS_OK);
    CHECK(output_size == 5U);
    CHECK(output[0] == 'a' && output[3] == 'd');
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);

    large_mq = aixos_mq_create_static(
        1U, sizeof(large_mq_storage), large_mq_storage, large_mq_lengths);
    CHECK(large_mq != AIXOS_HANDLE_INVALID);
    aixos_isr_enter();
    CHECK(aixos_mq_send_from_isr(large_mq, large_mq_storage,
                                 sizeof(large_mq_storage)) ==
          AIXOS_ERR_INVAL);
    aixos_isr_exit();
    CHECK(aixos_mq_delete(large_mq) == AIXOS_OK);

    pipe = aixos_pipe_create_static(pipe_storage, sizeof(pipe_storage));
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_write_from_isr(pipe, "x", 1U) == AIXOS_ERR_CONTEXT);
    aixos_isr_enter();
    CHECK(aixos_pipe_write_from_isr(pipe, "xy", 2U) == 2);
    aixos_isr_exit();
    CHECK(aixos_pipe_read(pipe, output, sizeof(output), 0U) == 2);
    CHECK(output[0] == 'x' && output[1] == 'y');
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);

    large_pipe = aixos_pipe_create_static(large_pipe_storage,
                                          sizeof(large_pipe_storage));
    CHECK(large_pipe != AIXOS_HANDLE_INVALID);
    aixos_isr_enter();
    CHECK(aixos_pipe_write_from_isr(large_pipe, large_pipe_storage,
                                    sizeof(large_pipe_storage)) ==
          AIXOS_ERR_INVAL);
    aixos_isr_exit();
    CHECK(aixos_pipe_delete(large_pipe) == AIXOS_OK);

    deferred_timer_fires = 0U;
    timer_seen_in_isr = 0U;
    timer = aixos_timer_create("defer", AIXOS_TIMER_PERIODIC,
                               deferred_timer_callback, NULL);
    CHECK(timer != AIXOS_HANDLE_INVALID);
    CHECK(aixos_timer_start(timer, 2U) == AIXOS_OK);
    aixos_isr_enter();
    aixos_timer_tick(100U);
    CHECK(deferred_timer_fires == 0U);
    aixos_isr_exit();
    CHECK(aixos_timer_dispatch() == 1U);
    CHECK(deferred_timer_fires == 1U);
    CHECK(timer_seen_in_isr == 0U);
    CHECK(aixos_timer_delete(timer) == AIXOS_OK);

    CHECK(aixos_task_delete(static_task) == AIXOS_OK);
}

void test_mutex_inheritance(void)
{
    aixos_handle_t low_handle;
    aixos_handle_t high_handle;
    aixos_handle_t mutex;
    aixos_tcb_t *low;
    aixos_tcb_t *high;

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    low_handle = aixos_task_create("low", dummy_task, NULL, 256U, 2);
    high_handle = aixos_task_create("high", dummy_task, NULL, 256U, 8);
    CHECK(low_handle != AIXOS_HANDLE_INVALID);
    CHECK(high_handle != AIXOS_HANDLE_INVALID);
    low = aixos_tcb_from_handle(low_handle);
    high = aixos_tcb_from_handle(high_handle);
    mutex = aixos_mutex_create();
    CHECK(mutex != AIXOS_HANDLE_INVALID);

    aixos_test_set_current(low);
    CHECK(aixos_mutex_lock(mutex, 0U) == AIXOS_OK);
    CHECK(aixos_task_delete(low_handle) == AIXOS_ERR_BUSY);

    aixos_test_set_current(high);
    CHECK(aixos_mutex_lock(mutex, 5U) == AIXOS_OK);
    CHECK(high->state == AIXOS_TASK_BLOCKED);
    CHECK(low->priority == high->priority);
    aixos_task_tick(aixos_get_tick() + aixos_ms_to_ticks(5U));
    CHECK(high->state == AIXOS_TASK_READY);
    CHECK(high->wait_result == AIXOS_ERR_TIMEOUT);
    CHECK(low->priority == low->base_priority);

    aixos_test_set_current(low);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_OK);
    CHECK(aixos_mutex_delete(mutex) == AIXOS_OK);
    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(high_handle) == AIXOS_OK);
    CHECK(aixos_task_delete(low_handle) == AIXOS_OK);
}

void test_v6_safety_features(void)
{
    aixos_handle_t task;
    aixos_task_info_t task_info;
    aixos_mem_info_t mem_info;
    aixos_crash_record_t crash_copy;
    const aixos_crash_record_t *crash;
    aixos_trace_entry_t trace_entries[4];
    aixos_trace_info_t trace_info;
    aixos_sys_info_t sys_info;
    void *allocation;
    uint32_t i;

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();
    aixos_trace_init();

    CHECK(aixos_task_create("small", dummy_task, NULL, 128, 1) ==
          AIXOS_HANDLE_INVALID);
    task = aixos_task_create("guard", dummy_task, NULL, 256, 3);
    CHECK(task != AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_get_info(task, &task_info) == AIXOS_OK);
    CHECK(task_info.stack_guard_ok == 1U);
    CHECK(task_info.stack_free > 0U);
    {
        aixos_tcb_t *tcb_ptr = aixos_tcb_from_handle(task);
        if (tcb_ptr != NULL) {
            ((uint8_t *)tcb_ptr->stack_base)[0] = 0U;
        }
    }
    CHECK(aixos_task_stack_check(task) == AIXOS_ERR_CORRUPT);
    CHECK(aixos_task_get_info(task, &task_info) == AIXOS_OK);
    CHECK(task_info.stack_guard_ok == 0U);
    {
        aixos_tcb_t *tcb_ptr = aixos_tcb_from_handle(task);
        if (tcb_ptr != NULL) {
            ((uint8_t *)tcb_ptr->stack_base)[0] = 0xE7U;
        }
    }

    aixos_test_switch_requests_reset();
    CHECK(aixos_sched_lock() == AIXOS_OK);
    CHECK(aixos_sched_lock() == AIXOS_OK);
    CHECK(aixos_sched_lock_count() == 2U);
    aixos_reschedule_request();
    CHECK(aixos_test_switch_requests_get() == 0U);
    aixos_test_set_current(aixos_tcb_from_handle(task));
    CHECK(aixos_task_sleep(1U) == AIXOS_ERR_LOCKED);
    CHECK(aixos_task_suspend(task) == AIXOS_ERR_LOCKED);
    CHECK(aixos_task_delete(task) == AIXOS_ERR_LOCKED);
    aixos_test_set_current(NULL);
    CHECK(aixos_sched_unlock() == AIXOS_OK);
    CHECK(aixos_test_switch_requests_get() == 0U);
    CHECK(aixos_sched_unlock() == AIXOS_OK);
    CHECK(aixos_test_switch_requests_get() == 1U);
    CHECK(aixos_sched_unlock() == AIXOS_ERR_INVAL);

    aixos_crash_record_clear();
    aixos_crash_record_store_extended(2U, 9U, 0x1234U, 0x5678U, 0x9ABCU,
                                      0x11U, 0x22U, 0x33U);
    crash = aixos_crash_record_get();
    CHECK(crash != NULL);
    if (crash != NULL) {
        CHECK(crash->version == AIXOS_CRASH_RECORD_VERSION);
        CHECK(crash->sequence == 1U);
        CHECK(crash->fault_status == 0x11U);
        crash_copy = *crash;
        CHECK(aixos_crash_record_validate(&crash_copy) == AIXOS_OK);
        crash_copy.reason ^= 1U;
        CHECK(aixos_crash_record_validate(&crash_copy) == AIXOS_ERR_CORRUPT);
    }

    aixos_trace_init();
    for (i = 0U; i < AIXOS_CFG_TRACE_BUFFER_SIZE + 2U; i++) {
        aixos_trace_record(AIXOS_TRACE_TIMER, i, i + 1U);
    }
    CHECK(aixos_trace_snapshot(trace_entries, 4U, &trace_info) == 4U);
    CHECK(trace_info.available == AIXOS_CFG_TRACE_BUFFER_SIZE);
    CHECK(trace_info.capacity == AIXOS_CFG_TRACE_BUFFER_SIZE);
    CHECK(trace_info.overwritten == 2U);
    CHECK(trace_entries[0].sequence ==
          AIXOS_CFG_TRACE_BUFFER_SIZE - 2U);
    CHECK(trace_entries[3].arg0 == AIXOS_CFG_TRACE_BUFFER_SIZE + 1U);

    allocation = aixos_malloc(32U);
    CHECK(allocation != NULL);
    aixos_heap_lockdown();
    CHECK(aixos_heap_is_locked());
    CHECK(aixos_malloc(8U) == NULL);
    CHECK(aixos_realloc(allocation, 64U) == NULL);
    aixos_free(allocation);
    aixos_mem_info(&mem_info);
    CHECK(mem_info.runtime_locked == 1U);
    CHECK(mem_info.alloc_failures >= 2U);

    aixos_test_set_sched_stats(UINT64_C(0x100000005),
                               UINT64_C(0x100000000),
                               UINT64_C(0x200000003));
    aixos_sys_info(&sys_info);
    CHECK(sys_info.total_ticks == UINT64_C(0x100000005));
    CHECK(sys_info.idle_ticks == UINT64_C(0x100000000));
    CHECK(sys_info.switch_count == UINT64_C(0x200000003));

    CHECK(aixos_task_delete(task) == AIXOS_OK);
}
