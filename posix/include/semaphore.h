/* ── POSIX semaphore definitions ──────────────────────────────────────────
 *
 * Named and unnamed semaphores for AIXOS.
 * sem_t maps directly to aixos_sem_posix_t (both are handle-based).
 * Since aixos_timespec_t matches struct timespec layout, sem_timedwait
 * uses a direct pointer cast instead of field-by-field conversion.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_SEMAPHORE_H
#define AIXOS_POSIX_SEMAPHORE_H

#include "aixos/posix.h"
#include "errno.h"
#include "fcntl.h"
#include "time.h"
#include <stdarg.h>
#include "sys/types.h"

/* ── Semaphore type ───────────────────────────────────────────────────── */
typedef aixos_sem_posix_t sem_t;

#define SEM_FAILED ((sem_t *)(intptr_t)-1)

/* ── Named semaphore helpers (implemented in posix/src/semaphore.c) ───── */
extern sem_t *aixos_posix_sem_open_named(const char *name, int flags,
                                         unsigned int value);
extern int    aixos_posix_sem_close_named(sem_t *sem);
extern int    aixos_posix_sem_unlink_named(const char *name);

/* ── Named semaphore API ──────────────────────────────────────────────── */
static inline sem_t *sem_open(const char *name, int flags, ...)
{
    unsigned int value = 0U;
    if ((flags & O_CREAT) != 0) {
        va_list arguments;
        va_start(arguments, flags);
        (void)va_arg(arguments, mode_t);   /* mode – ignored on RTOS */
        value = va_arg(arguments, unsigned int);
        va_end(arguments);
    }
    return aixos_posix_sem_open_named(name, flags, value);
}

static inline int sem_close(sem_t *sem)
{
    int result = aixos_posix_sem_close_named(sem);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int sem_unlink(const char *name)
{
    int result = aixos_posix_sem_unlink_named(name);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

/* ── Unnamed semaphore API ────────────────────────────────────────────── */
static inline int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    int result;
    if (pshared != 0) {
        errno = ENOTSUP;
        return -1;
    }
    result = aixos_sem_posix_init(sem, value);
    if (result != 0) {
        errno = result;
        return -1;
    }
    return 0;
}

/* ── Semaphore operations ─────────────────────────────────────────────── */
static inline int sem_destroy(sem_t *sem)
{
    int result = aixos_sem_posix_destroy(sem);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int sem_wait(sem_t *sem)
{
    int result = aixos_sem_posix_wait(sem);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int sem_trywait(sem_t *sem)
{
    int result = aixos_sem_posix_trywait(sem);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int sem_post(sem_t *sem)
{
    int result = aixos_sem_posix_post(sem);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

static inline int sem_getvalue(sem_t *sem, int *value)
{
    int result = aixos_sem_posix_getvalue(sem, value);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

/* Timed – aixos_timespec_t matches struct timespec, direct pointer cast */
static inline int sem_timedwait(sem_t *sem, const struct timespec *abstime)
{
    int result;
    if (abstime == NULL) {
        errno = EINVAL;
        return -1;
    }
    result = aixos_sem_posix_timedwait(sem, (const aixos_timespec_t *)abstime);
    if (result != 0) { errno = result; return -1; }
    return 0;
}

#endif /* AIXOS_POSIX_SEMAPHORE_H */
