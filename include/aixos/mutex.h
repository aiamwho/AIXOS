#ifndef AIXOS_MUTEX_H
#define AIXOS_MUTEX_H
#include "aixos/types.h"
aixos_handle_t aixos_mutex_create(void);
int aixos_mutex_lock(aixos_handle_t mutex, uint32_t timeout_ms);
int aixos_mutex_unlock(aixos_handle_t mutex);
int aixos_mutex_delete(aixos_handle_t mutex);
struct aixos_tcb;
int aixos_mutex_task_can_delete(struct aixos_tcb *tcb);
void aixos_mutex_waiter_added(struct aixos_tcb *tcb);
void aixos_mutex_waiter_removed(struct aixos_tcb *tcb);
void aixos_mutex_task_priority_changed(struct aixos_tcb *tcb);
#endif
