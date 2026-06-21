/* ── POSIX unistd definitions ─────────────────────────────────────────────
 *
 * Standard POSIX I/O and sleep functions for AIXOS.
 * pipe/read/write/close are declared as proper C functions (not macros)
 * so that the linker resolves them to implementations in posix/src/unistd.c.
 * sleep/usleep use static inline wrappers around aixos_task_sleep().
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_UNISTD_H
#define AIXOS_POSIX_UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include "sys/types.h"
#include "errno.h"
#include "aixos/aixos.h"

/* ── Internal implementation declarations ──────────────────────────────── */
int     aixos_posix_pipe_open(int descriptors[2]);
ssize_t aixos_posix_read(int descriptor, void *buffer, size_t count);
ssize_t aixos_posix_write(int descriptor, const void *buffer, size_t count);
int     aixos_posix_close(int descriptor);

/* ── File descriptor I/O functions ──────────────────────────────────────
 * Direct 1:1 alias to aixos_posix_ implementations.
 * ───────────────────────────────────────────────────────────────────── */
#define pipe   aixos_posix_pipe_open
#define read   aixos_posix_read
#define write  aixos_posix_write
#define close  aixos_posix_close

/* ── Sleep functions ──────────────────────────────────────────────────── */
static inline unsigned int sleep(unsigned int seconds)
{
    uint64_t milliseconds = (uint64_t)seconds * 1000U;
    int result;
    if (milliseconds > UINT32_MAX) {
        errno = EINVAL;
        return seconds;
    }
    result = aixos_task_sleep((uint32_t)milliseconds);
    if (result != AIXOS_OK) {
        errno = EINVAL;
        return seconds;
    }
    return 0U;
}

static inline int usleep(unsigned int microseconds)
{
    uint32_t milliseconds = (microseconds + 999U) / 1000U;
    int result = aixos_task_sleep(milliseconds);
    if (result != AIXOS_OK) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

#endif /* AIXOS_POSIX_UNISTD_H */
