/* ── POSIX file control definitions ───────────────────────────────────────
 *
 * File open flags and mode bits for open() / mq_open() / sem_open().
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_FCNTL_H
#define AIXOS_POSIX_FCNTL_H

/* ── File access mode flags ───────────────────────────────────────────── */
#define O_RDONLY    0x0000      /* Open for reading only */
#define O_WRONLY    0x0001      /* Open for writing only */
#define O_RDWR      0x0002      /* Open for reading and writing */

/* ── File creation / status flags ─────────────────────────────────────── */
#define O_APPEND    0x0008      /* Append on each write */
#define O_NONBLOCK  0x0004      /* Non-blocking mode */
#define O_CREAT     0x0200      /* Create file if it does not exist */
#define O_EXCL      0x0800      /* Exclusive use flag (with O_CREAT) */
#define O_TRUNC     0x0400      /* Truncate to zero length */

#endif /* AIXOS_POSIX_FCNTL_H */
