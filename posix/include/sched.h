/* ── POSIX scheduling definitions ─────────────────────────────────────────
 *
 * Scheduling policies, parameters, and function declarations.
 * Functions are declared (not inline); implementations in posix.c.
 * ─────────────────────────────────────────────────────────────────────── */
#ifndef AIXOS_POSIX_SCHED_H
#define AIXOS_POSIX_SCHED_H

#include <stddef.h>

/* ── Scheduling parameters ────────────────────────────────────────────── */
struct sched_param {
    int sched_priority;
};

/* ── Scheduling policies ──────────────────────────────────────────────── */
#define SCHED_OTHER 0
#define SCHED_FIFO  1
#define SCHED_RR    2

/* ── Scheduling functions ─────────────────────────────────────────────── */

/* 1:1 mappings – same signature, direct alias */
#define sched_yield             aixos_sched_yield
#define sched_get_priority_min  aixos_sched_get_priority_min
#define sched_get_priority_max  aixos_sched_get_priority_max

#endif /* AIXOS_POSIX_SCHED_H */
