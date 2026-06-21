/* ── POSIX system types ───────────────────────────────────────────────────
 *
 * Standard POSIX type definitions used across multiple headers.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_SYS_TYPES_H
#define AIXOS_POSIX_SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* ── Standard POSIX types ─────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_SSIZE_T
#define AIXOS_POSIX_SSIZE_T
typedef long ssize_t;
#endif

#ifndef AIXOS_POSIX_MODE_T
#define AIXOS_POSIX_MODE_T
typedef unsigned int mode_t;
#endif

#ifndef AIXOS_POSIX_PID_T
#define AIXOS_POSIX_PID_T
typedef int32_t pid_t;
#endif

#ifndef AIXOS_POSIX_PTHREAD_T
#define AIXOS_POSIX_PTHREAD_T
typedef int32_t pthread_t;     /* same as aixos_handle_t */
#endif

#endif /* AIXOS_POSIX_SYS_TYPES_H */
