#include "aixos/event.h"
#include "aixos/task.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct {
    uint32_t flags;
    aixos_handle_t handle;
    aixos_list_t wait_list;
} aixos_event_t;

static aixos_event_t event_pool[AIXOS_CFG_MAX_EVENT];

static aixos_event_t *event_from_handle(aixos_handle_t handle)
{
    return (aixos_event_t *)aixos_obj_from_handle(handle, AIXOS_OBJ_EVENT);
}

static uint32_t event_match(const aixos_event_t *event, uint32_t mask,
                            uint8_t mode)
{
    uint32_t match = event->flags & mask;
    if ((mode & AIXOS_EVENT_AND) != 0U) {
        return match == mask ? match : 0U;
    }
    return match;
}

aixos_handle_t aixos_event_create(void)
{
    int i;
    int slot;
    aixos_event_t *event;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_HANDLE_INVALID;
    }
    flags = aixos_arch_int_disable();
    for (i = 0; i < AIXOS_CFG_MAX_EVENT; i++) {
        if (event_pool[i].handle == 0) {
            break;
        }
    }
    if (i == AIXOS_CFG_MAX_EVENT) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    event = &event_pool[i];
    slot = aixos_slot_alloc(AIXOS_POOL_EVENT, event);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    memset(event, 0, sizeof(*event));
    event->handle = aixos_slot_handle(AIXOS_POOL_EVENT, slot);
    aixos_list_init(&event->wait_list);
    aixos_arch_int_restore(flags);
    return event->handle;
}

int aixos_event_wait(aixos_handle_t handle, uint32_t mask, uint8_t mode,
                     uint32_t timeout_ms, uint32_t *matched)
{
    aixos_event_t *event;
    uint32_t match;
    int result = AIXOS_OK;
    aixos_arch_flags_t flags;
    uint8_t match_mode = mode & (AIXOS_EVENT_AND | AIXOS_EVENT_OR);
    uint32_t start_tick = aixos_get_tick();
    uint32_t remaining = timeout_ms;
    
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    if (matched == NULL || mask == 0U ||
        (match_mode != AIXOS_EVENT_AND && match_mode != AIXOS_EVENT_OR) ||
        (mode & ~(AIXOS_EVENT_AND | AIXOS_EVENT_OR | AIXOS_EVENT_CLEAR)) != 0U) {
        return AIXOS_ERR_INVAL;
    }
    
    for (;;) {
        flags = aixos_arch_int_disable();
        event = event_from_handle(handle);
        if (event == NULL) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
        }
        match = event_match(event, mask, mode);
        if (match != 0U) {
            if ((mode & AIXOS_EVENT_CLEAR) != 0U) {
                event->flags &= ~match;
            }
            aixos_arch_int_restore(flags);
            *matched = match;
            return AIXOS_OK;
        }
        if (remaining == 0U) {
            aixos_arch_int_restore(flags);
            return result == AIXOS_ERR_TIMEOUT ? AIXOS_ERR_TIMEOUT : AIXOS_ERR_AGAIN;
        }
        g_cur_task->pend_mask = mask;
        g_cur_task->pend_mode = mode;
        g_cur_task->pend_result = 0U;
        result = aixos_task_block_current(&event->wait_list, event,
                                          AIXOS_OBJ_EVENT, remaining, flags);
        if (result == AIXOS_OK) {
            // 被唤醒后，重确认条件
            remaining = aixos_timeout_remaining_ms(start_tick, timeout_ms);
            continue;
        }
        return result;
    }
}

static int event_set_common(aixos_handle_t handle, uint32_t flags_to_set)
{
    aixos_event_t *event;
    aixos_list_t *position;
    aixos_list_t *next;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    event = event_from_handle(handle);
    if (event == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    event->flags |= flags_to_set;
    AIXOS_LIST_FOR_EACH_SAFE(position, next, &event->wait_list) {
        aixos_tcb_t *tcb =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, wait_node);
        uint32_t match = event_match(event, tcb->pend_mask, tcb->pend_mode);
        if (match != 0U) {
            if ((tcb->pend_mode & AIXOS_EVENT_CLEAR) != 0U) {
                event->flags &= ~match;
            }
            tcb->pend_result = match;
            aixos_task_wake(tcb, AIXOS_OK);
        }
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_event_set(aixos_handle_t handle, uint32_t flags_to_set)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return event_set_common(handle, flags_to_set);
}

int aixos_event_clear(aixos_handle_t handle, uint32_t flags_to_clear)
{
    aixos_event_t *event;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    event = event_from_handle(handle);
    if (event == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    event->flags &= ~flags_to_clear;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_event_delete(aixos_handle_t handle)
{
    aixos_event_t *event;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    event = event_from_handle(handle);
    if (event == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (!aixos_list_is_empty(&event->wait_list)) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_BUSY;
    }
    aixos_slot_free(AIXOS_POOL_EVENT, (int)AIXOS_HANDLE_IDX(handle));
    memset(event, 0, sizeof(*event));
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

#ifdef AIXOS_HOST_TEST
int aixos_test_event_add_waiter(aixos_handle_t handle, aixos_handle_t task,
                                uint32_t mask, uint8_t mode)
{
    aixos_event_t *event;
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    event = event_from_handle(handle);
    tcb = aixos_tcb_from_handle(task);
    if (event == NULL || tcb == NULL || mask == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    aixos_sched_remove_task(tcb);
    tcb->state = AIXOS_TASK_BLOCKED;
    tcb->wait_obj = event;
    tcb->wait_list = &event->wait_list;
    tcb->wait_type = AIXOS_OBJ_EVENT;
    tcb->wait_result = AIXOS_OK;
    tcb->pend_mask = mask;
    tcb->pend_mode = mode;
    tcb->pend_result = 0U;
    aixos_list_add_tail(&tcb->wait_node, &event->wait_list);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}
#endif
