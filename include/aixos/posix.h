#ifndef AIXOS_POSIX_H
#define AIXOS_POSIX_H

#include <stddef.h>
#include <stdint.h>
#include "aixos/aixos.h"

/* ── POSIX errno definitions ──────────────────────────────────────────── */

#define AIXOS_EPERM           1
#define AIXOS_ENOENT          2
#define AIXOS_ESRCH           3
#define AIXOS_EINTR           4
#define AIXOS_EIO             5
#define AIXOS_ENXIO           6
#define AIXOS_E2BIG           7
#define AIXOS_ENOEXEC         8
#define AIXOS_EBADF           9
#define AIXOS_ECHILD          10
#define AIXOS_EAGAIN          11
#define AIXOS_ENOMEM          12
#define AIXOS_EACCES          13
#define AIXOS_EFAULT          14
#define AIXOS_EBUSY           16
#define AIXOS_EEXIST          17
#define AIXOS_EXDEV           18
#define AIXOS_ENODEV          19
#define AIXOS_ENOTDIR         20
#define AIXOS_EISDIR          21
#define AIXOS_EINVAL          22
#define AIXOS_ENFILE          23
#define AIXOS_EMFILE          24
#define AIXOS_ENOSPC          28
#define AIXOS_ESPIPE          29
#define AIXOS_EROFS           30
#define AIXOS_ENAMETOOLONG    36
#define AIXOS_EDEADLK         35
#define AIXOS_ENOSYS          38
#define AIXOS_ENOTEMPTY       39
#define AIXOS_ELOOP           42
#define AIXOS_ENOTSUP         45
#define AIXOS_ETIMEDOUT       60
#define AIXOS_EPROTO          71

/* ── POSIX thread type definitions ────────────────────────────────────── */

typedef aixos_handle_t aixos_pthread_t;
typedef uint32_t aixos_pthread_key_t;
typedef aixos_handle_t aixos_sem_posix_t;
typedef aixos_handle_t aixos_mqd_t;
typedef int32_t aixos_timer_posix_t;

typedef struct {
    size_t stack_size;
    int priority;
    int detach_state;
    int sched_policy;
    int inheritsched;
} aixos_pthread_attr_t;

typedef struct {
    aixos_handle_t native;   /* kernel mutex handle */
    uint8_t type;
    aixos_pthread_t owner;
    uint16_t lock_count;
    uint8_t protocol;
    int prio_ceiling;
} aixos_pthread_mutex_t;

typedef struct {
    int type;
    int protocol;
    int prio_ceiling;
} aixos_pthread_mutexattr_t;

typedef struct {
    aixos_handle_t semaphore;
    uint32_t waiters;
} aixos_pthread_cond_t;

typedef struct {
    int clock_id;
} aixos_pthread_condattr_t;

typedef struct {
    aixos_pthread_mutex_t mutex;
    aixos_pthread_cond_t readers_condition;
    aixos_pthread_cond_t writers_condition;
    uint32_t readers;
    uint32_t waiting_writers;
    aixos_pthread_t writer;
    aixos_pthread_t reader_threads[AIXOS_CFG_POSIX_RWLOCK_READERS];
    uint32_t reader_counts[AIXOS_CFG_POSIX_RWLOCK_READERS];
} aixos_pthread_rwlock_t;

typedef struct {
    int process_shared;
} aixos_pthread_rwlockattr_t;

typedef struct {
    aixos_pthread_mutex_t mutex;
    aixos_pthread_cond_t condition;
    uint32_t threshold;
    uint32_t count;
    uint32_t generation;
} aixos_pthread_barrier_t;

typedef struct {
    int process_shared;
} aixos_pthread_barrierattr_t;

typedef struct {
    volatile uint32_t state;
} aixos_pthread_once_t;

/* ── POSIX time types (with include guards for redefinition safety) ───── */

#ifndef AIXOS_TIMESPEC_DEFINED
#define AIXOS_TIMESPEC_DEFINED
typedef struct {
    int64_t tv_sec;
    long tv_nsec;
} aixos_timespec_t;
#endif /* AIXOS_TIMESPEC_DEFINED */

#ifndef AIXOS_SIGVAL_DEFINED
#define AIXOS_SIGVAL_DEFINED
union aixos_sigval {
    int sival_int;
    void *sival_ptr;
};
typedef union aixos_sigval aixos_sigval_t;
#endif /* AIXOS_SIGVAL_DEFINED */

#ifndef AIXOS_SIGEVENT_DEFINED
#define AIXOS_SIGEVENT_DEFINED
typedef struct {
    int notify;
    aixos_sigval_t value;
    void (*function)(aixos_sigval_t);
} aixos_sigevent_t;
#endif /* AIXOS_SIGEVENT_DEFINED */

#ifndef AIXOS_ITIMERSPEC_DEFINED
#define AIXOS_ITIMERSPEC_DEFINED
typedef struct {
    aixos_timespec_t interval;
    aixos_timespec_t value;
} aixos_itimerspec_t;
#endif /* AIXOS_ITIMERSPEC_DEFINED */

#ifndef AIXOS_MQ_ATTR_DEFINED
#define AIXOS_MQ_ATTR_DEFINED
typedef struct {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
} aixos_mq_attr_t;
#endif /* AIXOS_MQ_ATTR_DEFINED */

/* ── POSIX constant definitions ───────────────────────────────────────── */

/* Thread create / detach states */
#define AIXOS_PTHREAD_CREATE_JOINABLE  0
#define AIXOS_PTHREAD_CREATE_DETACHED  1

/* Mutex types */
#define AIXOS_PTHREAD_MUTEX_NORMAL     0
#define AIXOS_PTHREAD_MUTEX_RECURSIVE  1
#define AIXOS_PTHREAD_MUTEX_ERRORCHECK 2

/* Mutex protocols */
#define AIXOS_PTHREAD_PRIO_NONE        0
#define AIXOS_PTHREAD_PRIO_INHERIT     1
#define AIXOS_PTHREAD_PRIO_PROTECT     2

/* Scheduling inheritance */
#define AIXOS_PTHREAD_INHERIT_SCHED    0
#define AIXOS_PTHREAD_EXPLICIT_SCHED   1

/* Process sharing */
#define AIXOS_PTHREAD_PROCESS_PRIVATE  0
#define AIXOS_PTHREAD_PROCESS_SHARED   1

/* Once and barrier */
#define AIXOS_PTHREAD_ONCE_INIT        { 0U }
#define AIXOS_PTHREAD_BARRIER_SERIAL_THREAD 1

/* Clock and timer */
#define AIXOS_CLOCK_REALTIME           0
#define AIXOS_CLOCK_MONOTONIC          1
#define AIXOS_TIMER_ABSTIME            1

/* Signal event notification */
#define AIXOS_SIGEV_NONE               0
#define AIXOS_SIGEV_SIGNAL             1
#define AIXOS_SIGEV_THREAD             2

/* ── POSIX thread attribute functions ─────────────────────────────────── */

int aixos_pthread_attr_init(aixos_pthread_attr_t *attr);
int aixos_pthread_attr_destroy(aixos_pthread_attr_t *attr);
int aixos_pthread_attr_setstacksize(aixos_pthread_attr_t *attr, size_t size);
int aixos_pthread_attr_getstacksize(const aixos_pthread_attr_t *attr,
                                    size_t *size);
int aixos_pthread_attr_setdetachstate(aixos_pthread_attr_t *attr, int state);
int aixos_pthread_attr_getdetachstate(const aixos_pthread_attr_t *attr,
                                      int *state);
int aixos_pthread_attr_setschedpolicy(aixos_pthread_attr_t *attr,
                                      int policy);
int aixos_pthread_attr_getschedpolicy(const aixos_pthread_attr_t *attr,
                                      int *policy);
int aixos_pthread_attr_setinheritsched(aixos_pthread_attr_t *attr,
                                       int inheritsched);
int aixos_pthread_attr_getinheritsched(const aixos_pthread_attr_t *attr,
                                       int *inheritsched);
int aixos_pthread_attr_setschedprio(aixos_pthread_attr_t *attr, int priority);
int aixos_pthread_attr_getschedprio(const aixos_pthread_attr_t *attr,
                                    int *priority);

/* ── POSIX thread functions ───────────────────────────────────────────── */

int aixos_pthread_create(aixos_pthread_t *thread,
                         const aixos_pthread_attr_t *attr,
                         void *(*start_routine)(void *), void *arg);
int aixos_pthread_join(aixos_pthread_t thread, void **value);
int aixos_pthread_detach(aixos_pthread_t thread);
aixos_pthread_t aixos_pthread_self(void);
int aixos_pthread_equal(aixos_pthread_t left, aixos_pthread_t right);
#ifdef AIXOS_HOST_TEST
void aixos_pthread_exit(void *value);
#else
void aixos_pthread_exit(void *value) __attribute__((noreturn));
#endif
int aixos_pthread_getschedprio(aixos_pthread_t thread, int *priority);
int aixos_pthread_setschedprio(aixos_pthread_t thread, int priority);
int aixos_sched_yield(void);
int aixos_sched_get_priority_min(int policy);
int aixos_sched_get_priority_max(int policy);

/* ── POSIX thread-specific data ───────────────────────────────────────── */

int aixos_pthread_once(aixos_pthread_once_t *once_control,
                       void (*init_routine)(void));
int aixos_pthread_key_create(aixos_pthread_key_t *key,
                             void (*destructor)(void *));
int aixos_pthread_key_delete(aixos_pthread_key_t key);
void *aixos_pthread_getspecific(aixos_pthread_key_t key);
int aixos_pthread_setspecific(aixos_pthread_key_t key, const void *value);

/* ── POSIX mutex attribute functions ──────────────────────────────────── */

int aixos_pthread_mutexattr_init(aixos_pthread_mutexattr_t *attr);
int aixos_pthread_mutexattr_destroy(aixos_pthread_mutexattr_t *attr);
int aixos_pthread_mutexattr_settype(aixos_pthread_mutexattr_t *attr, int type);
int aixos_pthread_mutexattr_gettype(const aixos_pthread_mutexattr_t *attr,
                                    int *type);
int aixos_pthread_mutexattr_setprotocol(aixos_pthread_mutexattr_t *attr,
                                        int protocol);
int aixos_pthread_mutexattr_getprotocol(
    const aixos_pthread_mutexattr_t *attr, int *protocol);
int aixos_pthread_mutexattr_setprioceiling(
    aixos_pthread_mutexattr_t *attr, int prioceiling);
int aixos_pthread_mutexattr_getprioceiling(
    const aixos_pthread_mutexattr_t *attr, int *prioceiling);

/* ── POSIX mutex functions ────────────────────────────────────────────── */

int aixos_pthread_mutex_init(aixos_pthread_mutex_t *mutex,
                             const aixos_pthread_mutexattr_t *attr);
int aixos_pthread_mutex_destroy(aixos_pthread_mutex_t *mutex);
int aixos_pthread_mutex_lock(aixos_pthread_mutex_t *mutex);
int aixos_pthread_mutex_trylock(aixos_pthread_mutex_t *mutex);
int aixos_pthread_mutex_timedlock(aixos_pthread_mutex_t *mutex,
                                  const aixos_timespec_t *absolute);
int aixos_pthread_mutex_unlock(aixos_pthread_mutex_t *mutex);

/* ── POSIX condition variable functions ────────────────────────────────── */

int aixos_pthread_cond_init(aixos_pthread_cond_t *condition,
                            const aixos_pthread_condattr_t *attr);
int aixos_pthread_condattr_init(aixos_pthread_condattr_t *attr);
int aixos_pthread_condattr_destroy(aixos_pthread_condattr_t *attr);
int aixos_pthread_condattr_setclock(aixos_pthread_condattr_t *attr,
                                    int clock_id);
int aixos_pthread_condattr_getclock(const aixos_pthread_condattr_t *attr,
                                    int *clock_id);
int aixos_pthread_cond_destroy(aixos_pthread_cond_t *condition);
int aixos_pthread_cond_wait(aixos_pthread_cond_t *condition,
                            aixos_pthread_mutex_t *mutex);
int aixos_pthread_cond_timedwait(aixos_pthread_cond_t *condition,
                                 aixos_pthread_mutex_t *mutex,
                                 const aixos_timespec_t *absolute);
int aixos_pthread_cond_signal(aixos_pthread_cond_t *condition);
int aixos_pthread_cond_broadcast(aixos_pthread_cond_t *condition);

/* ── POSIX read-write lock functions ──────────────────────────────────── */

int aixos_pthread_rwlockattr_init(aixos_pthread_rwlockattr_t *attr);
int aixos_pthread_rwlockattr_destroy(aixos_pthread_rwlockattr_t *attr);
int aixos_pthread_rwlock_init(aixos_pthread_rwlock_t *lock,
                              const aixos_pthread_rwlockattr_t *attr);
int aixos_pthread_rwlock_destroy(aixos_pthread_rwlock_t *lock);
int aixos_pthread_rwlock_rdlock(aixos_pthread_rwlock_t *lock);
int aixos_pthread_rwlock_tryrdlock(aixos_pthread_rwlock_t *lock);
int aixos_pthread_rwlock_timedrdlock(aixos_pthread_rwlock_t *lock,
                                     const aixos_timespec_t *absolute);
int aixos_pthread_rwlock_wrlock(aixos_pthread_rwlock_t *lock);
int aixos_pthread_rwlock_trywrlock(aixos_pthread_rwlock_t *lock);
int aixos_pthread_rwlock_timedwrlock(aixos_pthread_rwlock_t *lock,
                                     const aixos_timespec_t *absolute);
int aixos_pthread_rwlock_unlock(aixos_pthread_rwlock_t *lock);

/* ── POSIX barrier functions ──────────────────────────────────────────── */

int aixos_pthread_barrierattr_init(aixos_pthread_barrierattr_t *attr);
int aixos_pthread_barrierattr_destroy(aixos_pthread_barrierattr_t *attr);
int aixos_pthread_barrier_init(aixos_pthread_barrier_t *barrier,
                               const aixos_pthread_barrierattr_t *attr,
                               unsigned int count);
int aixos_pthread_barrier_destroy(aixos_pthread_barrier_t *barrier);
int aixos_pthread_barrier_wait(aixos_pthread_barrier_t *barrier);

/* ── POSIX semaphore functions ────────────────────────────────────────── */

int aixos_sem_posix_init(aixos_sem_posix_t *sem, unsigned int value);
int aixos_sem_posix_destroy(aixos_sem_posix_t *sem);
int aixos_sem_posix_wait(aixos_sem_posix_t *sem);
int aixos_sem_posix_trywait(aixos_sem_posix_t *sem);
int aixos_sem_posix_timedwait(aixos_sem_posix_t *sem,
                              const aixos_timespec_t *absolute);
int aixos_sem_posix_post(aixos_sem_posix_t *sem);
int aixos_sem_posix_getvalue(aixos_sem_posix_t *sem, int *value);

/* ── POSIX clock and timer functions ──────────────────────────────────── */

int aixos_clock_gettime(int clock_id, aixos_timespec_t *time);
int aixos_clock_getres(int clock_id, aixos_timespec_t *resolution);
int aixos_nanosleep(const aixos_timespec_t *request,
                    aixos_timespec_t *remaining);
int aixos_clock_nanosleep(int clock_id, int absolute,
                          const aixos_timespec_t *request);
int aixos_timer_posix_create(int clock_id, const aixos_sigevent_t *event,
                             aixos_timer_posix_t *timer);
int aixos_timer_posix_delete(aixos_timer_posix_t timer);
int aixos_timer_posix_settime(aixos_timer_posix_t timer, int flags,
                              const aixos_itimerspec_t *value,
                              aixos_itimerspec_t *old_value);
int aixos_timer_posix_gettime(aixos_timer_posix_t timer,
                              aixos_itimerspec_t *value);
int aixos_timer_posix_getoverrun(aixos_timer_posix_t timer);

/* ── POSIX message queue functions ────────────────────────────────────── */

aixos_mqd_t aixos_mq_posix_open(long max_messages, long message_size);
int aixos_mq_posix_close(aixos_mqd_t queue);
int aixos_mq_posix_send(aixos_mqd_t queue, const char *message,
                        size_t length, unsigned int priority);
int aixos_mq_posix_timedsend(aixos_mqd_t queue, const char *message,
                             size_t length, unsigned int priority,
                             const aixos_timespec_t *absolute);
int aixos_mq_posix_receive(aixos_mqd_t queue, char *message,
                           size_t capacity, unsigned int *priority);
int aixos_mq_posix_timedreceive(aixos_mqd_t queue, char *message,
                                size_t capacity, unsigned int *priority,
                                const aixos_timespec_t *absolute);
int aixos_mq_posix_getattr(aixos_mqd_t queue, aixos_mq_attr_t *attr);
int aixos_mq_posix_setattr(aixos_mqd_t queue,
                           const aixos_mq_attr_t *new_attr,
                           aixos_mq_attr_t *old_attr);
int aixos_mq_posix_notify(aixos_mqd_t queue,
                          const aixos_sigevent_t *event);

/* ── POSIX errno access ───────────────────────────────────────────────── */

int *aixos_posix_errno_location(void);

/* ── Host test support ────────────────────────────────────────────────── */

#ifdef AIXOS_HOST_TEST
int aixos_posix_test_complete(aixos_pthread_t thread, void *value);
int aixos_posix_test_run_thread(aixos_pthread_t thread);
#endif

#endif /* AIXOS_POSIX_H */
