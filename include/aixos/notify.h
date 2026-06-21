#ifndef AIXOS_NOTIFY_H
#define AIXOS_NOTIFY_H

#include "aixos/types.h"

typedef enum {
    AIXOS_NOTIFY_SET_BITS = 0,
    AIXOS_NOTIFY_INCREMENT = 1,
    AIXOS_NOTIFY_OVERWRITE = 2,
    AIXOS_NOTIFY_NO_OVERWRITE = 3,
} aixos_notify_action_t;

int aixos_task_notify(aixos_handle_t task, uint32_t value,
                      aixos_notify_action_t action);
int aixos_task_notify_from_isr(aixos_handle_t task, uint32_t value,
                               aixos_notify_action_t action);
int aixos_task_notify_wait(uint32_t clear_on_entry, uint32_t clear_on_exit,
                           uint32_t *value, uint32_t timeout_ms);
int aixos_task_notify_take(int clear_count, uint32_t timeout_ms,
                           uint32_t *value);

#endif
