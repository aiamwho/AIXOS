#ifndef AIXOS_SEM_H
#define AIXOS_SEM_H
#include "aixos/types.h"
aixos_handle_t aixos_sem_create(int initial_count);
int aixos_sem_wait(aixos_handle_t sem, uint32_t timeout_ms);
int aixos_sem_post(aixos_handle_t sem);
int aixos_sem_post_from_isr(aixos_handle_t sem);
int aixos_sem_delete(aixos_handle_t sem);
int aixos_sem_get_count(aixos_handle_t sem);
#endif
