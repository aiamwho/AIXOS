#include "aixos/task.h"
#include "aixos/heap.h"
#include "aixos/mutex.h"
#include "aixos/mpu.h"
#include "aixos/timer.h"
#include "aixos/trace.h"
#include "kernel/list.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"
#include "kernel/heap_internal.h"

static aixos_list_t all_task_list;
static aixos_list_t timeout_list;
static aixos_list_t reap_list;
static int task_count;
static int is_system_running;
int aixos_first_start;

static void reap_stopped_tasks(void)
{
    aixos_list_t *position;
    aixos_list_t *next;
    AIXOS_LIST_FOR_EACH_SAFE(position, next, &reap_list) {
        aixos_tcb_t *tcb =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, all_node);
        aixos_list_del(&tcb->all_node);
        if (tcb->owns_stack != 0U) {
            aixos_free(tcb->stack_alloc_base);
        }
        if (tcb->owns_tcb != 0U) {
            aixos_free(tcb);
        }
    }
}

static int tick_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static int task_stack_guard_ok(const aixos_tcb_t *tcb)
{
    const uint8_t *stack;
    uint32_t i;
    if (tcb == NULL || tcb->stack_base == NULL ||
        tcb->stack_size < AIXOS_CFG_STACK_GUARD_BYTES) {
        return 0;
    }
    stack = (const uint8_t *)tcb->stack_base;
    for (i = 0U; i < AIXOS_CFG_STACK_GUARD_BYTES; i++) {
        if (stack[i] != 0xE7U) {
            return 0;
        }
    }
    return 1;
}

static void task_detach_wait_locked(aixos_tcb_t *tcb, int result)
{
    if (tcb->wait_node.next != &tcb->wait_node) {
        aixos_list_del(&tcb->wait_node);
        aixos_list_init(&tcb->wait_node);
        aixos_mutex_waiter_removed(tcb);
    }
    if (tcb->timeout_node.next != &tcb->timeout_node) {
        aixos_list_del(&tcb->timeout_node);
        aixos_list_init(&tcb->timeout_node);
    }
    tcb->wait_obj = NULL;
    tcb->wait_list = NULL;
    tcb->wait_type = AIXOS_OBJ_UNUSED;
    tcb->wait_result = result;
}

static void task_wait_insert_priority(aixos_list_t *wait_list,
                                      aixos_tcb_t *tcb)
{
    aixos_list_t *position;
    AIXOS_LIST_FOR_EACH(position, wait_list) {
        aixos_tcb_t *queued =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, wait_node);
        if (tcb->priority > queued->priority) {
            __aixos_list_add(&tcb->wait_node, position->prev, position);
            return;
        }
    }
    aixos_list_add_tail(&tcb->wait_node, wait_list);
}

uint32_t aixos_ms_to_ticks(uint32_t ms)
{
    uint64_t ticks;
    if (ms == UINT32_MAX) {
        return UINT32_MAX;
    }
    ticks = ((uint64_t)ms * AIXOS_CFG_SYSTICK_HZ + 999U) / 1000U;
    if (ticks == 0U && ms != 0U) {
        ticks = 1U;
    }
    if (ticks > AIXOS_CFG_TIMEOUT_MAX_TICKS) {
        ticks = AIXOS_CFG_TIMEOUT_MAX_TICKS;
    }
    return (uint32_t)ticks;
}

uint32_t aixos_timeout_remaining_ms(uint32_t start_tick, uint32_t timeout_ms)
{
    uint32_t total;
    uint32_t elapsed;
    uint32_t remaining;
    uint64_t milliseconds;
    if (timeout_ms == UINT32_MAX) {
        return UINT32_MAX;
    }
    total = aixos_ms_to_ticks(timeout_ms);
    elapsed = aixos_get_tick() - start_tick;
    if (elapsed >= total) {
        return 0U;
    }
    remaining = total - elapsed;
    milliseconds = ((uint64_t)remaining * 1000U +
                    AIXOS_CFG_SYSTICK_HZ - 1U) / AIXOS_CFG_SYSTICK_HZ;
    return milliseconds > UINT32_MAX ? UINT32_MAX : (uint32_t)milliseconds;
}

static void timeout_insert(aixos_tcb_t *tcb)
{
    aixos_list_t *position;
    AIXOS_LIST_FOR_EACH(position, &timeout_list) {
        aixos_tcb_t *queued =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, timeout_node);
        if ((int32_t)(tcb->wake_tick - queued->wake_tick) < 0) {
            __aixos_list_add(&tcb->timeout_node, position->prev, position);
            return;
        }
    }
    aixos_list_add_tail(&tcb->timeout_node, &timeout_list);
}

static void idle_task_entry(void *arg)
{
    (void)arg;
    for (;;) {
        reap_stopped_tasks();
#ifdef AIXOS_HOST_TEST
        return;
#else
        __asm volatile("wfi");
#endif
    }
}

#ifdef AIXOS_HOST_TEST
static void task_exit_trap(void);
#else
static void task_exit_trap(void) __attribute__((noreturn));
#endif

static void task_exit_trap(void)
{
    (void)aixos_task_delete(aixos_task_self());
#ifdef AIXOS_HOST_TEST
    return;
#else
    for (;;) {
    }
#endif
}

void aixos_task_return_trap(void)
{
    task_exit_trap();
}

aixos_tcb_t *aixos_tcb_from_handle(aixos_handle_t handle)
{
    return (aixos_tcb_t *)aixos_obj_from_handle(handle, AIXOS_OBJ_TASK);
}

int aixos_task_init(void)
{
    aixos_list_init(&all_task_list);
    aixos_list_init(&timeout_list);
    aixos_list_init(&reap_list);
    task_count = 0;
    is_system_running = 0;
    aixos_first_start = 0;
    return AIXOS_OK;
}

static aixos_handle_t task_create_common(const char *name,
                                         void (*entry)(void *), void *arg,
                                         void *stack, size_t stack_size,
                                         int priority, aixos_tcb_t *provided_tcb,
                                         int system_task, int user_task)
{
    aixos_tcb_t *tcb;
    void *stack_base;
    void *stack_alloc_base;
    int slot;
    aixos_handle_t handle;
    aixos_arch_flags_t flags;

    if (aixos_in_isr() || entry == NULL || priority < 0 ||
        priority >= AIXOS_CFG_MAX_PRIORITY ||
        task_count >= AIXOS_CFG_TASK_HANDLE_LIMIT ||
        (!system_task && !is_system_running &&
         task_count >= AIXOS_CFG_TASK_HANDLE_LIMIT -
                       AIXOS_CFG_SYSTEM_TASKS_RESERVED)) {
        return AIXOS_HANDLE_INVALID;
    }
    if (user_task &&
        (stack_size < AIXOS_CFG_MPU_MIN_REGION_SIZE ||
         (stack_size & (stack_size - 1U)) != 0U)) {
        return AIXOS_HANDLE_INVALID;
    }
    if (stack_size < AIXOS_CFG_MIN_TASK_STACK_SIZE) {
        return AIXOS_HANDLE_INVALID;
    }
    if (user_task && stack != NULL &&
        (((uintptr_t)stack & (stack_size - 1U)) != 0U)) {
        return AIXOS_HANDLE_INVALID;
    }

    tcb = provided_tcb;
    if (tcb == NULL) {
        tcb = (aixos_tcb_t *)aixos_kernel_malloc(sizeof(*tcb));
        if (tcb == NULL) {
            return AIXOS_HANDLE_INVALID;
        }
    }
    stack_base = stack;
    stack_alloc_base = stack;
    if (stack_base == NULL) {
        size_t alloc_size = stack_size;
        if (user_task) {
            if (stack_size > SIZE_MAX - (stack_size - 1U)) {
                if (provided_tcb == NULL) {
                    aixos_free(tcb);
                }
                return AIXOS_HANDLE_INVALID;
            }
            alloc_size = stack_size + (stack_size - 1U);
        }
        stack_alloc_base = aixos_kernel_malloc(alloc_size);
        if (stack_alloc_base == NULL) {
            if (provided_tcb == NULL) {
                aixos_free(tcb);
            }
            return AIXOS_HANDLE_INVALID;
        }
        stack_base = stack_alloc_base;
        if (user_task) {
            stack_base = (void *)(((uintptr_t)stack_alloc_base +
                                   (stack_size - 1U)) &
                                  ~((uintptr_t)stack_size - 1U));
        }
    }

    flags = aixos_arch_int_disable();
    slot = aixos_slot_alloc(AIXOS_POOL_TASK, tcb);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        if (stack == NULL) {
            aixos_free(stack_alloc_base);
        }
        if (provided_tcb == NULL) {
            aixos_free(tcb);
        }
        return AIXOS_HANDLE_INVALID;
    }
    handle = aixos_slot_handle(AIXOS_POOL_TASK, slot);

    memset(tcb, 0, sizeof(*tcb));
    aixos_list_init(&tcb->ready_node);
    aixos_list_init(&tcb->all_node);
    aixos_list_init(&tcb->wait_node);
    aixos_list_init(&tcb->timeout_node);
    aixos_list_init(&tcb->held_mutexes);
    if (name != NULL) {
        strncpy(tcb->name, name, sizeof(tcb->name) - 1U);
    }
    tcb->priority = priority;
    tcb->base_priority = priority;
    tcb->state = AIXOS_TASK_READY;
    tcb->handle = handle;
    tcb->stack_base = stack_base;
    tcb->stack_alloc_base = stack_alloc_base;
    tcb->stack_size = (uint32_t)stack_size;
    tcb->time_slice = AIXOS_CFG_TIME_SLICE_TICKS;
    tcb->owns_tcb = provided_tcb == NULL;
    tcb->owns_stack = stack == NULL;
    tcb->domain = user_task ? AIXOS_DOMAIN_USER : AIXOS_DOMAIN_KERNEL;
    tcb->arch_control = user_task ? 3U : 2U;
    memset(stack_base, 0xA5, stack_size);
    memset(stack_base, 0xE7, AIXOS_CFG_STACK_GUARD_BYTES);
    tcb->stack_top = aixos_arch_stack_init(entry,
        (void *)((uintptr_t)stack_base + stack_size), arg, user_task);
    aixos_mpu_task_init(tcb);

    aixos_list_add_tail(&tcb->all_node, &all_task_list);
    task_count++;
    if (is_system_running) {
        aixos_sched_add_task(tcb);
    }
    aixos_arch_int_restore(flags);
    AIXOS_TRACE(AIXOS_TRACE_TASK_CREATE, (uint32_t)handle, (uint32_t)priority);
    return handle;
}

aixos_handle_t aixos_task_create(const char *name, void (*entry)(void *),
                                 void *arg, size_t stack_size, int priority)
{
    return task_create_common(name, entry, arg, NULL, stack_size, priority,
                              NULL, 0, 0);
}

aixos_handle_t aixos_user_task_create(const char *name,
                                       void (*entry)(void *), void *arg,
                                       size_t stack_size, int priority)
{
    /* User task stacks are power-of-two MPU regions. */
    if (stack_size < 256U || (stack_size & (stack_size - 1U)) != 0U) {
        return AIXOS_HANDLE_INVALID;
    }
    return task_create_common(name, entry, arg, NULL, stack_size, priority,
                              NULL, 0, 1);
}

aixos_handle_t aixos_task_create_static(const char *name,
                                        void (*entry)(void *), void *arg,
                                        void *stack, size_t stack_size,
                                        int priority, aixos_tcb_t *tcb)
{
    return task_create_common(name, entry, arg, stack, stack_size, priority,
                              tcb, 0, 0);
}

aixos_handle_t aixos_user_task_create_static(const char *name,
                                             void (*entry)(void *),
                                             void *arg,
                                             void *stack,
                                             size_t stack_size,
                                             int priority,
                                             aixos_tcb_t *tcb)
{
    return task_create_common(name, entry, arg, stack, stack_size, priority,
                              tcb, 0, 1);
}

int aixos_task_delete(aixos_handle_t task)
{
    aixos_tcb_t *tcb;
    int is_current;
    aixos_arch_flags_t flags = aixos_arch_int_disable();

    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (!aixos_mutex_task_can_delete(tcb)) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_BUSY;
    }
    is_current = tcb == g_cur_task;
    if (is_current && aixos_sched_is_locked()) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_LOCKED;
    }
    aixos_sched_remove_task(tcb);
    task_detach_wait_locked(tcb, AIXOS_ERR_INTR);
    aixos_list_del(&tcb->all_node);
    tcb->state = AIXOS_TASK_STOP;
    aixos_slot_free(AIXOS_POOL_TASK, (int)AIXOS_HANDLE_IDX(task));
    task_count--;
    if (is_current) {
        aixos_list_add_tail(&tcb->all_node, &reap_list);
        g_cur_task = tcb;
        aixos_reschedule_request();
        aixos_arch_int_restore(flags);
#ifdef AIXOS_HOST_TEST
        g_cur_task = NULL;
        reap_stopped_tasks();
        return AIXOS_OK;
#else
        for (;;) {
        }
#endif
    } else {
        aixos_arch_int_restore(flags);
    }
    if (tcb->owns_stack != 0U) {
        aixos_free(tcb->stack_alloc_base);
    }
    if (tcb->owns_tcb != 0U) {
        aixos_free(tcb);
    }
    AIXOS_TRACE(AIXOS_TRACE_TASK_DELETE, (uint32_t)task, 0);
    return AIXOS_OK;
}

int aixos_task_block_current(aixos_list_t *wait_list, void *wait_obj,
                             aixos_obj_type_t wait_type, uint32_t timeout_ms,
                             uint32_t interrupt_flags)
{
    aixos_tcb_t *current = g_cur_task;
    uint32_t timeout_ticks;
    if (current == NULL || aixos_in_isr()) {
        aixos_arch_int_restore(interrupt_flags);
        return AIXOS_ERR_CONTEXT;
    }
    if (aixos_sched_is_locked()) {
        aixos_arch_int_restore(interrupt_flags);
        return AIXOS_ERR_LOCKED;
    }
    current->state = wait_type == AIXOS_OBJ_UNUSED ?
                     AIXOS_TASK_DELAYED : AIXOS_TASK_BLOCKED;
    current->wait_obj = wait_obj;
    current->wait_list = wait_list;
    current->wait_type = wait_type;
    current->wait_result = AIXOS_OK;
    aixos_sched_remove_task(current);
    if (wait_list != NULL) {
        task_wait_insert_priority(wait_list, current);
    }
    if (wait_type == AIXOS_OBJ_MUTEX) {
        aixos_mutex_waiter_added(current);
    }
    timeout_ticks = aixos_ms_to_ticks(timeout_ms);
    if (timeout_ticks != UINT32_MAX) {
        current->wake_tick = aixos_get_tick() + timeout_ticks;
        timeout_insert(current);
    }
    aixos_reschedule_request();
    aixos_arch_int_restore(interrupt_flags);
    return current->wait_result;
}

void aixos_task_waiter_priority_changed(aixos_tcb_t *tcb)
{
    if (tcb == NULL || tcb->wait_list == NULL ||
        tcb->wait_node.next == &tcb->wait_node ||
        tcb->wait_node.next == NULL) {
        return;
    }
    aixos_list_del(&tcb->wait_node);
    aixos_list_init(&tcb->wait_node);
    task_wait_insert_priority(tcb->wait_list, tcb);
}

void aixos_task_wake(aixos_tcb_t *tcb, int result)
{
    if (tcb == NULL ||
        (tcb->state != AIXOS_TASK_BLOCKED &&
         tcb->state != AIXOS_TASK_DELAYED)) {
        return;
    }
    task_detach_wait_locked(tcb, result);
    tcb->state = AIXOS_TASK_READY;
    aixos_sched_add_task(tcb);
    aixos_sched_request_preemption();
}

void aixos_task_tick(uint32_t now)
{
    aixos_list_t *position;
    aixos_list_t *next;
    if (g_cur_task != NULL && g_cur_task->stack_guard_failed == 0U &&
        !task_stack_guard_ok(g_cur_task)) {
        g_cur_task->stack_guard_failed = 1U;
        AIXOS_TRACE(AIXOS_TRACE_STACK_GUARD,
                    (uint32_t)g_cur_task->handle, 0U);
    }
    AIXOS_LIST_FOR_EACH_SAFE(position, next, &timeout_list) {
        aixos_tcb_t *tcb =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, timeout_node);
        int result;
        if (!tick_reached(now, tcb->wake_tick)) {
            break;
        }
        result = tcb->state == AIXOS_TASK_DELAYED ?
                 AIXOS_OK : AIXOS_ERR_TIMEOUT;
        aixos_task_wake(tcb, result);
    }
}

int aixos_task_sleep(uint32_t ms)
{
    aixos_arch_flags_t flags;
    int result;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    if (ms == 0U) {
        return aixos_task_yield();
    }
    flags = aixos_arch_int_disable();
    result = aixos_task_block_current(NULL, NULL, AIXOS_OBJ_UNUSED, ms, flags);
    return result;
}

int aixos_task_yield(void)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    aixos_sched_rotate_current();
    aixos_reschedule_request();
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

aixos_handle_t aixos_task_self(void)
{
    return g_cur_task != NULL ? g_cur_task->handle : AIXOS_HANDLE_INVALID;
}

int aixos_task_suspend(aixos_handle_t task)
{
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL || tcb->state == AIXOS_TASK_STOP ||
        tcb->state == AIXOS_TASK_SUSPENDED) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (tcb == g_cur_task && aixos_sched_is_locked()) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_LOCKED;
    }
    aixos_sched_remove_task(tcb);
    if (tcb->state == AIXOS_TASK_BLOCKED ||
        tcb->state == AIXOS_TASK_DELAYED) {
        task_detach_wait_locked(tcb, AIXOS_ERR_INTR);
    }
    tcb->state = AIXOS_TASK_SUSPENDED;
    if (tcb == g_cur_task) {
        aixos_reschedule_request();
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_task_resume(aixos_handle_t task)
{
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL || tcb->state != AIXOS_TASK_SUSPENDED) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    tcb->state = AIXOS_TASK_READY;
    aixos_sched_add_task(tcb);
    aixos_sched_request_preemption();
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_task_set_priority(aixos_handle_t task, int priority)
{
    aixos_tcb_t *tcb;
    int old_priority;
    aixos_arch_flags_t flags;
    if (priority < 0 || priority >= AIXOS_CFG_MAX_PRIORITY) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    old_priority = tcb->priority;
    tcb->base_priority = priority;
    tcb->priority = priority;
    aixos_sched_requeue_task(tcb, old_priority);
    aixos_mutex_task_priority_changed(tcb);
    aixos_sched_request_preemption();
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_task_get_info(aixos_handle_t task, aixos_task_info_t *info)
{
    aixos_tcb_t *tcb;
    uint32_t used = 0;
    const unsigned char *stack;
    aixos_arch_flags_t flags;
    if (info == NULL) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL || info == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    strncpy(info->name, tcb->name, sizeof(info->name));
    info->handle = tcb->handle;
    info->priority = tcb->priority;
    info->state = tcb->state;
    info->stack_size = tcb->stack_size;
    stack = (const unsigned char *)tcb->stack_base;
    used = AIXOS_CFG_STACK_GUARD_BYTES;
    while (used < tcb->stack_size && stack[used] == 0xA5U) {
        used++;
    }
    info->stack_used = tcb->stack_size - used;
    info->stack_free = used - AIXOS_CFG_STACK_GUARD_BYTES;
    info->stack_guard_ok = (uint8_t)task_stack_guard_ok(tcb);
    info->runtime_ticks = tcb->runtime_ticks;
    info->switch_count = tcb->switch_count;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_task_stack_check(aixos_handle_t task)
{
    aixos_tcb_t *tcb;
    int result;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    result = task_stack_guard_ok(tcb) ? AIXOS_OK : AIXOS_ERR_CORRUPT;
    aixos_arch_int_restore(flags);
    return result;
}

int aixos_task_is_user(aixos_handle_t task)
{
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    task = tcb->domain == AIXOS_DOMAIN_USER ? 1 : 0;
    aixos_arch_int_restore(flags);
    return (int)task;
}

void aixos_start(void)
{
    aixos_list_t *position;
    aixos_handle_t idle;
    is_system_running = 0;
    /* Dynamically allocate the idle task (stack + TCB via kernel malloc) */
    idle = aixos_task_create("idle", idle_task_entry, NULL,
                             AIXOS_CFG_IDLE_STACK_SIZE * sizeof(uint32_t),
                             AIXOS_CFG_IDLE_PRIORITY);
    if (idle == AIXOS_HANDLE_INVALID) {
        for (;;) {
        }
    }
    if (aixos_timer_service_start() != AIXOS_OK) {
        for (;;) {
        }
    }
#if AIXOS_CFG_HEAP_LOCK_ON_START
    aixos_heap_lockdown();
#endif
    AIXOS_LIST_FOR_EACH(position, &all_task_list) {
        aixos_tcb_t *tcb =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, all_node);
        if (tcb->state == AIXOS_TASK_READY) {
            aixos_sched_add_task(tcb);
        }
    }
    is_system_running = 1;
    aixos_schedule();
    aixos_first_start = 1;
    aixos_arch_systick_enable();
    aixos_arch_start_first_task();
#ifdef AIXOS_HOST_TEST
    idle_task_entry(NULL);
    return;
#endif
}

uint32_t aixos_task_count(void)
{
    return (uint32_t)task_count;
}

/* ============================================================
 * Task signal handling.
 * ============================================================ */

int aixos_task_signal_send(aixos_handle_t task, uint32_t signo)
{
#if AIXOS_CFG_ENABLE_SIGNALS
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags;
    
    if (signo >= 32U) {
        return AIXOS_ERR_INVAL;
    }
    
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    
    // 检查信号是否被屏蔽
    if ((tcb->signal_mask & (1U << signo)) != 0U) {
        // 信号被屏蔽，记录为待处理
        tcb->signal_pending |= (1U << signo);
        aixos_arch_int_restore(flags);
        return AIXOS_OK;
    }
    
    // 设置信号待处理标志
    tcb->signal_pending |= (1U << signo);
    tcb->pending_signal = 1U;
    
    // 如果任务阻塞在 signal wait 上，唤醒它
    if (tcb->state == AIXOS_TASK_BLOCKED &&
        tcb->wait_type == AIXOS_OBJ_SIGNAL &&
        tcb->wait_obj == tcb) {
        aixos_task_wake(tcb, AIXOS_OK);
    }
    
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
#else
    (void)task; (void)signo;
    return AIXOS_ERR_NOSYS;
#endif
}

int aixos_task_signal_handle(uint32_t signo, void (*handler)(void *), void *arg)
{
#if AIXOS_CFG_ENABLE_SIGNALS
    aixos_tcb_t *self = g_cur_task;
    if (self == NULL || signo >= 32U) {
        return AIXOS_ERR_INVAL;
    }
    // 注册信号处理函数
    self->signal_handler[signo] = (void *)handler;
    self->signal_arg[signo] = arg;
    return AIXOS_OK;
#else
    (void)signo; (void)handler; (void)arg;
    return AIXOS_ERR_NOSYS;
#endif
}

int aixos_task_signal_mask(uint32_t mask)
{
#if AIXOS_CFG_ENABLE_SIGNALS
    aixos_tcb_t *self = g_cur_task;
    if (self == NULL) {
        return AIXOS_ERR_INVAL;
    }
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    self->signal_mask = mask;
    // 检查是否有未被屏蔽的待处理信号
    uint32_t unmasked = self->signal_pending & ~mask;
    if (unmasked != 0U) {
        self->pending_signal = 1U;
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
#else
    (void)mask;
    return AIXOS_ERR_NOSYS;
#endif
}

int aixos_task_signal_pending(aixos_handle_t task, uint32_t *pending)
{
#if AIXOS_CFG_ENABLE_SIGNALS
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags;
    if (pending == NULL) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    *pending = tcb->signal_pending;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
#else
    (void)task; (void) pending;
    return AIXOS_ERR_NOSYS;
#endif
}

/* Signal delivery after task switches or selected blocking returns. */
int aixos_task_signal_deliver(void)
{
#if AIXOS_CFG_ENABLE_SIGNALS
    aixos_tcb_t *self = g_cur_task;
    uint32_t pending;
    uint32_t signo;
    
    if (self == NULL || self->pending_signal == 0U) {
        return AIXOS_OK;
    }
    
    pending = self->signal_pending & ~self->signal_mask;
    if (pending == 0U) {
        return AIXOS_OK;
    }
    
    // 查找最高优先级（最低位号）的待处理信号
    signo = (uint32_t)__builtin_ctz(pending);
    
    // 清除待处理位
    self->signal_pending &= ~(1U << signo);
    if (self->signal_pending == 0U) {
        self->pending_signal = 0U;
    }
    
    // 如果有处理函数，调用它
    void (*handler)(void *) = (void (*)(void *))self->signal_handler[signo];
    if (handler != NULL) {
        void *arg = self->signal_arg[signo];
        // 在任务上下文中调用处理函数
        handler(arg);
    }
    
    return AIXOS_OK;
#else
    return AIXOS_OK;
#endif
}

/* Wake recheck for IPC condition validation. */
void aixos_task_wake_recheck(aixos_tcb_t *tcb, int result)
{
    if (tcb == NULL) return;
    // 设置等待结果，但不从等待队列移除
    // 调用者需自行重确认条件
    tcb->wait_result = result;
    if (tcb->state == AIXOS_TASK_BLOCKED || tcb->state == AIXOS_TASK_DELAYED) {
        aixos_task_wake(tcb, result);
    }
}
