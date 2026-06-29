#include "aixos/timer.h"
#include "aixos/task.h"
#include "aixos/trace.h"
#include "kernel/list.h"
#include "kernel/object.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct {
    char name[AIXOS_CFG_TIMER_NAME_MAX];
    aixos_timer_type_t type;
    aixos_timer_callback_t callback;
    void *arg;
    uint32_t interval;
    uint32_t deadline;
    uint8_t active;
    uint8_t pending;
    aixos_list_t active_node;
    aixos_list_t pending_node;
    aixos_handle_t handle;
} aixos_timer_inner_t;

static aixos_timer_inner_t timer_pool[AIXOS_CFG_MAX_TIMER];
static aixos_list_t active_timers;
static aixos_list_t pending_timers;
static aixos_list_t service_waiters;
/* Timer service uses dynamically allocated stack + TCB */

static int tick_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static void active_insert(aixos_timer_inner_t *timer)
{
    aixos_list_t *position;
    AIXOS_LIST_FOR_EACH(position, &active_timers) {
        aixos_timer_inner_t *queued =
            AIXOS_CONTAINER_OF(position, aixos_timer_inner_t, active_node);
        if ((int32_t)(timer->deadline - queued->deadline) < 0) {
            __aixos_list_add(&timer->active_node, position->prev, position);
            return;
        }
    }
    aixos_list_add_tail(&timer->active_node, &active_timers);
}

void aixos_timer_init(void)
{
    int i;
    aixos_list_init(&active_timers);
    aixos_list_init(&pending_timers);
    aixos_list_init(&service_waiters);
    for (i = 0; i < AIXOS_CFG_MAX_TIMER; i++) {
        memset(&timer_pool[i], 0, sizeof(timer_pool[i]));
        aixos_list_init(&timer_pool[i].active_node);
        aixos_list_init(&timer_pool[i].pending_node);
    }
}

aixos_handle_t aixos_timer_create(const char *name, aixos_timer_type_t type,
                                  aixos_timer_callback_t callback, void *arg)
{
    int slot;
    aixos_handle_t handle;
    aixos_timer_inner_t *timer;
    aixos_arch_flags_t flags;
    int i;

    if (aixos_in_isr() || callback == NULL ||
        (type != AIXOS_TIMER_ONESHOT && type != AIXOS_TIMER_PERIODIC)) {
        return AIXOS_HANDLE_INVALID;
    }
    flags = aixos_arch_int_disable();
    for (i = 0; i < AIXOS_CFG_MAX_TIMER; i++) {
        if (timer_pool[i].handle == 0) {
            break;
        }
    }
    if (i == AIXOS_CFG_MAX_TIMER) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    timer = &timer_pool[i];
    slot = aixos_slot_alloc(AIXOS_POOL_TIMER, timer);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    handle = aixos_slot_handle(AIXOS_POOL_TIMER, slot);
    memset(timer, 0, sizeof(*timer));
    aixos_list_init(&timer->active_node);
    aixos_list_init(&timer->pending_node);
    if (name != NULL) {
        strncpy(timer->name, name, sizeof(timer->name) - 1U);
    }
    timer->type = type;
    timer->callback = callback;
    timer->arg = arg;
    timer->handle = handle;
    aixos_arch_int_restore(flags);
    return handle;
}

int aixos_timer_start(aixos_handle_t handle, uint32_t interval_ms)
{
    aixos_timer_inner_t *timer;
    aixos_arch_flags_t flags;
    uint32_t interval;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    interval = aixos_ms_to_ticks(interval_ms);
    if (interval_ms == 0U || interval == UINT32_MAX) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    timer = (aixos_timer_inner_t *)aixos_obj_from_handle(handle,
                                                         AIXOS_OBJ_TIMER);
    if (timer == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (timer->active != 0U) {
        aixos_list_del(&timer->active_node);
        aixos_list_init(&timer->active_node);
    }
    timer->interval = interval;
    timer->deadline = aixos_get_tick() + interval;
    timer->active = 1U;
    active_insert(timer);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_timer_stop(aixos_handle_t handle)
{
    aixos_timer_inner_t *timer;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    timer = (aixos_timer_inner_t *)aixos_obj_from_handle(handle,
                                                         AIXOS_OBJ_TIMER);
    if (timer == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (timer->active != 0U) {
        aixos_list_del(&timer->active_node);
        aixos_list_init(&timer->active_node);
        timer->active = 0U;
    }
    if (timer->pending != 0U) {
        aixos_list_del(&timer->pending_node);
        aixos_list_init(&timer->pending_node);
        timer->pending = 0U;
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_timer_delete(aixos_handle_t handle)
{
    aixos_timer_inner_t *timer;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    timer = (aixos_timer_inner_t *)aixos_obj_from_handle(handle,
                                                         AIXOS_OBJ_TIMER);
    if (timer == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (timer->active != 0U) {
        aixos_list_del(&timer->active_node);
    }
    if (timer->pending != 0U) {
        aixos_list_del(&timer->pending_node);
    }
    aixos_slot_free(AIXOS_POOL_TIMER, (int)AIXOS_HANDLE_IDX(handle));
    memset(timer, 0, sizeof(*timer));
    aixos_list_init(&timer->active_node);
    aixos_list_init(&timer->pending_node);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

void aixos_timer_tick(uint32_t now)
{
    /* Do not call callbacks from ISR context; move expired timers to the pending list
     * aixos_timer_dispatch() 在 timer_service_entry() 任务上下文中调用回调 */
    while (!aixos_list_is_empty(&active_timers)) {
        aixos_timer_inner_t *timer = AIXOS_CONTAINER_OF(
            aixos_list_first(&active_timers), aixos_timer_inner_t,
            active_node);
        if (!tick_reached(now, timer->deadline)) {
            break;
        }
        aixos_list_del(&timer->active_node);
        aixos_list_init(&timer->active_node);
        if (timer->type == AIXOS_TIMER_PERIODIC) {
            uint32_t late = now - timer->deadline;
            uint32_t periods = late / timer->interval + 1U;
            timer->deadline += periods * timer->interval;
            active_insert(timer);
        } else {
            timer->active = 0U;
        }
        if (timer->pending == 0U) {
            timer->pending = 1U;
            aixos_list_add_tail(&timer->pending_node, &pending_timers);
        }
    }
    if (!aixos_list_is_empty(&pending_timers) &&
        !aixos_list_is_empty(&service_waiters)) {
        aixos_tcb_t *service = AIXOS_CONTAINER_OF(
            aixos_list_first(&service_waiters), aixos_tcb_t, wait_node);
        aixos_task_wake(service, AIXOS_OK);
    }
}

unsigned int aixos_timer_dispatch(void)
{
    /* Dispatch callbacks outside ISR context. */
    if (aixos_in_isr()) {
        return 0U;
    }

    unsigned int dispatched = 0U;
    for (;;) {
        aixos_timer_inner_t *timer;
        aixos_timer_callback_t callback;
        void *arg;
        aixos_arch_flags_t flags = aixos_arch_int_disable();
        if (aixos_list_is_empty(&pending_timers)) {
            aixos_arch_int_restore(flags);
            break;
        }
        timer = AIXOS_CONTAINER_OF(aixos_list_first(&pending_timers),
                                   aixos_timer_inner_t, pending_node);
        aixos_list_del(&timer->pending_node);
        aixos_list_init(&timer->pending_node);
        timer->pending = 0U;
        callback = timer->callback;
        arg = timer->arg;
        aixos_arch_int_restore(flags);
        callback(arg);
        dispatched++;
    }
    return dispatched;
}

static void timer_service_entry(void *arg)
{
    (void)arg;
    for (;;) {
        aixos_arch_flags_t flags;
        (void)aixos_timer_dispatch();
        flags = aixos_arch_int_disable();
        if (!aixos_list_is_empty(&pending_timers)) {
            aixos_arch_int_restore(flags);
            continue;
        }
        (void)aixos_task_block_current(&service_waiters, NULL,
                                       AIXOS_OBJ_TIMER, UINT32_MAX, flags);
#ifdef AIXOS_HOST_TEST
        return;
#endif
    }
}

int aixos_timer_service_start(void)
{
    /* Dynamically allocate the timer service task */
    aixos_handle_t task = aixos_task_create(
        "timer", timer_service_entry, NULL,
        AIXOS_CFG_TIMER_STACK_SIZE, AIXOS_CFG_TIMER_TASK_PRIORITY);
    return task == AIXOS_HANDLE_INVALID ? AIXOS_ERR_NOMEM : AIXOS_OK;
}

#ifdef AIXOS_HOST_TEST
void aixos_test_timer_service_entry(void)
{
    timer_service_entry(NULL);
}
#endif
