/* ── POSIX time definitions ───────────────────────────────────────────────
 *
 * Clock, timer, and sleep function declarations with standard POSIX types.
 * Functions are declared (not defined inline); implementations live in
 * compat/posix/posix.c where the mapping to internal aixos_ types occurs.
 * Since aixos_timespec_t { int64_t tv_sec; long tv_nsec; } now matches
 * struct timespec layout exactly, the implementations can simply cast
 * pointers between the two types with zero conversion overhead.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_TIME_H
#define AIXOS_POSIX_TIME_H

#include <stdint.h>
#include "aixos/posix.h"
#include "errno.h"

/* ── Type definitions ─────────────────────────────────────────────────── */
typedef int     clockid_t;
typedef int64_t time_t;
typedef int32_t timer_t;

/* ── struct timespec ────────────────────────────────────────────────────
 * Layout matches aixos_timespec_t exactly: { int64_t tv_sec; long tv_nsec; }
 * so pointers can be freely cast between the two types.
 * ───────────────────────────────────────────────────────────────────── */
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

/* ── union sigval ───────────────────────────────────────────────────────
 * Layout matches aixos_sigval_t exactly: { int sival_int; void *sival_ptr; }
 * ───────────────────────────────────────────────────────────────────── */
union sigval {
    int   sival_int;
    void *sival_ptr;
};

/* ── struct sigevent ────────────────────────────────────────────────────
 * NOTE: Field names differ from internal aixos_sigevent_t which uses
 * notify / value / function.  The posix.c implementation maps between them.
 * ───────────────────────────────────────────────────────────────────── */
struct sigevent {
    int             sigev_notify;               /* Notification type   */
    int             sigev_signo;                /* Signal number       */
    union sigval    sigev_value;                /* Signal value        */
    void          (*sigev_notify_function)(union sigval);  /* Notify func */
    void           *sigev_notify_attributes;    /* Notify attributes   */
};

/* ── struct itimerspec ──────────────────────────────────────────────────
 * NOTE: Field names differ from internal aixos_itimerspec_t which uses
 * interval / value.  The posix.c implementation maps between them.
 * ───────────────────────────────────────────────────────────────────── */
struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

/* ── Clock and timer constants ────────────────────────────────────────── */
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1
#define TIMER_ABSTIME   1

/* ── Signal event notification methods ────────────────────────────────── */
#define SIGEV_NONE      0
#define SIGEV_SIGNAL    1
#define SIGEV_THREAD    2

/* ── Clock and sleep functions ────────────────────────────────────────── */

/* Inline wrappers: aixos_timespec_t and struct timespec are different
 * C types (identical layout), so pointers need explicit casts.
 */
static inline int clock_gettime(clockid_t clock_id, struct timespec *value)
{
    return aixos_clock_gettime(clock_id, (aixos_timespec_t *)value);
}
static inline int clock_getres(clockid_t clock_id, struct timespec *resolution)
{
    return aixos_clock_getres(clock_id, (aixos_timespec_t *)resolution);
}
static inline int nanosleep(const struct timespec *request,
                            struct timespec *remaining)
{
    return aixos_nanosleep((const aixos_timespec_t *)request,
                           (aixos_timespec_t *)remaining);
}
static inline int clock_nanosleep(clockid_t clock_id, int flags,
                                  const struct timespec *request,
                                  struct timespec *remaining)
{
    (void)remaining;
    return aixos_clock_nanosleep(clock_id, flags,
                                 (const aixos_timespec_t *)request);
}

/* ── POSIX timer functions ────────────────────────────────────────────── */

/* timer_create: aixos_sigevent_t has 3 fields while struct sigevent has 4
 * (+ sigev_notify_attributes), so field-by-field copy is required.
 */
static inline int timer_create(clockid_t clock_id, struct sigevent *event,
                               timer_t *timer)
{
    if (event == NULL) {
        return aixos_timer_posix_create(clock_id, NULL, timer);
    }
    if (event->sigev_notify_attributes != NULL) {
        errno = ENOTSUP;
        return -1;
    }
    {
        aixos_sigevent_t native;
        native.notify = event->sigev_notify;
        native.value.sival_int = event->sigev_value.sival_int;
        native.value.sival_ptr = event->sigev_value.sival_ptr;
        native.function = (void (*)(union aixos_sigval))(void *)
            event->sigev_notify_function;
        return aixos_timer_posix_create(clock_id, &native, timer);
    }
}

/* Direct alias: timer_t / aixos_timer_posix_t are both int32_t;
 * aixos_itimerspec_t and struct itimerspec have identical layout,
 * and taking const/pointer to either is ABI-compatible. */
#define timer_delete         aixos_timer_posix_delete
#define timer_getoverrun     aixos_timer_posix_getoverrun

/* timer_settime/timer_gettime use different struct names (same layout),
 * so use inline wrappers for correct casts. */

static inline int timer_settime(timer_t timer, int flags,
                  const struct itimerspec *value,
                  struct itimerspec *old_value)
{
    return aixos_timer_posix_settime(timer, flags,
                   (const aixos_itimerspec_t *)value,
                   (aixos_itimerspec_t *)old_value);
}

static inline int timer_gettime(timer_t timer, struct itimerspec *value)
{
    return aixos_timer_posix_gettime(timer,
                   (aixos_itimerspec_t *)value);
}

#endif /* AIXOS_POSIX_TIME_H */
