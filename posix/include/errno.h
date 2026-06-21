/* ── POSIX errno definitions ─────────────────────────────────────────────
 *
 * All standard errno constants with their POSIX numeric values.
 * errno is per-task; the macro dereferences a task-local slot.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_ERRNO_H
#define AIXOS_POSIX_ERRNO_H

/* ── Per-task errno access ────────────────────────────────────────────── */
extern int *aixos_posix_errno_location(void);
#define errno (*aixos_posix_errno_location())

/* ── Standard POSIX errno constants (numeric values per POSIX / glibc) ── */
#define EPERM           1       /* Operation not permitted */
#define ENOENT          2       /* No such file or directory */
#define ESRCH           3       /* No such process */
#define EINTR           4       /* Interrupted system call */
#define EIO             5       /* I/O error */
#define ENXIO           6       /* No such device or address */
#define E2BIG           7       /* Argument list too long */
#define ENOEXEC         8       /* Exec format error */
#define EBADF           9       /* Bad file descriptor */
#define ECHILD          10      /* No child processes */
#define EAGAIN          11      /* Resource temporarily unavailable */
#define ENOMEM          12      /* Cannot allocate memory */
#define EACCES          13      /* Permission denied */
#define EFAULT          14      /* Bad address */
#define EBUSY           16      /* Device or resource busy */
#define EEXIST          17      /* File exists */
#define EXDEV           18      /* Cross-device link */
#define ENODEV          19      /* No such device */
#define ENOTDIR         20      /* Not a directory */
#define EISDIR          21      /* Is a directory */
#define EINVAL          22      /* Invalid argument */
#define ENFILE          23      /* File table overflow */
#define EMFILE          24      /* Too many open files */
#define ENOSPC          28      /* No space left on device */
#define ESPIPE          29      /* Illegal seek */
#define EROFS           30      /* Read-only file system */
#define EDEADLK         35      /* Resource deadlock avoided */
#define ENAMETOOLONG    36      /* File name too long */
#define ENOSYS          38      /* Function not implemented */
#define ENOTEMPTY       39      /* Directory not empty */
#define ELOOP           42      /* Too many symbolic links */
#define ENOTSUP         45      /* Operation not supported */
#define ETIMEDOUT       60      /* Connection timed out */
#define EPROTO          71      /* Protocol error */

#endif /* AIXOS_POSIX_ERRNO_H */
