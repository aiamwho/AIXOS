#include <stdint.h>
#include "test.h"
#include "aixos/aixos.h"
#include "aixos/arch/arch.h"
#include "kernel/sched.h"
#include "kernel/object.h"
#include "config/aixos_cfg.h"

extern void aixos_test_switch_requests_reset(void);
extern unsigned int aixos_test_switch_requests_get(void);

static void stress_task(void *arg)
{
    (void)arg;
}

void test_heap_operation_sequence(void)
{
    void *slots[16] = {0};
    uint32_t sizes[16] = {0};
    uint32_t state = UINT32_C(0x12345678);
    uint32_t i;

    aixos_heap_init(NULL, 0U);
    for (i = 0U; i < 2000U; i++) {
        uint32_t index;
        uint32_t operation;
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        index = (state >> 8) & 15U;
        operation = state & 3U;
        if (slots[index] == NULL) {
            sizes[index] = ((state >> 16) & 127U) + 1U;
            slots[index] = aixos_malloc(sizes[index]);
        } else if (operation == 0U) {
            aixos_free(slots[index]);
            slots[index] = NULL;
        } else if (operation == 1U) {
            uint32_t new_size = ((state >> 12) & 127U) + 1U;
            void *replacement = aixos_realloc(slots[index], new_size);
            if (replacement != NULL) {
                slots[index] = replacement;
                sizes[index] = new_size;
            }
        } else {
            ((uint8_t *)slots[index])[sizes[index] - 1U] =
                (uint8_t)state;
        }
        CHECK(aixos_heap_check() == AIXOS_OK);
    }
    for (i = 0U; i < 16U; i++) {
        aixos_free(slots[i]);
    }
    CHECK(aixos_heap_check() == AIXOS_OK);
}

void test_tick_wrap_timeout(void)
{
    aixos_handle_t task;
    aixos_tcb_t *tcb;

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();
    task = aixos_task_create("wrap", stress_task, NULL, 256U, 3);
    CHECK(task != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(task);
    aixos_test_set_current(tcb);
    aixos_test_set_sched_stats(UINT64_C(0xFFFFFFFE), 0U, 0U);
    CHECK(aixos_task_sleep(3U) == AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_DELAYED);
    aixos_task_tick(0U);
    CHECK(tcb->state == AIXOS_TASK_DELAYED);
    aixos_task_tick(1U);
    CHECK(tcb->state == AIXOS_TASK_READY);
    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(task) == AIXOS_OK);
}

void test_dynamic_task_capacity_and_priorities(void)
{
    enum {
        TEST_TASKS = AIXOS_CFG_TASK_HANDLE_LIMIT -
                     AIXOS_CFG_SYSTEM_TASKS_RESERVED
    };
    static aixos_tcb_t tcbs[TEST_TASKS];
    static uint8_t stacks[TEST_TASKS][256U] __attribute__((aligned(8)));
    aixos_handle_t handles[TEST_TASKS];
    aixos_handle_t dynamic_task;
    uint32_t i;

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    for (i = 0U; i < TEST_TASKS; i++) {
        int priority = (int)(i % AIXOS_CFG_MAX_PRIORITY);
        handles[i] = aixos_task_create_static("capacity", stress_task, NULL,
                                              stacks[i], sizeof(stacks[i]),
                                              priority, &tcbs[i]);
        CHECK(handles[i] != AIXOS_HANDLE_INVALID);
    }
    CHECK(aixos_task_count() == TEST_TASKS);
    CHECK(aixos_pool_get_usage(AIXOS_POOL_TASK) == TEST_TASKS);

    CHECK(aixos_task_set_priority(handles[TEST_TASKS - 1], 63) == AIXOS_OK);
    aixos_schedule();
    CHECK(g_cur_task != NULL);
    CHECK(aixos_tcb_from_handle(handles[TEST_TASKS-1])->priority == 63);
    aixos_test_set_current(NULL);

    CHECK(aixos_task_create("reserved", stress_task, NULL, 256U, 1) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_count() == TEST_TASKS);
    CHECK(aixos_pool_get_usage(AIXOS_POOL_TASK) ==
          TEST_TASKS);

    for (i = 0U; i < TEST_TASKS; i++) {
        CHECK(aixos_task_delete(handles[i]) == AIXOS_OK);
    }
    CHECK(aixos_task_count() == 0U);
    CHECK(aixos_pool_get_usage(AIXOS_POOL_TASK) == 0);

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();
    aixos_heap_lockdown();
    CHECK(aixos_malloc(16U) == NULL);
    dynamic_task = aixos_task_create("runtime", stress_task, NULL, 256U, 40);
    CHECK(dynamic_task != AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_delete(dynamic_task) == AIXOS_OK);
    CHECK(aixos_heap_is_locked());
}

void test_task_state_transitions(void)
{
    aixos_handle_t low_handle;
    aixos_handle_t high_handle;
    aixos_handle_t sem;
    aixos_tcb_t *low;
    aixos_tcb_t *high;

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    low_handle = aixos_task_create("low", stress_task, NULL, 256U, 2);
    high_handle = aixos_task_create("high", stress_task, NULL, 256U, 8);
    sem = aixos_sem_create(0);
    CHECK(low_handle != AIXOS_HANDLE_INVALID);
    CHECK(high_handle != AIXOS_HANDLE_INVALID);
    CHECK(sem != AIXOS_HANDLE_INVALID);
    low = aixos_tcb_from_handle(low_handle);
    high = aixos_tcb_from_handle(high_handle);

    aixos_test_set_current(high);
    CHECK(aixos_sem_wait(sem, 100U) == AIXOS_OK);
    CHECK(high->state == AIXOS_TASK_BLOCKED);
    CHECK(high->wait_node.next != &high->wait_node);
    CHECK(high->timeout_node.next != &high->timeout_node);

    aixos_test_set_current(low);
    CHECK(aixos_task_suspend(high_handle) == AIXOS_OK);
    CHECK(high->state == AIXOS_TASK_SUSPENDED);
    CHECK(high->wait_node.next == &high->wait_node);
    CHECK(high->timeout_node.next == &high->timeout_node);
    CHECK(high->wait_result == AIXOS_ERR_INTR);
    CHECK(aixos_task_suspend(high_handle) == AIXOS_ERR_INVAL);

    aixos_test_switch_requests_reset();
    CHECK(aixos_task_resume(high_handle) == AIXOS_OK);
    CHECK(high->state == AIXOS_TASK_READY);
    CHECK(aixos_test_switch_requests_get() == 1U);

    aixos_test_switch_requests_reset();
    CHECK(aixos_task_set_priority(high_handle, 1) == AIXOS_OK);
    CHECK(aixos_test_switch_requests_get() == 0U);
    CHECK(aixos_task_set_priority(low_handle, 0) == AIXOS_OK);
    CHECK(aixos_test_switch_requests_get() == 1U);

    aixos_test_set_current(NULL);
    CHECK(aixos_sem_delete(sem) == AIXOS_OK);
    CHECK(aixos_task_delete(high_handle) == AIXOS_OK);
    CHECK(aixos_task_delete(low_handle) == AIXOS_OK);
}

void test_priority_ordered_waiters(void)
{
    aixos_handle_t low_handle;
    aixos_handle_t high_handle;
    aixos_handle_t sem;
    aixos_tcb_t *low;
    aixos_tcb_t *high;

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    low_handle = aixos_task_create("low-wait", stress_task, NULL, 256U, 2);
    high_handle = aixos_task_create("high-wait", stress_task, NULL, 256U, 8);
    sem = aixos_sem_create(0);
    low = aixos_tcb_from_handle(low_handle);
    high = aixos_tcb_from_handle(high_handle);

    aixos_test_set_current(low);
    CHECK(aixos_sem_wait(sem, 100U) == AIXOS_OK);
    aixos_test_set_current(high);
    CHECK(aixos_sem_wait(sem, 100U) == AIXOS_OK);
    CHECK(low->state == AIXOS_TASK_BLOCKED);
    CHECK(high->state == AIXOS_TASK_BLOCKED);

    aixos_test_set_current(NULL);
    CHECK(aixos_sem_post(sem) == AIXOS_OK);
    CHECK(high->state == AIXOS_TASK_READY);
    CHECK(low->state == AIXOS_TASK_BLOCKED);

    CHECK(aixos_task_set_priority(low_handle, 9) == AIXOS_OK);
    CHECK(aixos_sem_post(sem) == AIXOS_OK);
    CHECK(low->state == AIXOS_TASK_READY);

    CHECK(aixos_sem_delete(sem) == AIXOS_OK);
    CHECK(aixos_task_delete(high_handle) == AIXOS_OK);
    CHECK(aixos_task_delete(low_handle) == AIXOS_OK);
}

void test_task_notifications(void)
{
    aixos_handle_t task;
    aixos_tcb_t *tcb;
    uint32_t value = 0U;

    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    task = aixos_task_create("notify", stress_task, NULL, 256U, 3);
    CHECK(task != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(task);
    aixos_test_set_current(tcb);

    CHECK(aixos_task_notify(task, 0x3U, AIXOS_NOTIFY_SET_BITS) == AIXOS_OK);
    CHECK(aixos_task_notify_wait(0U, UINT32_MAX, &value, 0U) == AIXOS_OK);
    CHECK(value == 0x3U);
    CHECK(tcb->notify_value == 0U);
    CHECK(aixos_task_notify(task, 7U, AIXOS_NOTIFY_NO_OVERWRITE) == AIXOS_OK);
    CHECK(aixos_task_notify(task, 8U, AIXOS_NOTIFY_NO_OVERWRITE) ==
          AIXOS_ERR_BUSY);
    CHECK(aixos_task_notify_wait(0U, UINT32_MAX, &value, 0U) == AIXOS_OK);
    CHECK(value == 7U);
    CHECK(aixos_task_notify_from_isr(task, 1U, AIXOS_NOTIFY_INCREMENT) ==
          AIXOS_ERR_CONTEXT);
    aixos_isr_enter();
    CHECK(aixos_task_notify(task, 1U, AIXOS_NOTIFY_INCREMENT) ==
          AIXOS_ERR_CONTEXT);
    CHECK(aixos_task_notify_from_isr(task, 1U, AIXOS_NOTIFY_INCREMENT) ==
          AIXOS_OK);
    aixos_isr_exit();
    CHECK(aixos_task_notify_take(1, 0U, &value) == AIXOS_OK);
    CHECK(value == 1U);
    CHECK(aixos_task_notify_wait(0U, 0U, &value, 0U) == AIXOS_ERR_AGAIN);

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(task) == AIXOS_OK);
}
