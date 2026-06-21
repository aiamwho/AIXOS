#include "test.h"
#include "aixos/aixos.h"
#include "kernel/sched.h"
#include "aixos/heap.h"
#include <stdint.h>

static char test_buf[512] __attribute__((aligned(512)));
static void user_entry(void *arg)
{
    (void)arg;
}

void test_microkernel_services(void)
{
    aixos_handle_t user;
    aixos_handle_t kernel;
    aixos_cap_t sem;
    aixos_cap_t mq;
    char *message = (char *)&test_buf[0];
    char *output = (char *)&test_buf[64];
    size_t *received = (size_t *)&test_buf[128];
    aixos_syscall_request_t invalid = { AIXOS_SC_COUNT, {0} };

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();

    /* Dynamic user task creation */
    user = aixos_user_task_create("user", user_entry, NULL, 512, 2);
    CHECK(user != AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_is_user(user) == 1);
    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)test_buf,
                                    sizeof(test_buf),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_OK);

    kernel = aixos_task_create("kernel", user_entry, NULL, 256, 1);
    CHECK(kernel != AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_is_user(kernel) == 0);
    aixos_test_set_current(aixos_tcb_from_handle(kernel));
    CHECK(aixos_syscall_dispatch(&invalid) == AIXOS_ERR_PERM);

    aixos_test_set_current(aixos_tcb_from_handle(user));
    CHECK(aixos_user_task_self() == user);
    CHECK(aixos_syscall_dispatch(&invalid) == AIXOS_ERR_FAULT);
    CHECK(aixos_user_memory_check(message, 8, 1) == AIXOS_OK);
    CHECK(aixos_user_memory_check(&invalid, sizeof(invalid), 0) ==
          AIXOS_ERR_FAULT);

    sem = aixos_user_sem_create(1);
    CHECK(sem >= 0);
    CHECK(aixos_user_sem_wait(sem, 0) == AIXOS_OK);
    CHECK(aixos_user_sem_wait(sem, 0) == AIXOS_ERR_TIMEOUT);
    CHECK(aixos_user_sem_post(sem) == AIXOS_OK);
    CHECK(aixos_user_sem_wait(sem + 1, 0) == AIXOS_ERR_PERM);
    CHECK(aixos_user_sem_delete(sem) == AIXOS_OK);
    CHECK(aixos_user_sem_post(sem) == AIXOS_ERR_PERM);

    message[0] = 'o';
    message[1] = 's';
    message[2] = '\0';
    mq = aixos_user_mq_create(2, 8);
    CHECK(mq >= 0);
    CHECK(aixos_user_mq_send(mq, message, 3, 0) == AIXOS_OK);
    CHECK(aixos_user_mq_send(mq, &invalid, 3, 0) == AIXOS_ERR_FAULT);
    *received = 0U;
    CHECK(aixos_user_mq_recv(mq, output, 8, received, 0) == AIXOS_OK);
    CHECK(*received == 3U);
    CHECK(output[0] == 'o' && output[1] == 's');
    CHECK(aixos_user_mq_delete(mq) == AIXOS_OK);
    CHECK(aixos_user_cap_close(mq) == AIXOS_ERR_PERM);

    /* ── Sync Message IPC tests ──────────────────────────────────── */
    {
        aixos_cap_t ch, conn;
        int result;
        ch = aixos_user_channel_create(0);
        CHECK(ch >= 0);
        conn = aixos_user_connect(ch);
        CHECK(conn >= 0);
        /* msg_send requires a receiver; this is a non-blocking test only. */
        result = aixos_user_disconnect(conn);
        CHECK(result == AIXOS_OK);
        result = aixos_user_channel_destroy(ch);
        CHECK(result == AIXOS_OK);
    }

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(user) == AIXOS_OK);
    CHECK(aixos_task_delete(kernel) == AIXOS_OK);
}
