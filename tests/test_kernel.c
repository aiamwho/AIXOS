#include "test.h"
#include "aixos/aixos.h"
#include "kernel/sched.h"

static void dummy_task(void *arg)
{
    (void)arg;
}

static unsigned int timer_fires;

static void timer_callback(void *arg)
{
    (void)arg;
    timer_fires++;
}

void test_kernel_primitives(void)
{
    aixos_handle_t task;
    aixos_handle_t task2;
    aixos_handle_t sem;
    aixos_handle_t mutex;
    aixos_handle_t event;
    aixos_handle_t mq;
    aixos_handle_t pipe;
    aixos_handle_t timer;
    aixos_tcb_t *tcb;
    char out[4] = {0};
    size_t out_size = 0;
    uint32_t matched = 0;

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    task = aixos_task_create("test", dummy_task, NULL, 256, 3);
    CHECK(task != AIXOS_HANDLE_INVALID);
    CHECK(aixos_tcb_from_handle(task) != NULL);
    task2 = aixos_task_create("test2", dummy_task, NULL, 256, 4);
    CHECK(task2 != AIXOS_HANDLE_INVALID);

    sem = aixos_sem_create(1);
    CHECK(sem != AIXOS_HANDLE_INVALID);
    CHECK(aixos_sem_wait(sem, 0) == AIXOS_OK);
    CHECK(aixos_sem_wait(sem, 0) == AIXOS_ERR_TIMEOUT);
    CHECK(aixos_sem_post(sem) == AIXOS_OK);
    CHECK(aixos_sem_delete(sem) == AIXOS_OK);
    CHECK(aixos_sem_get_count(sem) == AIXOS_ERR_INVAL);

    mutex = aixos_mutex_create();
    CHECK(mutex != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(aixos_tcb_from_handle(task));
    CHECK(aixos_mutex_lock(mutex, 0) == AIXOS_OK);
    CHECK(aixos_mutex_unlock(mutex) == AIXOS_OK);
    CHECK(aixos_mutex_delete(mutex) == AIXOS_OK);

    event = aixos_event_create();
    CHECK(aixos_event_set(event, 3) == AIXOS_OK);
    CHECK(aixos_event_wait(event, 1, AIXOS_EVENT_AND | AIXOS_EVENT_CLEAR,
                           0, &matched) == AIXOS_OK);
    CHECK(matched == 1U);
    CHECK(aixos_event_clear(event, 2) == AIXOS_OK);
    CHECK(aixos_event_delete(event) == AIXOS_OK);

    mq = aixos_mq_create(2, sizeof(out));
    CHECK(aixos_mq_send(mq, "abc", 4, 0) == AIXOS_OK);
    CHECK(aixos_mq_recv(mq, out, sizeof(out), &out_size, 0) == AIXOS_OK);
    CHECK(out_size == 4);
    CHECK(out[0] == 'a' && out[2] == 'c');
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);

    pipe = aixos_pipe_create(4);
    CHECK(aixos_pipe_write(pipe, "xyz", 3, 0) == 3);
    CHECK(aixos_pipe_read(pipe, out, 3, 0) == 3);
    CHECK(out[0] == 'x' && out[2] == 'z');
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);

    tcb = aixos_tcb_from_handle(task);
    aixos_test_set_current(tcb);
    CHECK(aixos_task_block_current(NULL, NULL, AIXOS_OBJ_UNUSED, 5, 0) == AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_DELAYED);
    aixos_task_tick(4);
    CHECK(tcb->state == AIXOS_TASK_DELAYED);
    aixos_task_tick(5);
    CHECK(tcb->state == AIXOS_TASK_READY);

    timer_fires = 0;
    timer = aixos_timer_create("unit", AIXOS_TIMER_PERIODIC,
                               timer_callback, NULL);
    CHECK(timer != AIXOS_HANDLE_INVALID);
    CHECK(aixos_timer_start(timer, 3) == AIXOS_OK);
    aixos_timer_tick(2);
    CHECK(timer_fires == 0);
    aixos_timer_tick(3);
    CHECK(timer_fires == 0);
    CHECK(aixos_timer_dispatch() == 1U);
    CHECK(timer_fires == 1);
    aixos_timer_tick(6);
    CHECK(aixos_timer_dispatch() == 1U);
    CHECK(timer_fires == 2);
    CHECK(aixos_timer_delete(timer) == AIXOS_OK);

    aixos_test_set_current(aixos_tcb_from_handle(task2));
    CHECK(aixos_task_delete(task2) == AIXOS_OK);
    CHECK(aixos_tcb_from_handle(task2) == NULL);
}
