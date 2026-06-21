#include "aixos/notify.h"
#include "aixos/task.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"

static int notify_common(aixos_handle_t handle, uint32_t value,
                         aixos_notify_action_t action)
{
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(handle);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    switch (action) {
        case AIXOS_NOTIFY_SET_BITS:
            tcb->notify_value |= value;
            break;
        case AIXOS_NOTIFY_INCREMENT:
            if (tcb->notify_value == UINT32_MAX) {
                aixos_arch_int_restore(flags);
                return AIXOS_ERR_OVERFLOW;
            }
            tcb->notify_value++;
            break;
        case AIXOS_NOTIFY_OVERWRITE:
            tcb->notify_value = value;
            break;
        case AIXOS_NOTIFY_NO_OVERWRITE:
            if (tcb->notify_pending != 0U) {
                aixos_arch_int_restore(flags);
                return AIXOS_ERR_BUSY;
            }
            tcb->notify_value = value;
            break;
        default:
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
    }
    tcb->notify_pending = 1U;
    if (tcb->state == AIXOS_TASK_BLOCKED &&
        tcb->wait_type == AIXOS_OBJ_TASK &&
        tcb->wait_obj == tcb) {
        aixos_task_wake(tcb, AIXOS_OK);
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_task_notify(aixos_handle_t task, uint32_t value,
                      aixos_notify_action_t action)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return notify_common(task, value, action);
}

int aixos_task_notify_from_isr(aixos_handle_t task, uint32_t value,
                               aixos_notify_action_t action)
{
    if (!aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return notify_common(task, value, action);
}

int aixos_task_notify_wait(uint32_t clear_on_entry, uint32_t clear_on_exit,
                           uint32_t *value, uint32_t timeout_ms)
{
    aixos_tcb_t *self = g_cur_task;
    aixos_arch_flags_t flags;
    int result;
    if (aixos_in_isr() || self == NULL) {
        return AIXOS_ERR_CONTEXT;
    }
    if (value == NULL) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    self->notify_value &= ~clear_on_entry;
    if (self->notify_pending == 0U) {
        if (timeout_ms == 0U) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_AGAIN;
        }
        result = aixos_task_block_current(NULL, self, AIXOS_OBJ_TASK,
                                          timeout_ms, flags);
        if (result != AIXOS_OK || self->state == AIXOS_TASK_BLOCKED) {
            return result;
        }
        flags = aixos_arch_int_disable();
    }
    *value = self->notify_value;
    self->notify_value &= ~clear_on_exit;
    self->notify_pending = 0U;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_task_notify_take(int clear_count, uint32_t timeout_ms,
                           uint32_t *value)
{
    int result = aixos_task_notify_wait(0U, 0U, value, timeout_ms);
    aixos_arch_flags_t flags;
    if (result != AIXOS_OK) {
        return result;
    }
    flags = aixos_arch_int_disable();
    if (clear_count != 0) {
        g_cur_task->notify_value = 0U;
    } else if (g_cur_task->notify_value != 0U) {
        g_cur_task->notify_value--;
        if (g_cur_task->notify_value != 0U) {
            g_cur_task->notify_pending = 1U;
        }
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}
