#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

int aixos_posix_api_compile_probe(
    pthread_attr_t *thread_attr, pthread_mutex_t *mutex, sem_t *semaphore,
    mqd_t queue, int pipe_descriptors[2], char *buffer, size_t capacity)
{
    struct sched_param scheduling = { 1 };
    struct timespec timeout = { 0, 1000000L };
    struct sigevent timer_event = { 0 };
    struct itimerspec timer_value = { { 0, 0 }, { 0, 1000000L } };
    struct mq_attr attributes;
    timer_t timer;
    unsigned int priority = 0U;

    (void)pthread_attr_setschedparam(thread_attr, &scheduling);
    (void)pthread_mutex_timedlock(mutex, &timeout);
    (void)sem_timedwait(semaphore, &timeout);
    (void)mq_getattr(queue, &attributes);
    (void)mq_timedreceive(queue, buffer, capacity, &priority, &timeout);
    (void)clock_gettime(CLOCK_MONOTONIC, &timeout);
    timer_event.sigev_notify = SIGEV_NONE;
    (void)timer_create(CLOCK_MONOTONIC, &timer_event, &timer);
    (void)timer_settime(timer, 0, &timer_value, NULL);
    (void)timer_gettime(timer, &timer_value);
    (void)timer_getoverrun(timer);
    (void)timer_delete(timer);
    (void)pipe(pipe_descriptors);
    (void)read(pipe_descriptors[0], buffer, capacity);
    return sched_yield();
}
