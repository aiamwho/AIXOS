#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include "aixos/aixos.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

extern int test_failures;
extern int test_checks;

#define PUBLIC_CHECK(expression) do { \
    test_checks++; \
    if (!(expression)) test_failures++; \
} while (0)

static void public_dummy_task(void *argument)
{
    (void)argument;
}

static int public_timer_calls;

static void public_timer_callback(union sigval value)
{
    public_timer_calls += value.sival_int;
}

void test_posix_public_api(void)
{
    pthread_attr_t thread_attr;
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutex_attr;
    sem_t semaphore;
    sem_t *named_semaphore;
    struct sched_param parameter;
    struct timespec time_value = { 0, 0 };
    struct sigevent timer_event;
    struct itimerspec timer_setting;
    struct itimerspec timer_state = { { 0, 0 }, { 0, 0 } };
    timer_t software_timer;
    struct mq_attr queue_attr;
    mqd_t queue;
    size_t stack_size;
    int value;
    char message[8];
    unsigned int priority;
    int pipe_descriptors[2];

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    aixos_handle_t pub_task = aixos_task_create("public", public_dummy_task, NULL, 256, 3);
    PUBLIC_CHECK(pub_task != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(aixos_tcb_from_handle(pub_task));
    PUBLIC_CHECK(pthread_attr_init(&thread_attr) == 0);
    parameter.sched_priority = 63;
    PUBLIC_CHECK(pthread_attr_setschedparam(&thread_attr, &parameter) == 0);
    parameter.sched_priority = 0;
    PUBLIC_CHECK(pthread_attr_getschedparam(&thread_attr, &parameter) == 0);
    PUBLIC_CHECK(parameter.sched_priority == 63);
    PUBLIC_CHECK(pthread_attr_getstacksize(&thread_attr, &stack_size) == 0);
    PUBLIC_CHECK(stack_size == AIXOS_CFG_DEFAULT_STACK_SIZE);
    PUBLIC_CHECK(sched_get_priority_min(SCHED_FIFO) == 0);
    PUBLIC_CHECK(sched_get_priority_max(SCHED_RR) ==
                 AIXOS_CFG_MAX_PRIORITY - 1);

    PUBLIC_CHECK(pthread_mutexattr_init(&mutex_attr) == 0);
    PUBLIC_CHECK(pthread_mutexattr_setpshared(
        &mutex_attr, PTHREAD_PROCESS_SHARED) == ENOTSUP);
    PUBLIC_CHECK(pthread_mutex_init(&mutex, &mutex_attr) == 0);
    PUBLIC_CHECK(pthread_mutex_lock(&mutex) == 0);
    PUBLIC_CHECK(pthread_mutex_unlock(&mutex) == 0);
    PUBLIC_CHECK(pthread_mutex_destroy(&mutex) == 0);

    PUBLIC_CHECK(sem_init(&semaphore, 0, 1U) == 0);
    PUBLIC_CHECK(sem_getvalue(&semaphore, &value) == 0);
    PUBLIC_CHECK(value == 1);
    PUBLIC_CHECK(sem_wait(&semaphore) == 0);
    PUBLIC_CHECK(sem_trywait(&semaphore) == -1);
    PUBLIC_CHECK(errno == ETIMEDOUT);
    PUBLIC_CHECK(sem_post(&semaphore) == 0);
    PUBLIC_CHECK(sem_destroy(&semaphore) == 0);

    named_semaphore = sem_open("/gate", O_CREAT | O_EXCL, 0U, 2U);
    PUBLIC_CHECK(named_semaphore != SEM_FAILED);
    PUBLIC_CHECK(sem_wait(named_semaphore) == 0);
    PUBLIC_CHECK(sem_getvalue(named_semaphore, &value) == 0);
    PUBLIC_CHECK(value == 1);
    PUBLIC_CHECK(sem_close(named_semaphore) == 0);
    named_semaphore = sem_open("/gate", 0);
    PUBLIC_CHECK(named_semaphore != SEM_FAILED);
    PUBLIC_CHECK(sem_unlink("/gate") == 0);
    PUBLIC_CHECK(sem_open("/gate", 0) == SEM_FAILED);
    PUBLIC_CHECK(errno == ENOENT);
    PUBLIC_CHECK(sem_close(named_semaphore) == 0);

    aixos_test_set_sched_stats(1500U, 0U, 0U);
    PUBLIC_CHECK(clock_gettime(CLOCK_MONOTONIC, &time_value) == 0);
    PUBLIC_CHECK(time_value.tv_sec == 1);
    PUBLIC_CHECK(time_value.tv_nsec == 500000000L);

    memset(&timer_event, 0, sizeof(timer_event));
    timer_event.sigev_notify = SIGEV_THREAD;
    timer_event.sigev_value.sival_int = 3;
    timer_event.sigev_notify_function = public_timer_callback;
    PUBLIC_CHECK(timer_create(CLOCK_MONOTONIC, &timer_event,
                              &software_timer) == 0);
    memset(&timer_setting, 0, sizeof(timer_setting));
    timer_setting.it_value.tv_nsec = 2000000L;
    PUBLIC_CHECK(timer_settime(software_timer, 0, &timer_setting,
                               NULL) == 0);
    PUBLIC_CHECK(timer_gettime(software_timer, &timer_state) == 0);
    PUBLIC_CHECK(timer_state.it_value.tv_nsec == 2000000L);
    aixos_test_set_sched_stats(1502U, 0U, 0U);
    aixos_timer_tick(1502U);
    PUBLIC_CHECK(aixos_timer_dispatch() == 1U);
    PUBLIC_CHECK(public_timer_calls == 3);
    PUBLIC_CHECK(timer_gettime(software_timer, &timer_state) == 0);
    PUBLIC_CHECK(timer_state.it_value.tv_sec == 0 &&
                 timer_state.it_value.tv_nsec == 0);
    PUBLIC_CHECK(timer_getoverrun(software_timer) == 0);
    PUBLIC_CHECK(timer_delete(software_timer) == 0);

    queue_attr.mq_flags = 0;
    queue_attr.mq_maxmsg = 3;
    queue_attr.mq_msgsize = sizeof(message);
    queue_attr.mq_curmsgs = 0;
    queue = mq_open("/public", O_CREAT | O_EXCL, 0U, &queue_attr);
    PUBLIC_CHECK(queue != AIXOS_HANDLE_INVALID);
    PUBLIC_CHECK(mq_send(queue, "lo", 3U, 1U) == 0);
    PUBLIC_CHECK(mq_send(queue, "hi", 3U, 9U) == 0);
    PUBLIC_CHECK(mq_getattr(queue, &queue_attr) == 0);
    PUBLIC_CHECK(queue_attr.mq_curmsgs == 2);
    PUBLIC_CHECK(mq_receive(queue, message, sizeof(message), &priority) == 3);
    PUBLIC_CHECK(message[0] == 'h' && priority == 9U);
    PUBLIC_CHECK(mq_close(queue) == 0);
    queue = mq_open("/public", O_RDWR);
    PUBLIC_CHECK(queue != AIXOS_HANDLE_INVALID);
    PUBLIC_CHECK(mq_unlink("/public") == 0);
    PUBLIC_CHECK(mq_open("/public", 0) == AIXOS_HANDLE_INVALID);
    PUBLIC_CHECK(errno == ENOENT);
    PUBLIC_CHECK(mq_close(queue) == 0);

    PUBLIC_CHECK(pipe(pipe_descriptors) == 0);
    PUBLIC_CHECK(write(pipe_descriptors[1], "xy", 2U) == 2);
    PUBLIC_CHECK(read(pipe_descriptors[0], message, sizeof(message)) == 2);
    PUBLIC_CHECK(message[0] == 'x' && message[1] == 'y');
    PUBLIC_CHECK(read(pipe_descriptors[1], message, sizeof(message)) == -1);
    PUBLIC_CHECK(errno == EBADF);
    PUBLIC_CHECK(close(pipe_descriptors[0]) == 0);
    PUBLIC_CHECK(close(pipe_descriptors[1]) == 0);

    PUBLIC_CHECK(pthread_attr_destroy(&thread_attr) == 0);
    aixos_test_set_current(NULL);
}
