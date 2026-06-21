/* ── POSIX threads (pthread) definitions ──────────────────────────────────
 *
 * Public POSIX thread API for AIXOS.  Type definitions map to internal
 * aixos_ types.  Functions that are 1:1 (same signature) use #define
 * aliases.  Functions that need sched_param mapping or timespec pointer
 * casts use static inline wrappers.
 *
 * Since aixos_timespec_t { int64_t tv_sec; long tv_nsec; } now matches
 * struct timespec layout exactly, timed functions can use direct casts
 * instead of field-by-field conversion.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_PTHREAD_H
#define AIXOS_POSIX_PTHREAD_H

#include "aixos/posix.h"
#include "sched.h"
#include "time.h"
#include "errno.h"

/* ── Type definitions ─────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_PTHREAD_T
#define AIXOS_POSIX_PTHREAD_T
typedef aixos_pthread_t pthread_t;
#endif

typedef aixos_pthread_attr_t       pthread_attr_t;
typedef aixos_pthread_key_t        pthread_key_t;
typedef aixos_pthread_once_t       pthread_once_t;
typedef aixos_pthread_mutex_t      pthread_mutex_t;
typedef aixos_pthread_mutexattr_t  pthread_mutexattr_t;
typedef aixos_pthread_cond_t       pthread_cond_t;
typedef aixos_pthread_condattr_t   pthread_condattr_t;
typedef aixos_pthread_rwlock_t     pthread_rwlock_t;
typedef aixos_pthread_rwlockattr_t pthread_rwlockattr_t;
typedef aixos_pthread_barrier_t    pthread_barrier_t;
typedef aixos_pthread_barrierattr_t pthread_barrierattr_t;

/* ── Thread create / detach states ────────────────────────────────────── */
#define PTHREAD_CREATE_JOINABLE  AIXOS_PTHREAD_CREATE_JOINABLE
#define PTHREAD_CREATE_DETACHED  AIXOS_PTHREAD_CREATE_DETACHED

/* ── Mutex types ──────────────────────────────────────────────────────── */
#define PTHREAD_MUTEX_NORMAL     AIXOS_PTHREAD_MUTEX_NORMAL
#define PTHREAD_MUTEX_RECURSIVE  AIXOS_PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_ERRORCHECK AIXOS_PTHREAD_MUTEX_ERRORCHECK

/* ── Mutex protocols ──────────────────────────────────────────────────── */
#define PTHREAD_PRIO_NONE        AIXOS_PTHREAD_PRIO_NONE
#define PTHREAD_PRIO_INHERIT     AIXOS_PTHREAD_PRIO_INHERIT
#define PTHREAD_PRIO_PROTECT     AIXOS_PTHREAD_PRIO_PROTECT

/* ── Scheduling inheritance ───────────────────────────────────────────── */
#define PTHREAD_INHERIT_SCHED    AIXOS_PTHREAD_INHERIT_SCHED
#define PTHREAD_EXPLICIT_SCHED   AIXOS_PTHREAD_EXPLICIT_SCHED

/* ── Process sharing ──────────────────────────────────────────────────── */
#define PTHREAD_PROCESS_PRIVATE  AIXOS_PTHREAD_PROCESS_PRIVATE
#define PTHREAD_PROCESS_SHARED   AIXOS_PTHREAD_PROCESS_SHARED

/* ── Once and barrier ─────────────────────────────────────────────────── */
#define PTHREAD_ONCE_INIT                 AIXOS_PTHREAD_ONCE_INIT
#define PTHREAD_BARRIER_SERIAL_THREAD     AIXOS_PTHREAD_BARRIER_SERIAL_THREAD

/* ══════════════════════════════════════════════════════════════════════════
 *  Thread attribute functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings – same signature, direct alias */
#define pthread_attr_init           aixos_pthread_attr_init
#define pthread_attr_destroy        aixos_pthread_attr_destroy
#define pthread_attr_setstacksize   aixos_pthread_attr_setstacksize
#define pthread_attr_getstacksize   aixos_pthread_attr_getstacksize
#define pthread_attr_setdetachstate aixos_pthread_attr_setdetachstate
#define pthread_attr_getdetachstate aixos_pthread_attr_getdetachstate

/* sched_policy – direct 1:1 since internal type matches int */
#define pthread_attr_setschedpolicy aixos_pthread_attr_setschedpolicy
#define pthread_attr_getschedpolicy aixos_pthread_attr_getschedpolicy

/* inheritsched – direct 1:1 since internal type matches int */
#define pthread_attr_setinheritsched aixos_pthread_attr_setinheritsched
#define pthread_attr_getinheritsched aixos_pthread_attr_getinheritsched

/* sched_param mapping – extract/insert sched_priority from/to struct */
static inline int pthread_attr_setschedparam(
    pthread_attr_t *attr, const struct sched_param *param)
{
    if (param == NULL) return EINVAL;
    return aixos_pthread_attr_setschedprio(attr, param->sched_priority);
}

static inline int pthread_attr_getschedparam(
    const pthread_attr_t *attr, struct sched_param *param)
{
    if (param == NULL) return EINVAL;
    return aixos_pthread_attr_getschedprio(attr, &param->sched_priority);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Thread functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_create  aixos_pthread_create
#define pthread_join    aixos_pthread_join
#define pthread_detach  aixos_pthread_detach
#define pthread_self    aixos_pthread_self
#define pthread_equal   aixos_pthread_equal
#define pthread_exit    aixos_pthread_exit

/* sched_param mapping for pthread_setschedparam / pthread_getschedparam */
static inline int pthread_setschedparam(
    pthread_t thread, int policy, const struct sched_param *param)
{
    if ((policy != SCHED_FIFO && policy != SCHED_RR &&
         policy != SCHED_OTHER) || param == NULL) {
        return EINVAL;
    }
    return aixos_pthread_setschedprio(thread, param->sched_priority);
}

static inline int pthread_getschedparam(
    pthread_t thread, int *policy, struct sched_param *param)
{
    int result;
    if (policy == NULL || param == NULL) return EINVAL;
    result = aixos_pthread_getschedprio(thread, &param->sched_priority);
    if (result == 0) *policy = SCHED_RR;
    return result;
}

/* pthread_setschedprio – direct 1:1 */
static inline int pthread_setschedprio(pthread_t thread, int priority)
{
    return aixos_pthread_setschedprio(thread, priority);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Thread-specific data
 * ══════════════════════════════════════════════════════════════════════════ */

#define pthread_once        aixos_pthread_once
#define pthread_key_create  aixos_pthread_key_create
#define pthread_key_delete  aixos_pthread_key_delete
#define pthread_getspecific aixos_pthread_getspecific
#define pthread_setspecific aixos_pthread_setspecific

/* ══════════════════════════════════════════════════════════════════════════
 *  Mutex attribute functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_mutexattr_init         aixos_pthread_mutexattr_init
#define pthread_mutexattr_destroy      aixos_pthread_mutexattr_destroy
#define pthread_mutexattr_settype      aixos_pthread_mutexattr_settype
#define pthread_mutexattr_gettype      aixos_pthread_mutexattr_gettype
#define pthread_mutexattr_setprotocol  aixos_pthread_mutexattr_setprotocol
#define pthread_mutexattr_getprotocol  aixos_pthread_mutexattr_getprotocol

/* prioceiling – direct 1:1 */
#define pthread_mutexattr_setprioceiling aixos_pthread_mutexattr_setprioceiling
#define pthread_mutexattr_getprioceiling aixos_pthread_mutexattr_getprioceiling

/* pshared – only PRIVATE supported on bare-metal RTOS */
static inline int pthread_mutexattr_setpshared(
    pthread_mutexattr_t *attr, int pshared)
{
    if (attr == NULL) return EINVAL;
    return pshared == PTHREAD_PROCESS_PRIVATE ? 0 : ENOTSUP;
}

static inline int pthread_mutexattr_getpshared(
    const pthread_mutexattr_t *attr, int *pshared)
{
    if (attr == NULL || pshared == NULL) return EINVAL;
    *pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Mutex functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_mutex_init      aixos_pthread_mutex_init
#define pthread_mutex_destroy   aixos_pthread_mutex_destroy
#define pthread_mutex_lock      aixos_pthread_mutex_lock
#define pthread_mutex_trylock   aixos_pthread_mutex_trylock
#define pthread_mutex_unlock    aixos_pthread_mutex_unlock

/* Timed – aixos_timespec_t matches struct timespec, direct pointer cast */
static inline int pthread_mutex_timedlock(
    pthread_mutex_t *mutex, const struct timespec *abstime)
{
    if (abstime == NULL) return EINVAL;
    return aixos_pthread_mutex_timedlock(
        mutex, (const aixos_timespec_t *)abstime);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Condition variable attribute functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_condattr_init      aixos_pthread_condattr_init
#define pthread_condattr_destroy   aixos_pthread_condattr_destroy
#define pthread_condattr_setclock  aixos_pthread_condattr_setclock
#define pthread_condattr_getclock  aixos_pthread_condattr_getclock

/* pshared – only PRIVATE supported on bare-metal RTOS */
static inline int pthread_condattr_setpshared(
    pthread_condattr_t *attr, int pshared)
{
    if (attr == NULL) return EINVAL;
    return pshared == PTHREAD_PROCESS_PRIVATE ? 0 : ENOTSUP;
}

static inline int pthread_condattr_getpshared(
    const pthread_condattr_t *attr, int *pshared)
{
    if (attr == NULL || pshared == NULL) return EINVAL;
    *pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Condition variable functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_cond_init       aixos_pthread_cond_init
#define pthread_cond_destroy    aixos_pthread_cond_destroy
#define pthread_cond_wait       aixos_pthread_cond_wait
#define pthread_cond_signal     aixos_pthread_cond_signal
#define pthread_cond_broadcast  aixos_pthread_cond_broadcast

/* Timed – direct pointer cast */
static inline int pthread_cond_timedwait(
    pthread_cond_t *cond, pthread_mutex_t *mutex,
    const struct timespec *abstime)
{
    if (abstime == NULL) return EINVAL;
    return aixos_pthread_cond_timedwait(
        cond, mutex, (const aixos_timespec_t *)abstime);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Read-write lock attribute functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_rwlockattr_init    aixos_pthread_rwlockattr_init
#define pthread_rwlockattr_destroy aixos_pthread_rwlockattr_destroy

/* pshared */
static inline int pthread_rwlockattr_setpshared(
    pthread_rwlockattr_t *attr, int pshared)
{
    if (attr == NULL) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE) return ENOTSUP;
    attr->process_shared = pshared;
    return 0;
}

static inline int pthread_rwlockattr_getpshared(
    const pthread_rwlockattr_t *attr, int *pshared)
{
    if (attr == NULL || pshared == NULL) return EINVAL;
    *pshared = attr->process_shared;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Read-write lock functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_rwlock_init       aixos_pthread_rwlock_init
#define pthread_rwlock_destroy    aixos_pthread_rwlock_destroy
#define pthread_rwlock_rdlock     aixos_pthread_rwlock_rdlock
#define pthread_rwlock_tryrdlock  aixos_pthread_rwlock_tryrdlock
#define pthread_rwlock_wrlock     aixos_pthread_rwlock_wrlock
#define pthread_rwlock_trywrlock  aixos_pthread_rwlock_trywrlock
#define pthread_rwlock_unlock     aixos_pthread_rwlock_unlock

/* Timed – direct pointer cast */
static inline int pthread_rwlock_timedrdlock(
    pthread_rwlock_t *lock, const struct timespec *abstime)
{
    if (abstime == NULL) return EINVAL;
    return aixos_pthread_rwlock_timedrdlock(
        lock, (const aixos_timespec_t *)abstime);
}

static inline int pthread_rwlock_timedwrlock(
    pthread_rwlock_t *lock, const struct timespec *abstime)
{
    if (abstime == NULL) return EINVAL;
    return aixos_pthread_rwlock_timedwrlock(
        lock, (const aixos_timespec_t *)abstime);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Barrier attribute functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_barrierattr_init    aixos_pthread_barrierattr_init
#define pthread_barrierattr_destroy aixos_pthread_barrierattr_destroy

/* pshared */
static inline int pthread_barrierattr_setpshared(
    pthread_barrierattr_t *attr, int pshared)
{
    if (attr == NULL) return EINVAL;
    if (pshared != PTHREAD_PROCESS_PRIVATE) return ENOTSUP;
    attr->process_shared = pshared;
    return 0;
}

static inline int pthread_barrierattr_getpshared(
    const pthread_barrierattr_t *attr, int *pshared)
{
    if (attr == NULL || pshared == NULL) return EINVAL;
    *pshared = attr->process_shared;
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Barrier functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1:1 mappings */
#define pthread_barrier_init    aixos_pthread_barrier_init
#define pthread_barrier_destroy aixos_pthread_barrier_destroy
#define pthread_barrier_wait    aixos_pthread_barrier_wait

#endif /* AIXOS_POSIX_PTHREAD_H */
