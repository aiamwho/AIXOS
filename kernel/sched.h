#ifndef AIXOS_SCHED_H
#define AIXOS_SCHED_H

#include "aixos/task.h"

extern aixos_tcb_t *g_cur_task;

void aixos_sched_init(void);
void aixos_sched_add_task(aixos_tcb_t *tcb);
void aixos_sched_remove_task(aixos_tcb_t *tcb);
void aixos_sched_requeue_task(aixos_tcb_t *tcb, int old_priority);
void aixos_sched_rotate_current(void);
void aixos_sched_request_preemption(void);
void aixos_schedule(void);
void aixos_tick_handler(void);
uint32_t aixos_get_tick(void);
void aixos_sched_stats_snapshot(aixos_sched_stats_t *stats);

int  aixos_sched_set_policy(aixos_handle_t task, int policy);
int  aixos_sched_get_policy(aixos_handle_t task);
void aixos_sched_boost_priority(aixos_tcb_t *tcb, int boost);
void aixos_sched_unboost_priority(aixos_tcb_t *tcb);

#ifdef AIXOS_HOST_TEST
void aixos_test_set_current(aixos_tcb_t *tcb);
void aixos_test_set_sched_stats(uint64_t total, uint64_t idle,
                                uint64_t switches);
#endif

#endif
