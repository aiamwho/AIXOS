/* ── POSIX message queue definitions ──────────────────────────────────────
 *
 * Message queue API for AIXOS.
 * mqd_t maps to aixos_mqd_t (handle-based).
 * struct mq_attr layout matches aixos_mq_attr_t exactly, allowing
 * direct pointer casts for getattr/setattr.
 * aixos_timespec_t matches struct timespec, allowing direct casts
 * for timedsend/timedreceive.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_MQUEUE_H
#define AIXOS_POSIX_MQUEUE_H

#include <stdarg.h>
#include <stddef.h>
#include "sys/types.h"
#include "aixos/posix.h"
#include "errno.h"
#include "fcntl.h"
#include "time.h"

/* ── Message queue descriptor ─────────────────────────────────────────── */
typedef aixos_mqd_t mqd_t;

/* ── Message queue attributes ───────────────────────────────────────────
 * Layout matches aixos_mq_attr_t exactly: { long mq_flags; long mq_maxmsg;
 * long mq_msgsize; long mq_curmsgs; } – pointers can be freely cast.
 * ───────────────────────────────────────────────────────────────────── */
struct mq_attr {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
};

/* ── Named message queue helpers (implemented in posix/src/mqueue.c) ─── */
extern mqd_t aixos_posix_mq_open_named(const char *name, int flags,
                                        const struct mq_attr *attr);
extern int  aixos_posix_mq_close_named(mqd_t descriptor);
extern int  aixos_posix_mq_unlink_named(const char *name);

/* ── Named message queue API ──────────────────────────────────────────── */
static inline mqd_t mq_open(const char *name, int flags, ...)
{
    const struct mq_attr *attr = NULL;
    if ((flags & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        (void)va_arg(arguments, mode_t);   /* mode – ignored on RTOS */
        attr = va_arg(arguments, const struct mq_attr *);
        va_end(arguments);
    }
    return aixos_posix_mq_open_named(name, flags, attr);
}

static inline int mq_close(mqd_t queue)
{
    int result = aixos_posix_mq_close_named(queue);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int mq_unlink(const char *name)
{
    int result = aixos_posix_mq_unlink_named(name);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

/* ── Message queue operations ─────────────────────────────────────────── */
static inline int mq_send(mqd_t queue, const char *message,
                           size_t length, unsigned int priority)
{
    int result = aixos_mq_posix_send(queue, message, length, priority);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline ssize_t mq_receive(mqd_t queue, char *message,
                                  size_t capacity, unsigned int *priority)
{
    int result = aixos_mq_posix_receive(queue, message, capacity, priority);
    if (result < 0) { errno = -result; return -1; }
    return (ssize_t)result;
}

/* Timed – aixos_timespec_t matches struct timespec, direct pointer cast */
static inline int mq_timedsend(mqd_t queue, const char *message,
                                size_t length, unsigned int priority,
                                const struct timespec *abstime)
{
    int result;
    if (abstime == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = aixos_mq_posix_timedsend(queue, message, length, priority,
                                       (const aixos_timespec_t *)abstime);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline ssize_t mq_timedreceive(
    mqd_t queue, char *message, size_t capacity, unsigned int *priority,
    const struct timespec *abstime)
{
    int result;
    if (abstime == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = aixos_mq_posix_timedreceive(queue, message, capacity, priority,
                                          (const aixos_timespec_t *)abstime);
    if (result < 0) { errno = -result; return -1; }
    return (ssize_t)result;
}

/* ── Attribute access ───────────────────────────────────────────────────
 * struct mq_attr layout matches aixos_mq_attr_t, so we can cast pointers.
 * ───────────────────────────────────────────────────────────────────── */
static inline int mq_getattr(mqd_t queue, struct mq_attr *attr)
{
    int result;
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = aixos_mq_posix_getattr(queue, (aixos_mq_attr_t *)attr);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int mq_setattr(mqd_t queue, const struct mq_attr *new_attr,
                              struct mq_attr *old_attr)
{
    int result = aixos_mq_posix_setattr(
        queue,
        (const aixos_mq_attr_t *)new_attr,
        (aixos_mq_attr_t *)old_attr);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

/* ── Notification ───────────────────────────────────────────────────────
 * struct sigevent fields differ from aixos_sigevent_t, so we map them.
 * ───────────────────────────────────────────────────────────────────── */
static inline int mq_notify(mqd_t queue, const struct sigevent *event)
{
    aixos_sigevent_t native;
    const aixos_sigevent_t *native_ptr = NULL;
    int result;
    if (event != NULL) {
        native.notify  = event->sigev_notify;
        native.value.sival_int = event->sigev_value.sival_int;
        native.value.sival_ptr = event->sigev_value.sival_ptr;
        void (*fn)(union aixos_sigval);
        fn = (void (*)(union aixos_sigval))(void *)event->sigev_notify_function;
        native.function = fn;
        native_ptr = &native;
    }
    result = aixos_mq_posix_notify(queue, native_ptr);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

#endif /* AIXOS_POSIX_MQUEUE_H */
