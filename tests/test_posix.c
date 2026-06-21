#include "test.h"
#include "aixos/posix.h"
#include "posix/include/sched.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

static int once_calls;
static int timer_calls;
static int timer_value;

static void dummy_task(void *argument)
{
    (void)argument;
}

static void *dummy_pthread(void *argument)
{
    return argument;
}

static void once_init(void)
{
    once_calls++;
}

static void timer_callback(aixos_sigval_t value)
{
    timer_calls++;
    timer_value = value.sival_int;
}

void test_posix_compatibility(void)
{
    aixos_pthread_attr_t attr;
    aixos_pthread_mutexattr_t mutex_attr;
    aixos_pthread_condattr_t cond_attr;
    aixos_pthread_mutex_t mutex;
    aixos_pthread_cond_t condition;
    aixos_pthread_rwlock_t rwlock;
    aixos_pthread_rwlockattr_t rwlock_attr;
    aixos_pthread_barrier_t barrier;
    aixos_pthread_barrierattr_t barrier_attr;
    aixos_pthread_once_t once = AIXOS_PTHREAD_ONCE_INIT;
    aixos_pthread_key_t key;
    aixos_pthread_t thread;
    aixos_pthread_t other_thread;
    aixos_sem_posix_t sem;
    aixos_mqd_t queue;
    aixos_mq_attr_t mq_attr;
    aixos_timespec_t time;
    aixos_timer_posix_t software_timer;
    aixos_sigevent_t timer_event;
    aixos_itimerspec_t timer_setting;
    aixos_itimerspec_t timer_state;
    size_t stack_size;
    int value;
    int priority;
    int state;
    int type;
    int protocol;
    int clock_id;
    void *thread_result = NULL;
    char message[8];
    unsigned int message_priority = 99U;

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();
    aixos_trace_init();

    thread = aixos_task_create("posix-test", dummy_task, NULL, 256, 3);
    CHECK(thread != AIXOS_HANDLE_INVALID);
    other_thread = aixos_task_create("posix-other", dummy_task, NULL, 256, 2);
    CHECK(other_thread != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(aixos_tcb_from_handle(thread));

    CHECK(aixos_pthread_attr_init(&attr) == 0);
    CHECK(aixos_pthread_attr_setstacksize(
              &attr, AIXOS_CFG_MIN_TASK_STACK_SIZE) == 0);
    CHECK(aixos_pthread_attr_getstacksize(&attr, &stack_size) == 0);
    CHECK(stack_size == AIXOS_CFG_MIN_TASK_STACK_SIZE);
    CHECK(aixos_pthread_attr_setdetachstate(
              &attr, AIXOS_PTHREAD_CREATE_JOINABLE) == 0);
    CHECK(aixos_pthread_attr_getdetachstate(&attr, &state) == 0);
    CHECK(state == AIXOS_PTHREAD_CREATE_JOINABLE);
    CHECK(aixos_pthread_attr_setschedprio(&attr, 63) == 0);
    CHECK(aixos_pthread_attr_getschedprio(&attr, &priority) == 0);
    CHECK(priority == 63);
    CHECK(aixos_sched_get_priority_min(SCHED_RR) == 0);
    CHECK(aixos_sched_get_priority_max(SCHED_RR) ==
          AIXOS_CFG_MAX_PRIORITY - 1);
    CHECK(aixos_pthread_equal(aixos_pthread_self(), thread) != 0);

    CHECK(aixos_pthread_once(&once, once_init) == 0);
    CHECK(aixos_pthread_once(&once, once_init) == 0);
    CHECK(once_calls == 1);
    CHECK(aixos_pthread_key_create(&key, NULL) == 0);
    CHECK(aixos_pthread_setspecific(key, (void *)0x1234U) == 0);
    CHECK(aixos_pthread_getspecific(key) == (void *)0x1234U);
    CHECK(aixos_pthread_key_delete(key) == 0);
    CHECK(aixos_pthread_key_create(&key, NULL) == 0);
    CHECK(aixos_pthread_getspecific(key) == NULL);
    CHECK(aixos_pthread_key_delete(key) == 0);

    CHECK(aixos_pthread_mutexattr_init(&mutex_attr) == 0);
    CHECK(aixos_pthread_mutexattr_gettype(&mutex_attr, &type) == 0);
    CHECK(type == AIXOS_PTHREAD_MUTEX_RECURSIVE);
    CHECK(aixos_pthread_mutexattr_getprotocol(&mutex_attr, &protocol) == 0);
    CHECK(protocol == AIXOS_PTHREAD_PRIO_INHERIT);
    CHECK(aixos_pthread_mutexattr_settype(
              &mutex_attr, AIXOS_PTHREAD_MUTEX_NORMAL) == 0);
    CHECK(aixos_pthread_mutexattr_gettype(&mutex_attr, &type) == 0);
    CHECK(type == AIXOS_PTHREAD_MUTEX_NORMAL);
    CHECK(aixos_pthread_mutexattr_settype(
              &mutex_attr, AIXOS_PTHREAD_MUTEX_ERRORCHECK) == 0);
    CHECK(aixos_pthread_mutexattr_gettype(&mutex_attr, &type) == 0);
    CHECK(type == AIXOS_PTHREAD_MUTEX_ERRORCHECK);
    /* Switch back to RECURSIVE for the lock/unlock test */
    CHECK(aixos_pthread_mutexattr_settype(
              &mutex_attr, AIXOS_PTHREAD_MUTEX_RECURSIVE) == 0);
    CHECK(aixos_pthread_mutex_init(&mutex, &mutex_attr) == 0);
    CHECK(aixos_pthread_mutex_lock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_trylock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_unlock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_unlock(&mutex) == 0);
    CHECK(aixos_pthread_mutex_destroy(&mutex) == 0);

    CHECK(aixos_pthread_condattr_init(&cond_attr) == 0);
    CHECK(aixos_pthread_condattr_getclock(&cond_attr, &clock_id) == 0);
    CHECK(clock_id == AIXOS_CLOCK_MONOTONIC);
    CHECK(aixos_pthread_condattr_setclock(
              &cond_attr, AIXOS_CLOCK_REALTIME) == AIXOS_ENOTSUP);
    CHECK(aixos_pthread_cond_init(&condition, &cond_attr) == 0);
    CHECK(aixos_pthread_cond_signal(&condition) == 0);
    CHECK(aixos_pthread_cond_broadcast(&condition) == 0);
    CHECK(aixos_pthread_cond_destroy(&condition) == 0);

    CHECK(aixos_pthread_rwlockattr_init(&rwlock_attr) == 0);
    rwlock_attr.process_shared = AIXOS_PTHREAD_PROCESS_SHARED;
    CHECK(aixos_pthread_rwlock_init(&rwlock, &rwlock_attr) ==
          AIXOS_ENOTSUP);
    rwlock_attr.process_shared = AIXOS_PTHREAD_PROCESS_PRIVATE;
    CHECK(aixos_pthread_rwlock_init(&rwlock, &rwlock_attr) == 0);
    CHECK(aixos_pthread_rwlock_rdlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_tryrdlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_trywrlock(&rwlock) == AIXOS_EBUSY);
    aixos_test_set_current(aixos_tcb_from_handle(other_thread));
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == AIXOS_EPERM);
    CHECK(aixos_pthread_rwlock_trywrlock(&rwlock) == AIXOS_EBUSY);
    aixos_test_set_current(aixos_tcb_from_handle(thread));
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_wrlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_tryrdlock(&rwlock) == AIXOS_EBUSY);
    CHECK(aixos_pthread_rwlock_unlock(&rwlock) == 0);
    CHECK(aixos_pthread_rwlock_destroy(&rwlock) == 0);
    CHECK(aixos_pthread_rwlockattr_destroy(&rwlock_attr) == 0);

    CHECK(aixos_pthread_barrierattr_init(&barrier_attr) == 0);
    barrier_attr.process_shared = AIXOS_PTHREAD_PROCESS_SHARED;
    CHECK(aixos_pthread_barrier_init(&barrier, &barrier_attr, 1U) ==
          AIXOS_ENOTSUP);
    barrier_attr.process_shared = AIXOS_PTHREAD_PROCESS_PRIVATE;
    CHECK(aixos_pthread_barrier_init(&barrier, &barrier_attr, 1U) == 0);
    CHECK(aixos_pthread_barrier_wait(&barrier) ==
          AIXOS_PTHREAD_BARRIER_SERIAL_THREAD);
    CHECK(aixos_pthread_barrier_wait(&barrier) ==
          AIXOS_PTHREAD_BARRIER_SERIAL_THREAD);
    CHECK(barrier.generation == 2U);
    CHECK(aixos_pthread_barrier_destroy(&barrier) == 0);
    CHECK(aixos_pthread_barrierattr_destroy(&barrier_attr) == 0);

    CHECK(aixos_sem_posix_init(&sem, 1U) == 0);
    CHECK(aixos_sem_posix_getvalue(&sem, &value) == 0);
    CHECK(value == 1);
    CHECK(aixos_sem_posix_trywait(&sem) == 0);
    CHECK(aixos_sem_posix_trywait(&sem) == AIXOS_ETIMEDOUT);
    CHECK(aixos_sem_posix_post(&sem) == 0);
    CHECK(aixos_sem_posix_destroy(&sem) == 0);

    aixos_test_set_sched_stats(2500U, 1000U, 8U);
    CHECK(aixos_clock_gettime(AIXOS_CLOCK_MONOTONIC, &time) == 0);
    CHECK(time.tv_sec == 2);
    CHECK(time.tv_nsec == 500000000);
    CHECK(aixos_clock_getres(AIXOS_CLOCK_MONOTONIC, &time) == 0);
    CHECK(time.tv_nsec == 1000000);
    CHECK(aixos_clock_gettime(AIXOS_CLOCK_REALTIME, &time) ==
          AIXOS_ENOTSUP);

    memset(&timer_event, 0, sizeof(timer_event));
    timer_event.notify = AIXOS_SIGEV_THREAD;
    timer_event.value.sival_int = 42;
    timer_event.function = timer_callback;
    CHECK(aixos_timer_posix_create(AIXOS_CLOCK_MONOTONIC, &timer_event,
                                   &software_timer) == 0);
    timer_setting.value.tv_sec = 0;
    timer_setting.value.tv_nsec = 5000000;
    timer_setting.interval.tv_sec = 0;
    timer_setting.interval.tv_nsec = 3000000;
    CHECK(aixos_timer_posix_settime(software_timer, 0, &timer_setting,
                                    NULL) == 0);
    CHECK(aixos_timer_posix_gettime(software_timer, &timer_state) == 0);
    CHECK(timer_state.value.tv_nsec == 5000000);
    CHECK(timer_state.interval.tv_nsec == 3000000);
    aixos_test_set_sched_stats(2505U, 1000U, 8U);
    aixos_timer_tick(2505U);
    CHECK(aixos_timer_dispatch() == 1U);
    CHECK(timer_calls == 1 && timer_value == 42);
    aixos_test_set_sched_stats(2514U, 1000U, 8U);
    aixos_timer_tick(2514U);
    CHECK(aixos_timer_dispatch() == 1U);
    CHECK(timer_calls == 2);
    CHECK(aixos_timer_posix_getoverrun(software_timer) == 2);
    memset(&timer_setting, 0, sizeof(timer_setting));
    CHECK(aixos_timer_posix_settime(software_timer, 0, &timer_setting,
                                    &timer_state) == 0);
    CHECK(timer_state.interval.tv_nsec == 3000000);
    CHECK(aixos_timer_posix_gettime(software_timer, &timer_state) == 0);
    CHECK(timer_state.value.tv_sec == 0 && timer_state.value.tv_nsec == 0);
    CHECK(aixos_timer_posix_delete(software_timer) == 0);
    CHECK(aixos_timer_posix_gettime(software_timer, &timer_state) ==
          AIXOS_EINVAL);

    queue = aixos_mq_posix_open(4, sizeof(message));
    CHECK(queue != AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_posix_send(queue, "l", 2U, 1U) == 0);
    CHECK(aixos_mq_posix_send(queue, "a", 2U, 7U) == 0);
    CHECK(aixos_mq_posix_send(queue, "b", 2U, 7U) == 0);
    CHECK(aixos_mq_posix_send(queue, "m", 2U, 3U) == 0);
    CHECK(aixos_mq_posix_getattr(queue, &mq_attr) == 0);
    CHECK(mq_attr.mq_maxmsg == 4);
    CHECK(mq_attr.mq_msgsize == (long)sizeof(message));
    CHECK(mq_attr.mq_curmsgs == 4);
    memset(message, 0, sizeof(message));
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'a' && message_priority == 7U);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'b' && message_priority == 7U);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'm' && message_priority == 3U);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'l' && message_priority == 1U);
    CHECK(aixos_mq_posix_send(queue, "x", 2U,
                              AIXOS_CFG_MQ_PRIORITY_MAX + 1U) ==
          AIXOS_EINVAL);
    CHECK(aixos_mq_posix_close(queue) == 0);

    queue = aixos_mq_posix_open(3, sizeof(message));
    CHECK(queue != AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_posix_send(queue, "a", 2U, 1U) == 0);
    CHECK(aixos_mq_posix_send(queue, "b", 2U, 2U) == 0);
    CHECK(aixos_mq_posix_send(queue, "c", 2U, 3U) == 0);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'c' && message_priority == 3U);
    CHECK(aixos_mq_posix_send(queue, "d", 2U, 4U) == 0);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'd' && message_priority == 4U);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'b' && message_priority == 2U);
    CHECK(aixos_mq_posix_receive(queue, message, sizeof(message),
                                 &message_priority) == 2);
    CHECK(message[0] == 'a' && message_priority == 1U);
    CHECK(aixos_mq_posix_close(queue) == 0);

    CHECK(aixos_pthread_create(&thread, &attr, dummy_pthread,
                               (void *)0x5678U) == 0);
    CHECK(aixos_pthread_getschedprio(thread, &priority) == 0);
    CHECK(priority == 63);
    CHECK(aixos_posix_test_complete(thread, (void *)0x5678U) == 0);
    CHECK(aixos_pthread_join(thread, &thread_result) == 0);
    CHECK(thread_result == (void *)0x5678U);
    CHECK(aixos_task_delete(thread) == AIXOS_OK);

    CHECK(aixos_pthread_attr_destroy(&attr) == 0);
    aixos_test_set_current(NULL);
}
