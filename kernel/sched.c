#include "kernel/sched.h"
#include "aixos/trace.h"
#include "aixos/timer.h"
#include "kernel/list.h"
#include "config/aixos_cfg.h"
#include "aixos/arch/arch.h"

/* Timing-wheel tick advance hook. */
#if AIXOS_CFG_ENABLE_TIME_WHEEL
extern void aixos_timing_wheel_tick(uint32_t current_tick);
#endif

#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
static aixos_list_t ready_queues[AIXOS_CFG_MAX_PRIORITY];
static uint64_t ready_bitmap[(AIXOS_CFG_MAX_PRIORITY + 63) / 64];
#elif AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_SIMPLE
static aixos_list_t ready_list;
#endif
static volatile uint64_t total_ticks;
static volatile uint64_t idle_ticks;
static volatile uint64_t switch_count;

aixos_tcb_t *g_cur_task;

void aixos_sched_init(void)
{
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
    int i;
    for (i = 0; i < AIXOS_CFG_MAX_PRIORITY; i++) {
        aixos_list_init(&ready_queues[i]);
    }
    for (i = 0; i < (int)((AIXOS_CFG_MAX_PRIORITY + 63) / 64); i++) {
        ready_bitmap[i] = 0;
    }
#elif AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_SIMPLE
    aixos_list_init(&ready_list);
#endif
    total_ticks = 0;
    idle_ticks = 0;
    switch_count = 0;
    g_cur_task = NULL;
}

#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_SIMPLE
static void sched_simple_insert(aixos_tcb_t *tcb)
{
    aixos_list_t *position;
    AIXOS_LIST_FOR_EACH(position, &ready_list) {
        aixos_tcb_t *queued =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, ready_node);
        if (tcb->priority > queued->priority) {
            __aixos_list_add(&tcb->ready_node, position->prev, position);
            return;
        }
    }
    aixos_list_add_tail(&tcb->ready_node, &ready_list);
}

static void sched_simple_insert_after_same_priority(aixos_tcb_t *tcb)
{
    aixos_list_t *position;
    aixos_list_t *last_same = NULL;
    AIXOS_LIST_FOR_EACH(position, &ready_list) {
        aixos_tcb_t *queued =
            AIXOS_CONTAINER_OF(position, aixos_tcb_t, ready_node);
        if (queued->priority == tcb->priority) {
            last_same = position;
        } else if (queued->priority < tcb->priority) {
            break;
        }
    }
    if (last_same != NULL) {
        __aixos_list_add(&tcb->ready_node, last_same, last_same->next);
    } else {
        sched_simple_insert(tcb);
    }
}
#endif

void aixos_sched_add_task(aixos_tcb_t *tcb)
{
    int priority;
    if (tcb == NULL) {
        return;
    }
    priority = tcb->priority;
    if (priority < 0 || priority >= AIXOS_CFG_MAX_PRIORITY) {
        return;
    }
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
    aixos_list_add_tail(&tcb->ready_node, &ready_queues[priority]);
    ready_bitmap[priority / 64] |= UINT64_C(1) << ((unsigned int)priority % 64);
#elif AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_SIMPLE
    sched_simple_insert(tcb);
#endif
}

void aixos_sched_remove_task(aixos_tcb_t *tcb)
{
    int priority;
    if (tcb == NULL || tcb->ready_node.next == NULL) {
        return;
    }
    priority = tcb->priority;
    aixos_list_del(&tcb->ready_node);
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
    if (aixos_list_is_empty(&ready_queues[priority])) {
        ready_bitmap[priority / 64] &= ~(UINT64_C(1) << ((unsigned int)priority % 64));
    }
#else
    (void)priority;
#endif
}

void aixos_sched_requeue_task(aixos_tcb_t *tcb, int old_priority)
{
    if (tcb == NULL || old_priority < 0 ||
        old_priority >= AIXOS_CFG_MAX_PRIORITY) {
        return;
    }
    if (tcb->ready_node.next != NULL) {
        aixos_list_del(&tcb->ready_node);
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
        if (aixos_list_is_empty(&ready_queues[old_priority])) {
            ready_bitmap[old_priority / 64] &= ~(UINT64_C(1) << ((unsigned int)old_priority % 64));
        }
#endif
        aixos_sched_add_task(tcb);
    }
}

void aixos_sched_rotate_current(void)
{
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
    aixos_list_t *head;
#endif
    if (g_cur_task == NULL || g_cur_task->ready_node.next == NULL) {
        return;
    }
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
    head = &ready_queues[g_cur_task->priority];
    if (head->next != head->prev) {
        aixos_list_del(&g_cur_task->ready_node);
        aixos_list_add_tail(&g_cur_task->ready_node, head);
    }
#elif AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_SIMPLE
    if (g_cur_task->ready_node.next != &ready_list) {
        aixos_tcb_t *next =
            AIXOS_CONTAINER_OF(g_cur_task->ready_node.next,
                               aixos_tcb_t, ready_node);
        if (next->priority == g_cur_task->priority) {
            aixos_list_del(&g_cur_task->ready_node);
            sched_simple_insert_after_same_priority(g_cur_task);
        }
    }
#endif
}

static aixos_tcb_t *sched_find_highest(void)
{
#if AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_BITMAP
    int word;
    int highest;
    aixos_list_t *node;

    for (word = (int)((AIXOS_CFG_MAX_PRIORITY + 63) / 64) - 1;
         word >= 0; word--) {
        if (ready_bitmap[word] != 0U) {
            highest = (word * 64) + (63 - __builtin_clzll(ready_bitmap[word]));
            if (highest >= AIXOS_CFG_MAX_PRIORITY) {
                highest = AIXOS_CFG_MAX_PRIORITY - 1;
            }
            node = aixos_list_first(&ready_queues[highest]);
            return AIXOS_CONTAINER_OF(node, aixos_tcb_t, ready_node);
        }
    }
#elif AIXOS_CFG_SCHEDULER == AIXOS_CFG_SCHED_SIMPLE
    aixos_list_t *node;
    if (!aixos_list_is_empty(&ready_list)) {
        node = aixos_list_first(&ready_list);
        return AIXOS_CONTAINER_OF(node, aixos_tcb_t, ready_node);
    }
#endif
    return NULL;
}

void aixos_sched_request_preemption(void)
{
    aixos_tcb_t *highest = sched_find_highest();
    if (highest == NULL || highest == g_cur_task) {
        return;
    }
    if (g_cur_task == NULL || highest->priority > g_cur_task->priority) {
        aixos_reschedule_request();
    }
}

void aixos_schedule(void)
{
    aixos_tcb_t *previous = g_cur_task;
    aixos_tcb_t *next = sched_find_highest();

    if (previous != NULL && previous->state == AIXOS_TASK_RUNNING) {
        previous->state = AIXOS_TASK_READY;
    }
    g_cur_task = next;
    if (next != NULL) {
        next->state = AIXOS_TASK_RUNNING;
    }
    aixos_arch_mpu_configure_task(next);
    if (next != previous) {
        switch_count++;
        if (next != NULL) {
            next->switch_count++;
        }
        AIXOS_TRACE(AIXOS_TRACE_TASK_SWITCH,
                    previous != NULL ? (uint32_t)previous->handle : UINT32_MAX,
                    next != NULL ? (uint32_t)next->handle : UINT32_MAX);
    }
}

void aixos_tick_handler(void)
{
    total_ticks++;

    /* Advance the timing wheel when enabled. */
#if AIXOS_CFG_ENABLE_TIME_WHEEL
    aixos_timing_wheel_tick((uint32_t)total_ticks);
#endif

    if (g_cur_task != NULL) {
        g_cur_task->runtime_ticks++;
        if (g_cur_task->priority == AIXOS_CFG_IDLE_PRIORITY) {
            idle_ticks++;
        }
        if (g_cur_task->time_slice > 0U) {
            g_cur_task->time_slice--;
        }
    }

    aixos_task_tick((uint32_t)total_ticks);
    aixos_timer_tick((uint32_t)total_ticks);

    if (g_cur_task != NULL && g_cur_task->time_slice == 0U) {
        g_cur_task->time_slice = AIXOS_CFG_TIME_SLICE_TICKS;
        aixos_sched_rotate_current();
        aixos_reschedule_request();
    }
}

uint32_t aixos_get_tick(void)
{
    return (uint32_t)total_ticks;
}

uint32_t aixos_cpu_usage_get(void)
{
    uint64_t total;
    uint64_t idle;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    total = total_ticks;
    idle = idle_ticks;
    aixos_arch_int_restore(flags);
    if (total == 0U || idle > total) {
        return 0;
    }
    return (uint32_t)(((total - idle) * 100U) / total);
}

void aixos_sched_stats_snapshot(aixos_sched_stats_t *stats)
{
    aixos_arch_flags_t flags;
    if (stats == NULL) {
        return;
    }
    flags = aixos_arch_int_disable();
    stats->total_ticks = total_ticks;
    stats->idle_ticks = idle_ticks;
    stats->switch_count = switch_count;
    aixos_arch_int_restore(flags);
}

#ifdef AIXOS_HOST_TEST
void aixos_test_set_current(aixos_tcb_t *tcb)
{
    g_cur_task = tcb;
    if (tcb != NULL) {
        tcb->state = AIXOS_TASK_RUNNING;
    }
}

void aixos_test_set_sched_stats(uint64_t total, uint64_t idle,
                                uint64_t switches)
{
    total_ticks = total;
    idle_ticks = idle;
    switch_count = switches;
}
#endif
