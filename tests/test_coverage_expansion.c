#include <stdint.h>
#include <string.h>

#include "test.h"
#include "aixos/aixos.h"
#include "aixos/crash.h"
#include "aixos/microkernel.h"
#include "aixos/namespace.h"
#include "aixos/posix.h"
#include "aixos/sem.h"
#include "aixos/trace.h"
#include "kernel/heap_internal.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "kernel/timewheel.h"
#include "posix/include/fcntl.h"
#include "posix/include/mqueue.h"
#include "posix/include/semaphore.h"
#include "posix/include/unistd.h"

extern int aixos_posix_test_complete(aixos_pthread_t thread_id, void *value);
extern int aixos_first_start;

static unsigned int callback_count;
static unsigned int dump_count;
static unsigned int once_count;
static unsigned int signal_count;
static unsigned int sigev_count;
static unsigned int destructor_count;
static aixos_pthread_key_t coverage_exit_key;

static void coverage_dummy_task(void *arg)
{
    (void)arg;
}

static void *coverage_thread_entry(void *arg)
{
    return arg;
}

static void *coverage_thread_tls_entry(void *arg)
{
    (void)aixos_pthread_setspecific(coverage_exit_key, arg);
    return arg;
}

static void coverage_key_destructor(void *arg)
{
    if (arg != NULL) {
        destructor_count++;
    }
}

static void coverage_timer_callback(void *arg)
{
    uintptr_t *value = (uintptr_t *)arg;
    callback_count++;
    if (value != NULL) {
        (*value)++;
    }
}

static void coverage_trace_output(const char *line)
{
    CHECK(line != NULL);
    dump_count++;
}

static void coverage_once_init(void)
{
    once_count++;
}

static void coverage_signal_handler(void *arg)
{
    uintptr_t *value = (uintptr_t *)arg;
    signal_count++;
    if (value != NULL) {
        (*value)++;
    }
}

static void coverage_sigev_handler(aixos_sigval_t value)
{
    uintptr_t *counter = (uintptr_t *)value.sival_ptr;
    sigev_count++;
    if (counter != NULL) {
        (*counter)++;
    }
}

static void coverage_reset_kernel(void)
{
    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();
    aixos_namespace_init();
    aixos_timing_wheel_init();
    aixos_trace_init();
}

void test_trace_crash_timer_scheduler_paths(void)
{
    aixos_trace_entry_t entries[AIXOS_CFG_TRACE_BUFFER_SIZE];
    aixos_trace_info_t trace_info;
    aixos_sys_info_t sys_info;
    aixos_crash_record_t invalid = { 0 };
    const aixos_crash_record_t *record;
    aixos_handle_t task_a;
    aixos_handle_t task_b;
    aixos_tcb_t *tcb_a;
    aixos_tcb_t *tcb_b;
    aixos_handle_t once_timer;
    aixos_handle_t periodic_timer;
    uintptr_t timer_arg = 0U;
    uint32_t frame[8] = { 0U };
    unsigned int dispatched;

    coverage_reset_kernel();

    CHECK(aixos_trace_snapshot(NULL, 0U, &trace_info) == 0U);
    CHECK(trace_info.available == 0U);
    for (uint32_t i = 0U; i < AIXOS_CFG_TRACE_BUFFER_SIZE + 3U; i++) {
        aixos_trace_record(AIXOS_TRACE_TIMER, i, i + 1U);
    }
    CHECK(aixos_trace_snapshot(entries, AIXOS_CFG_TRACE_BUFFER_SIZE,
                               &trace_info) == AIXOS_CFG_TRACE_BUFFER_SIZE);
    CHECK(trace_info.available == AIXOS_CFG_TRACE_BUFFER_SIZE);
    CHECK(trace_info.overwritten == 3U);
    aixos_trace_get_info(&trace_info);
    CHECK(trace_info.capacity == AIXOS_CFG_TRACE_BUFFER_SIZE);
    dump_count = 0U;
    aixos_trace_dump(coverage_trace_output);
    CHECK(dump_count == AIXOS_CFG_TRACE_BUFFER_SIZE);
    aixos_trace_dump(NULL);
    aixos_sys_info(&sys_info);
    CHECK(sys_info.task_count == 0U);
    aixos_sys_info(NULL);

    CHECK(aixos_crash_record_validate(NULL) == AIXOS_ERR_CORRUPT);
    CHECK(aixos_crash_record_validate(&invalid) == AIXOS_ERR_CORRUPT);
    aixos_crash_record_clear();
    CHECK(aixos_crash_record_get() == NULL);
    aixos_crash_record_store(1U, 2U, 3U, 4U, 5U);
    record = aixos_crash_record_get();
    CHECK(record != NULL);
    if (record != NULL) {
        CHECK(record->reason == 2U);
        CHECK(record->program_counter == 3U);
    }
    aixos_crash_record_store_extended(1U, 6U, 7U, 8U, 9U, 10U, 11U, 12U);
    record = aixos_crash_record_get();
    CHECK(record != NULL);
    if (record != NULL) {
        CHECK(record->sequence == 2U);
        CHECK(record->fault_status == 10U);
        CHECK(record->auxiliary == 12U);
    }
    frame[6] = 0x12345678U;
    aixos_arm_fault_handler(frame, 0x44U);
    record = aixos_crash_record_get();
    CHECK(record != NULL);
    if (record != NULL) {
        CHECK(record->reason == 0x44U);
        CHECK(record->program_counter == 0x12345678U);
    }
    aixos_system_reset();
    aixos_panic("host panic", 0x55U);
    record = aixos_crash_record_get();
    CHECK(record != NULL);
    if (record != NULL) {
        CHECK(record->reason == 0x55U);
    }

    task_a = aixos_task_create("ta", coverage_dummy_task, NULL, 256U, 2);
    task_b = aixos_task_create("tb", coverage_dummy_task, NULL, 256U, 2);
    CHECK(task_a != AIXOS_HANDLE_INVALID);
    CHECK(task_b != AIXOS_HANDLE_INVALID);
    tcb_a = aixos_tcb_from_handle(task_a);
    tcb_b = aixos_tcb_from_handle(task_b);
    CHECK(tcb_a != NULL && tcb_b != NULL);
    if (tcb_a != NULL && tcb_b != NULL) {
        aixos_sched_add_task(tcb_a);
        aixos_sched_add_task(tcb_b);
        aixos_schedule();
        CHECK(aixos_task_self() == task_a);
        aixos_sched_rotate_current();
        aixos_schedule();
        CHECK(aixos_task_self() == task_b);
        tcb_b->time_slice = 1U;
        aixos_tick_handler();
        CHECK(aixos_get_tick() > 0U);
    }
    CHECK(aixos_cpu_usage_get() <= 100U);
    aixos_test_set_sched_stats(10U, 12U, 1U);
    CHECK(aixos_cpu_usage_get() == 0U);
    CHECK(aixos_task_delete(task_a) == AIXOS_OK);
    CHECK(aixos_task_delete(task_b) == AIXOS_OK);

    once_timer = aixos_timer_create("once", AIXOS_TIMER_ONESHOT,
                                    coverage_timer_callback, &timer_arg);
    periodic_timer = aixos_timer_create("periodic", AIXOS_TIMER_PERIODIC,
                                        coverage_timer_callback, &timer_arg);
    CHECK(once_timer != AIXOS_HANDLE_INVALID);
    CHECK(periodic_timer != AIXOS_HANDLE_INVALID);
    CHECK(aixos_timer_start(once_timer, 1U) == AIXOS_OK);
    CHECK(aixos_timer_start(periodic_timer, 2U) == AIXOS_OK);
    CHECK(aixos_timer_start(periodic_timer, 3U) == AIXOS_OK);
    aixos_timer_tick(aixos_get_tick() + 10U);
    dispatched = aixos_timer_dispatch();
    CHECK(dispatched >= 2U);
    CHECK(callback_count >= 2U);
    CHECK(timer_arg >= 2U);
    CHECK(aixos_timer_stop(periodic_timer) == AIXOS_OK);
    CHECK(aixos_timer_stop(periodic_timer) == AIXOS_OK);
    CHECK(aixos_timer_delete(once_timer) == AIXOS_OK);
    CHECK(aixos_timer_delete(periodic_timer) == AIXOS_OK);
    CHECK(aixos_timer_create("bad", AIXOS_TIMER_ONESHOT, NULL, NULL) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_timer_create("bad", (aixos_timer_type_t)99,
                             coverage_timer_callback, NULL) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_timer_start(AIXOS_HANDLE_INVALID, 1U) == AIXOS_ERR_INVAL);
    CHECK(aixos_timer_start(periodic_timer, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_timer_dispatch() == 0U);
    CHECK(aixos_timer_service_start() == AIXOS_OK);
    aixos_test_timer_service_entry();
}

void test_task_signal_and_lifecycle_edges(void)
{
    static uint8_t user_stack[512] __attribute__((aligned(512)));
    static aixos_tcb_t static_tcb;
    aixos_handle_t kernel_task;
    aixos_handle_t static_user;
    aixos_tcb_t *tcb;
    aixos_task_info_t info;
    uint32_t pending = 0U;
    uintptr_t signal_arg = 0U;
    uint32_t start_tick;

    coverage_reset_kernel();
    CHECK(aixos_ms_to_ticks(0U) == 0U);
    CHECK(aixos_ms_to_ticks(1U) >= 1U);
    CHECK(aixos_ms_to_ticks(UINT32_MAX) == UINT32_MAX);
    CHECK(aixos_ms_to_ticks(UINT32_MAX - 1U) ==
          AIXOS_CFG_TIMEOUT_MAX_TICKS);
    aixos_test_set_sched_stats(100U, 0U, 0U);
    start_tick = aixos_get_tick();
    CHECK(aixos_timeout_remaining_ms(start_tick, UINT32_MAX) == UINT32_MAX);
    CHECK(aixos_timeout_remaining_ms(start_tick - 2U, 1U) == 0U);
    CHECK(aixos_timeout_remaining_ms(start_tick, 10U) > 0U);

    kernel_task = aixos_task_create("kernel", coverage_dummy_task, NULL,
                                    256U, 3);
    CHECK(kernel_task != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(kernel_task);
    CHECK(tcb != NULL);
    if (tcb == NULL) {
        return;
    }
    aixos_test_set_current(tcb);
    CHECK(aixos_task_yield() == AIXOS_OK);
    CHECK(aixos_task_sleep(0U) == AIXOS_OK);
    CHECK(aixos_task_suspend(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_resume(kernel_task) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_get_info(AIXOS_HANDLE_INVALID, &info) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_get_info(kernel_task, NULL) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_get_info(kernel_task, &info) == AIXOS_OK);
    CHECK(aixos_task_stack_check(kernel_task) == AIXOS_OK);
    ((uint8_t *)tcb->stack_base)[0] = 0U;
    CHECK(aixos_task_stack_check(kernel_task) == AIXOS_ERR_CORRUPT);
    aixos_task_tick(aixos_get_tick() + 1U);
    CHECK(tcb->stack_guard_failed == 1U);
    CHECK(aixos_task_is_user(kernel_task) == 0);
    CHECK(aixos_task_is_user(AIXOS_HANDLE_INVALID) == AIXOS_ERR_INVAL);

    CHECK(aixos_task_signal_send(kernel_task, 32U) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_signal_pending(kernel_task, NULL) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_signal_handle(1U, coverage_signal_handler,
                                   &signal_arg) == AIXOS_OK);
    tcb->state = AIXOS_TASK_BLOCKED;
    tcb->wait_type = AIXOS_OBJ_SIGNAL;
    tcb->wait_obj = tcb;
    CHECK(aixos_task_signal_send(kernel_task, 2U) == AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_READY);
    aixos_sched_remove_task(tcb);
    tcb->state = AIXOS_TASK_BLOCKED;
    aixos_task_wake_recheck(tcb, AIXOS_ERR_INTR);
    CHECK(tcb->state == AIXOS_TASK_READY);
    CHECK(aixos_task_signal_mask(1U << 1U) == AIXOS_OK);
    CHECK(aixos_task_signal_send(kernel_task, 1U) == AIXOS_OK);
    CHECK(aixos_task_signal_pending(kernel_task, &pending) == AIXOS_OK);
    CHECK((pending & (1U << 1U)) != 0U);
    CHECK(aixos_task_signal_deliver() == AIXOS_OK);
    CHECK(signal_count == 0U);
    CHECK(aixos_task_signal_mask(0U) == AIXOS_OK);
    CHECK(aixos_task_signal_deliver() == AIXOS_OK);
    CHECK(signal_count == 1U && signal_arg == 1U);
    CHECK(aixos_task_signal_pending(AIXOS_HANDLE_INVALID, &pending) ==
          AIXOS_ERR_INVAL);
    aixos_test_set_current(NULL);
    CHECK(aixos_task_signal_handle(1U, coverage_signal_handler,
                                   &signal_arg) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_signal_mask(0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_signal_deliver() == AIXOS_OK);

    static_user = aixos_user_task_create_static("ust", coverage_dummy_task,
                                                NULL, user_stack,
                                                sizeof(user_stack), 2,
                                                &static_tcb);
    CHECK(static_user != AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_is_user(static_user) == 1);
    CHECK(aixos_task_delete(static_user) == AIXOS_OK);
    CHECK(aixos_task_delete(kernel_task) == AIXOS_OK);
}

void test_posix_extended_api_edges(void)
{
    aixos_pthread_attr_t attr;
    aixos_pthread_t thread;
    aixos_pthread_t thread_run;
    aixos_pthread_t thread_exit;
    aixos_pthread_t thread_detached;
    aixos_pthread_t thread_detach_completed;
    void *joined = NULL;
    aixos_pthread_key_t key;
    aixos_pthread_once_t once = AIXOS_PTHREAD_ONCE_INIT;
    aixos_pthread_mutexattr_t mattr;
    aixos_pthread_mutex_t mutex;
    aixos_pthread_mutex_t cond_mutex;
    aixos_pthread_condattr_t cattr;
    aixos_pthread_cond_t cond;
    aixos_pthread_rwlockattr_t rwattr;
    aixos_pthread_rwlock_t rwlock;
    aixos_pthread_barrierattr_t battr;
    aixos_pthread_barrier_t barrier;
    aixos_sem_posix_t sem;
    aixos_timespec_t ts;
    aixos_timespec_t rem;
    aixos_itimerspec_t its;
    aixos_itimerspec_t old_its;
    aixos_sigevent_t sev;
    aixos_timer_posix_t ptimer;
    aixos_mqd_t mq;
    aixos_mq_attr_t mq_attr;
    char msg[8] = { 0 };
    unsigned int prio = 0U;
    int value = 0;
    int out = 0;
    size_t stack_size = 0U;
    uintptr_t timer_signal_arg = 0U;
    aixos_handle_t current;
    aixos_handle_t peer_current = AIXOS_HANDLE_INVALID;

    coverage_reset_kernel();
    CHECK(aixos_posix_errno_location() != NULL);

    CHECK(aixos_pthread_attr_init(&attr) == 0);
    CHECK(aixos_pthread_attr_setstacksize(&attr, 512U) == 0);
    CHECK(aixos_pthread_attr_getstacksize(&attr, &stack_size) == 0);
    CHECK(stack_size == 512U);
    CHECK(aixos_pthread_attr_setdetachstate(
              &attr, AIXOS_PTHREAD_CREATE_JOINABLE) == 0);
    CHECK(aixos_pthread_attr_getdetachstate(&attr, &out) == 0);
    CHECK(out == AIXOS_PTHREAD_CREATE_JOINABLE);
    CHECK(aixos_pthread_attr_setschedpolicy(&attr, 7) == 0);
    CHECK(aixos_pthread_attr_getschedpolicy(&attr, &out) == 0);
    CHECK(out == 7);
    CHECK(aixos_pthread_attr_setinheritsched(
              &attr, AIXOS_PTHREAD_EXPLICIT_SCHED) == 0);
    CHECK(aixos_pthread_attr_getinheritsched(&attr, &out) == 0);
    CHECK(out == AIXOS_PTHREAD_EXPLICIT_SCHED);
    CHECK(aixos_pthread_attr_setschedprio(&attr, 3) == 0);
    CHECK(aixos_pthread_attr_getschedprio(&attr, &out) == 0);
    CHECK(out == 3);
    CHECK(aixos_pthread_create(NULL, &attr, coverage_thread_entry,
                               NULL) == AIXOS_EINVAL);
    CHECK(aixos_pthread_create(&thread, &attr, NULL, NULL) == AIXOS_EINVAL);
    CHECK(aixos_pthread_create(&thread, &attr, coverage_thread_entry,
                               (void *)0x1234) == 0);
    CHECK(aixos_pthread_equal(thread, thread) != 0);
    CHECK(aixos_pthread_self() == AIXOS_HANDLE_INVALID);
    CHECK(aixos_pthread_getschedprio(thread, &out) == 0);
    CHECK(aixos_pthread_setschedprio(thread, 4) == 0);
    CHECK(aixos_pthread_getschedprio(thread, &out) == 0);
    CHECK(out == 4);
    CHECK(aixos_posix_test_complete(thread, (void *)0xCAFE) == 0);
    CHECK(aixos_pthread_join(thread, &joined) == 0);
    CHECK(joined == (void *)0xCAFE);
    CHECK(aixos_task_delete(thread) == AIXOS_OK);
    CHECK(aixos_pthread_join(thread, NULL) == AIXOS_ESRCH);
    CHECK(aixos_pthread_detach(thread) == AIXOS_ESRCH);

    CHECK(aixos_pthread_key_create(&coverage_exit_key,
                                   coverage_key_destructor) == 0);
    destructor_count = 0U;
    CHECK(aixos_pthread_create(&thread_run, &attr, coverage_thread_tls_entry,
                               (void *)0xB00) == 0);
    CHECK(aixos_posix_test_run_thread(thread_run) == 0);
    joined = NULL;
    CHECK(aixos_pthread_join(thread_run, &joined) == 0);
    CHECK(joined == (void *)0xB00);
    CHECK(destructor_count == 1U);
    CHECK(aixos_task_delete(thread_run) == AIXOS_OK);

    CHECK(aixos_pthread_create(&thread_exit, &attr, coverage_thread_entry,
                               (void *)0xBAD) == 0);
    aixos_test_set_current(aixos_tcb_from_handle(thread_exit));
    CHECK(aixos_pthread_setspecific(coverage_exit_key, (void *)0xC00) == 0);
    aixos_pthread_exit((void *)0xBEEF);
    joined = NULL;
    CHECK(aixos_pthread_join(thread_exit, &joined) == 0);
    CHECK(joined == (void *)0xBEEF);
    CHECK(destructor_count == 2U);
    CHECK(aixos_pthread_key_delete(coverage_exit_key) == 0);
    CHECK(aixos_pthread_attr_setdetachstate(
              &attr, AIXOS_PTHREAD_CREATE_DETACHED) == 0);
    CHECK(aixos_pthread_create(&thread_detached, &attr,
                               coverage_thread_entry, (void *)0xDAD) == 0);
    CHECK(aixos_posix_test_run_thread(thread_detached) == 0);
    CHECK(aixos_pthread_join(thread_detached, NULL) == AIXOS_ESRCH);
    CHECK(aixos_task_delete(thread_detached) == AIXOS_OK);
    CHECK(aixos_pthread_attr_setdetachstate(
              &attr, AIXOS_PTHREAD_CREATE_JOINABLE) == 0);
    CHECK(aixos_pthread_create(&thread_detach_completed, &attr,
                               coverage_thread_entry, NULL) == 0);
    CHECK(aixos_posix_test_complete(thread_detach_completed, NULL) == 0);
    CHECK(aixos_pthread_detach(thread_detach_completed) == 0);
    CHECK(aixos_pthread_join(thread_detach_completed, NULL) == AIXOS_ESRCH);
    CHECK(aixos_task_delete(thread_detach_completed) == AIXOS_OK);
    CHECK(aixos_pthread_attr_destroy(&attr) == 0);

    CHECK(aixos_pthread_once(NULL, coverage_once_init) == AIXOS_EINVAL);
    CHECK(aixos_pthread_once(&once, NULL) == AIXOS_EINVAL);
    CHECK(aixos_pthread_once(&once, coverage_once_init) == 0);
    CHECK(aixos_pthread_once(&once, coverage_once_init) == 0);
    CHECK(once_count == 1U);

    CHECK(aixos_pthread_key_create(&key, NULL) == 0);
    CHECK(aixos_pthread_getspecific(key) == NULL);
    CHECK(aixos_pthread_setspecific(key, &value) == AIXOS_EINVAL);
    current = aixos_task_create("posix", coverage_dummy_task, NULL, 512U, 2);
    CHECK(current != AIXOS_HANDLE_INVALID);
    if (current == AIXOS_HANDLE_INVALID) {
        return;
    }
    peer_current = aixos_task_create("posix2", coverage_dummy_task, NULL,
                                     512U, 3);
    CHECK(peer_current != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(aixos_tcb_from_handle(current));
    CHECK(aixos_pthread_setspecific(key, &value) == 0);
    CHECK(aixos_pthread_getspecific(key) == &value);
    CHECK(aixos_pthread_key_delete(key) == 0);
    CHECK(aixos_pthread_getspecific(key) == NULL);
    CHECK(aixos_pthread_key_delete(key) == AIXOS_EINVAL);

    CHECK(aixos_pthread_mutexattr_init(&mattr) == 0);
    CHECK(aixos_pthread_mutexattr_settype(&mattr,
                                          AIXOS_PTHREAD_MUTEX_RECURSIVE) == 0);
    CHECK(aixos_pthread_mutexattr_gettype(&mattr, &out) == 0);
    CHECK(out == AIXOS_PTHREAD_MUTEX_RECURSIVE);
    CHECK(aixos_pthread_mutexattr_setprotocol(&mattr,
                                              AIXOS_PTHREAD_PRIO_PROTECT) == 0);
    CHECK(aixos_pthread_mutexattr_getprotocol(&mattr, &out) == 0);
    CHECK(out == AIXOS_PTHREAD_PRIO_PROTECT);
    CHECK(aixos_pthread_mutexattr_setprioceiling(&mattr, 2) == 0);
    CHECK(aixos_pthread_mutexattr_getprioceiling(&mattr, &out) == 0);
    CHECK(out == 2);
    CHECK(aixos_pthread_mutex_init(&mutex, &mattr) == 0);
    CHECK(aixos_pthread_mutex_lock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_lock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_unlock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_unlock(&mutex) == 0);
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    CHECK(aixos_pthread_mutex_timedlock(&mutex, &ts) == 0);
    CHECK(aixos_pthread_mutex_unlock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_destroy(&mutex) == 0);
    CHECK(aixos_pthread_mutexattr_destroy(&mattr) == 0);
    CHECK(aixos_sched_yield() == 0);
    CHECK(aixos_sched_get_priority_min(0) == 0);
    CHECK(aixos_sched_get_priority_max(0) == AIXOS_CFG_MAX_PRIORITY - 1);

    CHECK(aixos_pthread_condattr_init(&cattr) == 0);
    CHECK(aixos_pthread_condattr_getclock(&cattr, &out) == 0);
    CHECK(out == AIXOS_CLOCK_MONOTONIC);
    CHECK(aixos_pthread_condattr_setclock(&cattr,
                                          AIXOS_CLOCK_REALTIME) ==
          AIXOS_ENOTSUP);
    CHECK(aixos_pthread_cond_init(&cond, &cattr) == 0);
    CHECK(aixos_pthread_mutex_init(&cond_mutex, NULL) == 0);
    CHECK(aixos_pthread_mutex_lock(&cond_mutex) == 0);
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    CHECK(aixos_pthread_cond_timedwait(&cond, &cond_mutex, &ts) ==
          AIXOS_ETIMEDOUT);
    CHECK(aixos_pthread_mutex_unlock(&cond_mutex) == 0);
    cond.waiters = 1U;
    CHECK(aixos_pthread_cond_signal(&cond) == 0);
    CHECK(aixos_pthread_mutex_lock(&cond_mutex) == 0);
    CHECK(aixos_pthread_cond_wait(&cond, &cond_mutex) == 0);
    CHECK(aixos_pthread_mutex_unlock(&cond_mutex) == 0);
    CHECK(aixos_pthread_cond_signal(&cond) == 0);
    CHECK(aixos_pthread_cond_signal(&cond) == 0);
    CHECK(aixos_pthread_cond_broadcast(&cond) == 0);
    CHECK(aixos_pthread_mutex_destroy(&cond_mutex) == 0);
    CHECK(aixos_pthread_cond_destroy(&cond) == 0);
    CHECK(aixos_pthread_condattr_destroy(&cattr) == 0);

    CHECK(aixos_pthread_rwlockattr_init(&rwattr) == 0);
    CHECK(aixos_pthread_rwlock_init(&rwlock, &rwattr) == 0);
    CHECK(aixos_pthread_rwlock_rdlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_trywrlock(&rwlock) == AIXOS_EBUSY);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_tryrdlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_wrlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_tryrdlock(&rwlock) == AIXOS_EBUSY);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_wrlock(&rwlock) == 0);
    aixos_test_set_current(aixos_tcb_from_handle(peer_current));
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    CHECK(aixos_pthread_rwlock_timedrdlock(&rwlock, &ts) ==
          AIXOS_ETIMEDOUT);
    aixos_test_set_current(aixos_tcb_from_handle(current));
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_rdlock(&rwlock) == 0);
    aixos_test_set_current(aixos_tcb_from_handle(peer_current));
    CHECK(aixos_pthread_rwlock_timedwrlock(&rwlock, &ts) ==
          AIXOS_ETIMEDOUT);
    aixos_test_set_current(aixos_tcb_from_handle(current));
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_timedrdlock(&rwlock, &ts) == 0);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_timedwrlock(&rwlock, &ts) == 0);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_destroy(&rwlock) == 0);
    CHECK(aixos_pthread_rwlockattr_destroy(&rwattr) == 0);

    CHECK(aixos_pthread_barrierattr_init(&battr) == 0);
    CHECK(aixos_pthread_barrier_init(&barrier, &battr, 1U) == 0);
    CHECK(aixos_pthread_barrier_wait(&barrier) ==
          AIXOS_PTHREAD_BARRIER_SERIAL_THREAD);
    CHECK(aixos_pthread_barrier_destroy(&barrier) == 0);
    CHECK(aixos_pthread_barrierattr_destroy(&battr) == 0);

    CHECK(aixos_sem_posix_init(&sem, 1U) == 0);
    CHECK(aixos_sem_posix_getvalue(&sem, &out) == 0);
    CHECK(out == 1);
    CHECK(aixos_sem_posix_wait(&sem) == 0);
    CHECK(aixos_sem_posix_trywait(&sem) == AIXOS_ETIMEDOUT);
    CHECK(aixos_sem_posix_post(&sem) == 0);
    CHECK(aixos_sem_posix_timedwait(&sem, &ts) == 0);
    CHECK(aixos_sem_posix_destroy(&sem) == 0);

    aixos_test_set_sched_stats(2500U, 0U, 0U);
    CHECK(aixos_clock_gettime(AIXOS_CLOCK_MONOTONIC, &ts) == 0);
    CHECK(aixos_clock_getres(AIXOS_CLOCK_MONOTONIC, &rem) == 0);
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    CHECK(aixos_nanosleep(&ts, &rem) == 0);
    CHECK(aixos_clock_nanosleep(AIXOS_CLOCK_MONOTONIC, 0, &ts) == 0);
    ts.tv_nsec = 1000000;
    CHECK(aixos_clock_nanosleep(AIXOS_CLOCK_MONOTONIC,
                                AIXOS_TIMER_ABSTIME, &ts) == 0);

    memset(&sev, 0, sizeof(sev));
    sev.notify = AIXOS_SIGEV_THREAD;
    sev.value.sival_ptr = &timer_signal_arg;
    sev.function = coverage_sigev_handler;
    CHECK(aixos_timer_posix_create(AIXOS_CLOCK_MONOTONIC, &sev,
                                   &ptimer) == 0);
    CHECK(aixos_timer_posix_create(AIXOS_CLOCK_MONOTONIC, &sev,
                                   NULL) == AIXOS_EINVAL);
    sev.notify = AIXOS_SIGEV_SIGNAL;
    CHECK(aixos_timer_posix_create(AIXOS_CLOCK_MONOTONIC, &sev,
                                   &ptimer) == AIXOS_EINVAL);
    sev.notify = AIXOS_SIGEV_THREAD;
    memset(&its, 0, sizeof(its));
    CHECK(aixos_timer_posix_settime(ptimer, 3, &its, NULL) ==
          AIXOS_EINVAL);
    CHECK(aixos_timer_posix_settime(ptimer, 0, NULL, NULL) ==
          AIXOS_EINVAL);
    its.value.tv_nsec = 1000000000L;
    CHECK(aixos_timer_posix_settime(ptimer, 0, &its, NULL) ==
          AIXOS_EINVAL);
    its.value.tv_nsec = 0;
    its.value.tv_nsec = 1000000;
    its.interval.tv_nsec = 2000000;
    CHECK(aixos_timer_posix_settime(ptimer, 0, &its, &old_its) == 0);
    CHECK(aixos_timer_posix_gettime(ptimer, &old_its) == 0);
    aixos_timer_tick(aixos_get_tick() + 10U);
    CHECK(aixos_timer_dispatch() >= 1U);
    CHECK(sigev_count >= 1U);
    CHECK(timer_signal_arg >= 1U);
    CHECK(aixos_timer_posix_getoverrun(ptimer) >= 0);
    memset(&its, 0, sizeof(its));
    CHECK(aixos_timer_posix_settime(ptimer, 0, &its, NULL) == 0);
    CHECK(aixos_timer_posix_delete(ptimer) == 0);
    CHECK(aixos_timer_posix_gettime(ptimer, &old_its) == AIXOS_EINVAL);
    CHECK(aixos_timer_posix_create(AIXOS_CLOCK_REALTIME, NULL,
                                   &ptimer) == AIXOS_ENOTSUP);

    mq = aixos_mq_posix_open(2, 4);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_posix_send(mq, "abcd", 4U, 1U) == 0);
    CHECK(aixos_mq_posix_getattr(mq, &mq_attr) == 0);
    CHECK(mq_attr.mq_curmsgs == 1);
    CHECK(aixos_mq_posix_receive(mq, msg, sizeof(msg), &prio) == 4);
    CHECK(prio == 1U);
    CHECK(aixos_mq_posix_timedsend(mq, "xy", 2U, 0U, &ts) == 0);
    CHECK(aixos_mq_posix_timedreceive(mq, msg, sizeof(msg), &prio,
                                      &ts) == 2);
    CHECK(aixos_mq_posix_setattr(mq, NULL, &mq_attr) == 0);
    CHECK(aixos_mq_posix_notify(mq, NULL) == AIXOS_ENOSYS);
    CHECK(aixos_mq_posix_close(mq) == 0);
    CHECK(aixos_mq_posix_open(0, 1) == AIXOS_HANDLE_INVALID);

    aixos_test_set_current(NULL);
    if (peer_current != AIXOS_HANDLE_INVALID) {
        CHECK(aixos_task_delete(peer_current) == AIXOS_OK);
    }
    CHECK(aixos_task_delete(current) == AIXOS_OK);
}

void test_microkernel_user_syscall_edges(void)
{
    static uint8_t user_stack[512] __attribute__((aligned(512)));
    static uint8_t user_buffer[512] __attribute__((aligned(512)));
    static aixos_tcb_t user_tcb;
    aixos_handle_t user;
    aixos_tcb_t *tcb;
    aixos_syscall_request_t req;
    aixos_cap_t sem;
    aixos_cap_t mq;
    size_t *received = (size_t *)&user_buffer[128];
    char *message = (char *)&user_buffer[0];
    char *output = (char *)&user_buffer[64];

    coverage_reset_kernel();
    memset(user_buffer, 0, sizeof(user_buffer));
    user = aixos_user_task_create_static("uapi", coverage_dummy_task, NULL,
                                         user_stack, sizeof(user_stack), 2,
                                         &user_tcb);
    CHECK(user != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(user);
    CHECK(tcb != NULL);
    if (tcb == NULL) {
        return;
    }
    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)user_buffer,
                                    sizeof(user_buffer),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_OK);
    aixos_sched_add_task(tcb);
    aixos_test_set_current(tcb);

    CHECK(aixos_user_yield() == AIXOS_OK);
    CHECK(aixos_user_sleep(0U) == AIXOS_OK);
    CHECK(aixos_user_clock_get() == aixos_get_tick());
    CHECK(aixos_test_syscall_invoke_fast(AIXOS_SC_CLOCK_GET,
                                         0U, 0U, 0U, 0U, 0U) ==
          (int32_t)aixos_get_tick());
    req.number = AIXOS_SC_SLEEP;
    req.args[0] = 0U;
    CHECK(aixos_syscall_dispatch(&req) == AIXOS_OK);
    req.number = AIXOS_SC_TASK_SELF;
    CHECK(aixos_syscall_dispatch(&req) == (int32_t)user);

    sem = aixos_user_sem_create(2);
    CHECK(sem >= 0);
    CHECK(aixos_user_sem_wait(sem, 0U) == AIXOS_OK);
    CHECK(aixos_user_sem_post(sem) == AIXOS_OK);
    CHECK(aixos_user_sem_delete(sem) == AIXOS_OK);

    message[0] = 'a';
    message[1] = 'i';
    mq = aixos_user_mq_create(2U, 8U);
    CHECK(mq >= 0);
    CHECK(aixos_user_mq_send(mq, message, 2U, 0U) == AIXOS_OK);
    *received = 0U;
    CHECK(aixos_user_mq_recv(mq, output, 8U, received, 0U) == AIXOS_OK);
    CHECK(*received == 2U);
    CHECK(output[0] == 'a' && output[1] == 'i');
    CHECK(aixos_user_mq_delete(mq) == AIXOS_OK);

    CHECK(aixos_user_task_exit() == AIXOS_OK);
    CHECK(aixos_task_count() == 0U);
    aixos_test_set_current(NULL);
}

void test_kernel_start_and_return_trap_edges(void)
{
    aixos_handle_t task;
    aixos_tcb_t *tcb;

    coverage_reset_kernel();
    task = aixos_task_create("ret", coverage_dummy_task, NULL, 512U, 2);
    CHECK(task != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(task);
    CHECK(tcb != NULL);
    if (tcb != NULL) {
        aixos_test_set_current(tcb);
        aixos_task_return_trap();
        CHECK(aixos_task_count() == 0U);
    }

    coverage_reset_kernel();
    aixos_start();
    CHECK(aixos_first_start == 1);
    CHECK(aixos_task_count() >= 2U);
    aixos_test_set_current(NULL);
}

void test_mutex_waiter_and_priority_edges(void)
{
    aixos_handle_t owner_task;
    aixos_handle_t waiter_task;
    aixos_handle_t mutex;
    aixos_tcb_t *owner;
    aixos_tcb_t *waiter;

    coverage_reset_kernel();
    owner_task = aixos_task_create("owner", coverage_dummy_task, NULL,
                                   512U, 1);
    waiter_task = aixos_task_create("waiter", coverage_dummy_task, NULL,
                                    512U, 5);
    CHECK(owner_task != AIXOS_HANDLE_INVALID);
    CHECK(waiter_task != AIXOS_HANDLE_INVALID);
    owner = aixos_tcb_from_handle(owner_task);
    waiter = aixos_tcb_from_handle(waiter_task);
    CHECK(owner != NULL && waiter != NULL);
    if (owner == NULL || waiter == NULL) {
        return;
    }
    aixos_sched_add_task(owner);
    aixos_sched_add_task(waiter);

    mutex = aixos_mutex_create();
    CHECK(mutex != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(owner);
    CHECK(aixos_mutex_lock(mutex, 0U) == AIXOS_OK);
    CHECK(owner->priority == 1);
    aixos_test_set_current(waiter);
    CHECK(aixos_mutex_lock(mutex, 5U) == AIXOS_OK);
    CHECK(waiter->state == AIXOS_TASK_BLOCKED);
    CHECK(owner->priority == 5);
    CHECK(aixos_mutex_task_can_delete(owner) == 0);
    aixos_test_set_current(owner);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_OK);
    CHECK(waiter->state == AIXOS_TASK_READY);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_ERR_INVAL);
    CHECK(owner->priority == 1);
    aixos_test_set_current(waiter);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_OK);

    CHECK(aixos_mutex_delete(mutex) == AIXOS_OK);
    CHECK(aixos_mutex_delete(mutex) == AIXOS_ERR_INVAL);

    mutex = aixos_mutex_create();
    CHECK(mutex != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(owner);
    CHECK(aixos_mutex_lock(mutex, 0U) == AIXOS_OK);
    aixos_test_set_current(waiter);
    CHECK(aixos_mutex_lock(mutex, 5U) == AIXOS_OK);
    aixos_test_set_current(owner);
    CHECK(aixos_mutex_delete(mutex) == AIXOS_ERR_BUSY);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_OK);
    CHECK(waiter->state == AIXOS_TASK_READY);
    aixos_test_set_current(waiter);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_OK);
    CHECK(aixos_mutex_delete(mutex) == AIXOS_OK);

    aixos_mutex_waiter_added(NULL);
    aixos_mutex_waiter_removed(NULL);
    aixos_mutex_task_priority_changed(NULL);
    aixos_test_set_current(NULL);
    CHECK(aixos_mutex_lock(AIXOS_HANDLE_INVALID, 0U) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_mutex_unlock(AIXOS_HANDLE_INVALID) == AIXOS_ERR_CONTEXT);

    CHECK(aixos_task_delete(owner_task) == AIXOS_OK);
    CHECK(aixos_task_delete(waiter_task) == AIXOS_OK);
}

void test_ipc_isr_and_parameter_edges(void)
{
    static uint8_t pipe_storage[4];
    static uint8_t mq_storage[12];
    static size_t mq_lengths[3];
    aixos_handle_t pipe;
    aixos_handle_t mq;
    aixos_handle_t event;
    aixos_handle_t task;
    aixos_tcb_t *tcb;
    uint8_t buffer[32] = { 0U };
    size_t size = 0U;
    uint32_t priority = 0U;
    uint32_t matched = 0U;

    coverage_reset_kernel();
    task = aixos_task_create("ipc", coverage_dummy_task, NULL, 512U, 2);
    CHECK(task != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(task);
    CHECK(tcb != NULL);
    if (tcb == NULL) {
        return;
    }
    aixos_test_set_current(tcb);

    CHECK(aixos_pipe_create_static(NULL, sizeof(pipe_storage)) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_create(0U) == AIXOS_HANDLE_INVALID);
    pipe = aixos_pipe_create_static(pipe_storage, sizeof(pipe_storage));
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_write(pipe, NULL, 1U, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_read(pipe, NULL, 1U, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_write(pipe, buffer,
                           AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U,
                           0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_read(pipe, buffer,
                          AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U,
                          0U) == AIXOS_ERR_INVAL);
    aixos_isr_enter();
    CHECK(aixos_pipe_write(pipe, buffer, 1U, 0U) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_pipe_read(pipe, buffer, 1U, 0U) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_pipe_write_from_isr(pipe, buffer, 1U) == 1);
    CHECK(aixos_pipe_write_from_isr(pipe, buffer,
                                    AIXOS_CFG_ISR_COPY_MAX_BYTES + 1U) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_ERR_CONTEXT);
    aixos_isr_exit();
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_ERR_INVAL);
    pipe = aixos_pipe_create_static(pipe_storage, sizeof(pipe_storage));
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    CHECK(aixos_test_pipe_add_waiter(pipe, task, 0) == AIXOS_OK);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_ERR_BUSY);
    CHECK(aixos_pipe_write(pipe, buffer, 1U, 0U) == 1);
    CHECK(tcb->state == AIXOS_TASK_READY);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);
    pipe = aixos_pipe_create_static(pipe_storage, sizeof(pipe_storage));
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_write(pipe, buffer, sizeof(pipe_storage), 0U) ==
          (int)sizeof(pipe_storage));
    CHECK(aixos_test_pipe_add_waiter(pipe, task, 1) == AIXOS_OK);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_ERR_BUSY);
    CHECK(aixos_pipe_read(pipe, buffer, 1U, 0U) == 1);
    CHECK(tcb->state == AIXOS_TASK_READY);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);

    CHECK(aixos_mq_create(0U, 1U) == AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_create_static(1U, 4U, NULL, mq_lengths) ==
          AIXOS_HANDLE_INVALID);
    mq = aixos_mq_create_static(3U, 4U, mq_storage, mq_lengths);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_send_priority(mq, buffer, 2U, 2U, 0U) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_mq_send_priority(mq, buffer, 2U, 0U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_send_priority(mq, buffer, 2U, 0U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_send_priority(mq, buffer, 2U, 0U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_ERR_BUSY);
    CHECK(aixos_mq_recv_priority(mq, buffer, 1U, &size,
                                 &priority, 0U) == AIXOS_ERR_OVERFLOW);
    CHECK(size == 2U);
    CHECK(aixos_mq_recv_priority(mq, buffer, sizeof(buffer), &size,
                                 &priority, 0U) == AIXOS_OK);
    CHECK(aixos_mq_get_info(AIXOS_HANDLE_INVALID,
                            (aixos_mq_info_t *)&buffer[0]) ==
          AIXOS_ERR_INVAL);
    aixos_isr_enter();
    CHECK(aixos_mq_send(mq, buffer, 1U, 0U) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_mq_send_from_isr(mq, buffer, 1U) == AIXOS_OK);
    CHECK(aixos_mq_recv_priority(mq, buffer, sizeof(buffer), &size,
                                 NULL, 0U) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_mq_delete(mq) == AIXOS_ERR_CONTEXT);
    aixos_isr_exit();
    while (aixos_mq_recv(mq, buffer, sizeof(buffer), &size, 0U) ==
           AIXOS_OK) {
    }
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);
    mq = aixos_mq_create_static(2U, 4U, mq_storage, mq_lengths);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    CHECK(aixos_test_mq_add_waiter(mq, task, 0) == AIXOS_OK);
    CHECK(aixos_mq_delete(mq) == AIXOS_ERR_BUSY);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_READY);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_OK);
    CHECK(aixos_test_mq_add_waiter(mq, task, 1) == AIXOS_OK);
    CHECK(aixos_mq_delete(mq) == AIXOS_ERR_BUSY);
    CHECK(aixos_mq_recv(mq, buffer, sizeof(buffer), &size, 0U) ==
          AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_READY);
    while (aixos_mq_recv(mq, buffer, sizeof(buffer), &size, 0U) ==
           AIXOS_OK) {
    }
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);

    event = aixos_event_create();
    CHECK(event != AIXOS_HANDLE_INVALID);
    CHECK(aixos_event_wait(event, 0U, AIXOS_EVENT_OR, 0U, &matched) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_event_clear(event, 0U) == AIXOS_OK);
    aixos_isr_enter();
    CHECK(aixos_event_create() == AIXOS_HANDLE_INVALID);
    CHECK(aixos_event_wait(event, 1U, AIXOS_EVENT_OR, 0U, &matched) ==
          AIXOS_ERR_CONTEXT);
    CHECK(aixos_event_clear(event, 1U) == AIXOS_ERR_CONTEXT);
    CHECK(aixos_event_delete(event) == AIXOS_ERR_CONTEXT);
    aixos_isr_exit();
    CHECK(aixos_event_delete(event) == AIXOS_OK);
    event = aixos_event_create();
    CHECK(event != AIXOS_HANDLE_INVALID);
    CHECK(aixos_test_event_add_waiter(event, task, 0x8U,
                                      AIXOS_EVENT_OR |
                                      AIXOS_EVENT_CLEAR) == AIXOS_OK);
    CHECK(aixos_event_delete(event) == AIXOS_ERR_BUSY);
    CHECK(aixos_event_set(event, 0x8U) == AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_READY);
    CHECK(tcb->pend_result == 0x8U);
    CHECK(aixos_event_delete(event) == AIXOS_OK);

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(task) == AIXOS_OK);
}

void test_heap_object_additional_edges(void)
{
    static unsigned char custom_heap[512] __attribute__((aligned(8)));
    void *first;
    void *second;
    void *third;
    int slot;
    int value = 7;
    aixos_handle_t handle;
    aixos_mem_info_t info;

    aixos_heap_init(custom_heap, sizeof(custom_heap));
    aixos_object_init();
    CHECK(aixos_pool_get_usage(-1) == 0);
    CHECK(aixos_pool_get_usage(AIXOS_POOL_COUNT) == 0);
    CHECK(aixos_slot_alloc(-1, &value) < 0);
    CHECK(aixos_slot_handle(-1, 0) == AIXOS_HANDLE_INVALID);
    aixos_slot_free(-1, 0);
    CHECK(aixos_obj_from_handle(AIXOS_HANDLE_INVALID,
                                AIXOS_OBJ_SEM) == NULL);
    CHECK(aixos_handle_is_valid(AIXOS_HANDLE_INVALID,
                                AIXOS_OBJ_SEM) == 0);
    CHECK(aixos_test_set_slot_generation(-1, 0, 1U) == AIXOS_ERR_INVAL);

    slot = aixos_slot_alloc(AIXOS_POOL_SEM, &value);
    CHECK(slot >= 0);
    handle = aixos_slot_handle(AIXOS_POOL_SEM, slot);
    CHECK(handle != AIXOS_HANDLE_INVALID);
    CHECK(aixos_pool_get_usage(AIXOS_POOL_SEM) == 1);
    CHECK(aixos_obj_from_handle(handle, AIXOS_OBJ_MUTEX) == NULL);
    CHECK(aixos_handle_is_valid(handle, AIXOS_OBJ_SEM) == 1);
    aixos_slot_free(AIXOS_POOL_SEM, slot);
    CHECK(aixos_test_set_slot_generation(AIXOS_POOL_SEM, slot,
                                         0x00FFFFFFU) == AIXOS_OK);
    slot = aixos_slot_alloc(AIXOS_POOL_SEM, &value);
    CHECK(slot >= 0);
    handle = aixos_slot_handle(AIXOS_POOL_SEM, slot);
    CHECK(AIXOS_HANDLE_GEN(handle) == 1U);
    aixos_slot_free(AIXOS_POOL_SEM, slot);

    first = aixos_malloc(32U);
    second = aixos_malloc(48U);
    CHECK(first != NULL);
    CHECK(second != NULL);
    memset(first, 0xA5, 32U);
    third = aixos_realloc(first, 96U);
    CHECK(third != NULL);
    if (third != NULL) {
        CHECK(((unsigned char *)third)[0] == 0xA5U);
    }
    CHECK(aixos_realloc(NULL, 16U) != NULL);
    CHECK(aixos_realloc(second, 0U) == NULL);
    CHECK(aixos_realloc((void *)custom_heap, 16U) == NULL);
    aixos_heap_lockdown();
    CHECK(aixos_heap_is_locked() == 1);
    CHECK(aixos_malloc(8U) == NULL);
    CHECK(aixos_kernel_malloc(8U) != NULL);
    CHECK(aixos_realloc(third, 128U) == NULL);
    aixos_mem_info(&info);
    CHECK(info.runtime_locked == 1U);
    aixos_mem_info(NULL);

    aixos_isr_enter();
    CHECK(aixos_malloc(8U) == NULL);
    aixos_free(third);
    aixos_isr_exit();
}

void test_ipc_waiter_edge_paths(void)
{
    static uint8_t static_pipe_storage[8];
    static uint8_t static_mq_storage[8];
    static size_t static_mq_lengths[2];
    aixos_handle_t waiter;
    aixos_handle_t peer;
    aixos_tcb_t *waiter_tcb;
    aixos_tcb_t *peer_tcb;
    aixos_handle_t event;
    aixos_handle_t pipe;
    aixos_handle_t mq;
    uint8_t buffer[16] = { 0U };
    size_t size = 0U;
    uint32_t matched = 0U;
    uint32_t notify_value = 0U;

    coverage_reset_kernel();
    waiter = aixos_task_create("waiter", coverage_dummy_task, NULL, 512U, 4);
    peer = aixos_task_create("peer", coverage_dummy_task, NULL, 512U, 3);
    CHECK(waiter != AIXOS_HANDLE_INVALID);
    CHECK(peer != AIXOS_HANDLE_INVALID);
    waiter_tcb = aixos_tcb_from_handle(waiter);
    peer_tcb = aixos_tcb_from_handle(peer);
    CHECK(waiter_tcb != NULL && peer_tcb != NULL);
    if (waiter_tcb == NULL || peer_tcb == NULL) {
        return;
    }
    aixos_sched_add_task(waiter_tcb);
    aixos_sched_add_task(peer_tcb);

    event = aixos_event_create();
    CHECK(event != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_event_wait(event, 0x3U, AIXOS_EVENT_AND | AIXOS_EVENT_CLEAR,
                           0U, &matched) == AIXOS_ERR_AGAIN);
    aixos_test_set_current(peer_tcb);
    CHECK(aixos_event_set(event, 0x1U) == AIXOS_OK);
    CHECK(aixos_event_set(event, 0x2U) == AIXOS_OK);
    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_event_wait(event, 0x3U, AIXOS_EVENT_AND, 0U,
                           &matched) == AIXOS_OK);
    CHECK(matched == 0x3U);
    CHECK(aixos_event_wait(event, 0x1U, AIXOS_EVENT_OR, 0U,
                           &matched) == AIXOS_OK);
    CHECK(matched == 0x1U);
    CHECK(aixos_event_set(event, 0x5U) == AIXOS_OK);
    CHECK(aixos_event_wait(event, 0x4U, AIXOS_EVENT_OR | AIXOS_EVENT_CLEAR,
                           0U, &matched) == AIXOS_OK);
    CHECK(matched == 0x4U);
    CHECK(aixos_event_delete(event) == AIXOS_OK);

    pipe = aixos_pipe_create_static(static_pipe_storage,
                                    sizeof(static_pipe_storage));
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_pipe_read(pipe, buffer, 4U, 0U) == 0);
    aixos_test_set_current(peer_tcb);
    buffer[0] = 1U;
    buffer[1] = 2U;
    CHECK(aixos_pipe_write(pipe, buffer, 2U, 0U) == 2);
    CHECK(aixos_pipe_write(pipe, buffer, sizeof(static_pipe_storage), 0U) ==
          (int)(sizeof(static_pipe_storage) - 2U));
    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_pipe_write(pipe, buffer, 1U, 0U) == 0);
    aixos_test_set_current(peer_tcb);
    CHECK(aixos_pipe_read(pipe, buffer, 1U, 0U) == 1);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);

    mq = aixos_mq_create_static(2U, 4U, static_mq_storage,
                                static_mq_lengths);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_mq_recv(mq, buffer, sizeof(buffer), &size, 0U) ==
          AIXOS_ERR_AGAIN);
    aixos_test_set_current(peer_tcb);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_ERR_BUSY);
    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_mq_send(mq, buffer, 2U, 0U) == AIXOS_ERR_BUSY);
    aixos_test_set_current(peer_tcb);
    CHECK(aixos_mq_recv(mq, buffer, sizeof(buffer), &size, 0U) == AIXOS_OK);
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);

    aixos_test_set_current(waiter_tcb);
    CHECK(aixos_task_notify_wait(0U, 0U, NULL, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_task_notify(waiter, 0x3U, AIXOS_NOTIFY_SET_BITS) == AIXOS_OK);
    CHECK(aixos_task_notify(waiter, 0x4U, AIXOS_NOTIFY_NO_OVERWRITE) ==
          AIXOS_ERR_BUSY);
    CHECK(aixos_task_notify_wait(0x1U, 0x2U, &notify_value, 0U) ==
          AIXOS_OK);
    CHECK((notify_value & 0x2U) != 0U);
    waiter_tcb->notify_value = UINT32_MAX;
    waiter_tcb->notify_pending = 0U;
    CHECK(aixos_task_notify(waiter, 0U, AIXOS_NOTIFY_INCREMENT) ==
          AIXOS_ERR_OVERFLOW);
    waiter_tcb->notify_value = 2U;
    waiter_tcb->notify_pending = 1U;
    CHECK(aixos_task_notify_take(0, 0U, &notify_value) == AIXOS_OK);
    CHECK(waiter_tcb->notify_value == 1U);
    CHECK(aixos_task_notify_take(1, 0U, &notify_value) == AIXOS_OK);
    CHECK(waiter_tcb->notify_value == 0U);
    CHECK(aixos_task_notify(waiter, 0U, (aixos_notify_action_t)99) ==
          AIXOS_ERR_INVAL);

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(waiter) == AIXOS_OK);
    CHECK(aixos_task_delete(peer) == AIXOS_OK);
}

void test_posix_named_wrapper_edges(void)
{
    sem_t *named_sem;
    sem_t *same_sem;
    struct mq_attr attr;
    mqd_t named_mq;
    mqd_t same_mq;
    int descriptors[2];
    char data[8] = { 0 };

    coverage_reset_kernel();

    CHECK(aixos_posix_sem_open_named(NULL, O_CREAT, 1U) == SEM_FAILED);
    CHECK(aixos_posix_sem_open_named("bad", O_CREAT, 1U) == SEM_FAILED);
    CHECK(aixos_posix_sem_open_named("/n", 0, 0U) == SEM_FAILED);
    named_sem = aixos_posix_sem_open_named("/n", O_CREAT, 1U);
    CHECK(named_sem != SEM_FAILED);
    same_sem = aixos_posix_sem_open_named("/n", O_CREAT, 1U);
    CHECK(same_sem == named_sem);
    CHECK(aixos_posix_sem_open_named("/n", O_CREAT | O_EXCL, 1U) ==
          SEM_FAILED);
    CHECK(aixos_posix_sem_unlink_named(NULL) == -1);
    CHECK(aixos_posix_sem_unlink_named("/missing") == -1);
    CHECK(aixos_posix_sem_unlink_named("/n") == 0);
    CHECK(aixos_posix_sem_close_named(named_sem) == 0);
    CHECK(aixos_posix_sem_close_named(same_sem) == 0);
    CHECK(aixos_posix_sem_close_named(same_sem) == -1);

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 2;
    attr.mq_msgsize = 4;
    CHECK(aixos_posix_mq_open_named(NULL, O_CREAT, &attr) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_posix_mq_open_named("bad", O_CREAT, &attr) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_posix_mq_open_named("/q", O_NONBLOCK, &attr) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_posix_mq_open_named("/q", 0, &attr) ==
          AIXOS_HANDLE_INVALID);
    named_mq = aixos_posix_mq_open_named("/q", O_CREAT, &attr);
    CHECK(named_mq != AIXOS_HANDLE_INVALID);
    same_mq = aixos_posix_mq_open_named("/q", O_CREAT, &attr);
    CHECK(same_mq == named_mq);
    CHECK(aixos_posix_mq_open_named("/q", O_CREAT | O_EXCL, &attr) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_posix_mq_unlink_named(NULL) == -1);
    CHECK(aixos_posix_mq_unlink_named("/missing") == -1);
    CHECK(aixos_posix_mq_unlink_named("/q") == 0);
    CHECK(aixos_posix_mq_close_named(named_mq) == 0);
    CHECK(aixos_posix_mq_close_named(same_mq) == 0);
    CHECK(aixos_posix_mq_close_named(same_mq) == -1);

    CHECK(aixos_posix_pipe_open(NULL) == -1);
    CHECK(aixos_posix_read(-1, data, sizeof(data)) == -1);
    CHECK(aixos_posix_write(-1, data, sizeof(data)) == -1);
    CHECK(aixos_posix_close(-1) == -1);
    CHECK(aixos_posix_pipe_open(descriptors) == 0);
    data[0] = 'o';
    data[1] = 'k';
    CHECK(aixos_posix_write(descriptors[1], data, 2U) == 2);
    memset(data, 0, sizeof(data));
    CHECK(aixos_posix_read(descriptors[0], data, sizeof(data)) == 2);
    CHECK(data[0] == 'o' && data[1] == 'k');
    CHECK(aixos_posix_write(descriptors[0], data, 1U) == -1);
    CHECK(aixos_posix_read(descriptors[1], data, 1U) == -1);
    CHECK(aixos_posix_close(descriptors[0]) == 0);
    CHECK(aixos_posix_close(descriptors[0]) == -1);
    CHECK(aixos_posix_close(descriptors[1]) == 0);
}
