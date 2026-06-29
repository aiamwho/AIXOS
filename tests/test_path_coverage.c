#include <stdint.h>
#include <string.h>

#include "test.h"
#include "aixos/aixos.h"
#include "aixos/namespace.h"
#include "kernel/sched.h"
#include "kernel/timewheel.h"

static void path_dummy_task(void *arg)
{
    (void)arg;
}

static void path_reset_kernel(void)
{
    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();
    aixos_timer_init();
    aixos_namespace_init();
    aixos_timing_wheel_init();
}

void test_namespace_resource_manager(void)
{
    aixos_handle_t sem;
    aixos_handle_t mutex;
    aixos_handle_t resolved = AIXOS_HANDLE_INVALID;
    aixos_obj_type_t type = AIXOS_OBJ_UNUSED;
    uint16_t rights = 0U;
    aixos_cap_t cap = AIXOS_CAP_INVALID;
    aixos_handle_t user;
    aixos_tcb_t *user_tcb;
    uint32_t i;
    char name[32];

    path_reset_kernel();
    sem = aixos_sem_create(1);
    mutex = aixos_mutex_create();
    CHECK(sem != AIXOS_HANDLE_INVALID);
    CHECK(mutex != AIXOS_HANDLE_INVALID);

    CHECK(aixos_namespace_register(NULL, sem, AIXOS_OBJ_SEM,
                                   AIXOS_CAP_WAIT) == AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_register("", sem, AIXOS_OBJ_SEM,
                                   AIXOS_CAP_WAIT) == AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_register("invalid", AIXOS_HANDLE_INVALID,
                                   AIXOS_OBJ_SEM, AIXOS_CAP_WAIT) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_register("invalid", sem, AIXOS_OBJ_UNUSED,
                                   AIXOS_CAP_WAIT) == AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_register("012345678901234567890123456789012",
                                   sem, AIXOS_OBJ_SEM, AIXOS_CAP_WAIT) ==
          AIXOS_ERR_INVAL);

    CHECK(aixos_namespace_register("/sem/main", sem, AIXOS_OBJ_SEM,
                                   AIXOS_CAP_WAIT | AIXOS_CAP_SIGNAL) ==
          AIXOS_OK);
    CHECK(aixos_namespace_register("/sem/main", sem, AIXOS_OBJ_SEM,
                                   AIXOS_CAP_WAIT) == AIXOS_ERR_BUSY);
    CHECK(aixos_namespace_resolve(NULL, &resolved, &type, &rights) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_resolve("/sem/main", NULL, &type, &rights) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_resolve("/missing", &resolved, &type, &rights) ==
          AIXOS_ERR_NOT_FOUND);
    CHECK(aixos_namespace_resolve("/sem/main", &resolved, &type, &rights) ==
          AIXOS_OK);
    CHECK(resolved == sem);
    CHECK(type == AIXOS_OBJ_SEM);
    CHECK((rights & AIXOS_CAP_WAIT) != 0U);
    CHECK(aixos_namespace_resolve("/sem/main", &resolved, NULL, NULL) ==
          AIXOS_OK);

    user = aixos_user_task_create("user", path_dummy_task, NULL, 512U, 2);
    CHECK(user != AIXOS_HANDLE_INVALID);
    user_tcb = aixos_tcb_from_handle(user);
    CHECK(user_tcb != NULL);
    aixos_test_set_current(user_tcb);
    CHECK(aixos_namespace_open(NULL, AIXOS_CAP_WAIT, &cap) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_open("/sem/main", AIXOS_CAP_CONTROL, &cap) ==
          AIXOS_ERR_PERM);
    CHECK(aixos_namespace_open("/sem/main", AIXOS_CAP_WAIT, NULL) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_open("/sem/main", AIXOS_CAP_WAIT, &cap) ==
          AIXOS_OK);
    CHECK(cap >= 0);
    CHECK(aixos_user_sem_wait(cap, 0U) == AIXOS_OK);
    CHECK(aixos_user_sem_post(cap) == AIXOS_OK);
    CHECK(aixos_user_cap_close(cap) == AIXOS_OK);
    CHECK(aixos_user_sem_post(cap) == AIXOS_ERR_PERM);

    CHECK(aixos_namespace_unregister(NULL) == AIXOS_ERR_INVAL);
    CHECK(aixos_namespace_unregister("/missing") == AIXOS_ERR_NOT_FOUND);
    CHECK(aixos_namespace_unregister("/sem/main") == AIXOS_OK);
    CHECK(aixos_namespace_resolve("/sem/main", &resolved, &type, &rights) ==
          AIXOS_ERR_NOT_FOUND);

    aixos_namespace_init();
    for (i = 0U; i < AIXOS_CFG_NAMESPACE_MAX; i++) {
        int n = snprintf(name, sizeof(name), "/res/%02u", (unsigned)i);
        CHECK(n > 0);
        CHECK(aixos_namespace_register(name, mutex, AIXOS_OBJ_MUTEX,
                                       AIXOS_CAP_CONTROL) == AIXOS_OK);
    }
    CHECK(aixos_namespace_register("/res/full", mutex, AIXOS_OBJ_MUTEX,
                                   AIXOS_CAP_CONTROL) == AIXOS_ERR_NOMEM);

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(user) == AIXOS_OK);
    CHECK(aixos_sem_delete(sem) == AIXOS_OK);
    CHECK(aixos_mutex_delete(mutex) == AIXOS_OK);
}

void test_timing_wheel_paths(void)
{
    aixos_handle_t near_task;
    aixos_handle_t far_task;
    aixos_handle_t overflow_task;
    aixos_tcb_t *near_tcb;
    aixos_tcb_t *far_tcb;
    aixos_tcb_t *overflow_tcb;

    path_reset_kernel();
    near_task = aixos_task_create("near", path_dummy_task, NULL, 256U, 3);
    far_task = aixos_task_create("far", path_dummy_task, NULL, 256U, 4);
    overflow_task = aixos_task_create("overflow", path_dummy_task, NULL,
                                      256U, 5);
    CHECK(near_task != AIXOS_HANDLE_INVALID);
    CHECK(far_task != AIXOS_HANDLE_INVALID);
    CHECK(overflow_task != AIXOS_HANDLE_INVALID);
    near_tcb = aixos_tcb_from_handle(near_task);
    far_tcb = aixos_tcb_from_handle(far_task);
    overflow_tcb = aixos_tcb_from_handle(overflow_task);
    CHECK(near_tcb != NULL && far_tcb != NULL && overflow_tcb != NULL);

    near_tcb->state = AIXOS_TASK_DELAYED;
    near_tcb->wake_tick = 3U;
    aixos_timing_wheel_insert(near_tcb, near_tcb->wake_tick);
    aixos_timing_wheel_tick(0U);
    CHECK(near_tcb->state == AIXOS_TASK_DELAYED);
    aixos_timing_wheel_tick(1U);
    aixos_timing_wheel_tick(2U);
    aixos_timing_wheel_tick(3U);
    CHECK(near_tcb->state == AIXOS_TASK_READY);
    CHECK(near_tcb->wait_result == AIXOS_OK);

    far_tcb->state = AIXOS_TASK_BLOCKED;
    far_tcb->wait_type = AIXOS_OBJ_SEM;
    far_tcb->wake_tick = AIXOS_CFG_TW_SIZE1 + 4U;
    aixos_timing_wheel_insert(far_tcb, far_tcb->wake_tick);
    for (uint32_t tick = 4U; tick < AIXOS_CFG_TW_SIZE1 + 4U; tick++) {
        aixos_timing_wheel_tick(tick);
    }
    CHECK(far_tcb->state == AIXOS_TASK_BLOCKED);
    aixos_timing_wheel_tick(AIXOS_CFG_TW_SIZE1 + 4U);
    CHECK(far_tcb->state == AIXOS_TASK_READY);
    CHECK(far_tcb->wait_result == AIXOS_ERR_TIMEOUT);

    overflow_tcb->state = AIXOS_TASK_BLOCKED;
    overflow_tcb->wait_type = AIXOS_OBJ_EVENT;
    overflow_tcb->wake_tick = (AIXOS_CFG_TW_SIZE1 * AIXOS_CFG_TW_SIZE2) +
                              AIXOS_CFG_TW_SIZE1 + 7U;
    aixos_timing_wheel_insert(overflow_tcb, overflow_tcb->wake_tick);
    for (uint32_t tick = AIXOS_CFG_TW_SIZE1 + 5U;
         tick < overflow_tcb->wake_tick; tick++) {
        aixos_timing_wheel_tick(tick);
    }
    CHECK(overflow_tcb->state == AIXOS_TASK_BLOCKED);

    CHECK(aixos_task_delete(near_task) == AIXOS_OK);
    CHECK(aixos_task_delete(far_task) == AIXOS_OK);
    CHECK(aixos_task_delete(overflow_task) == AIXOS_OK);
}

void test_microkernel_sync_ipc_paths(void)
{
    static char user_stack[512] __attribute__((aligned(512)));
    static char user_buffer[512] __attribute__((aligned(512)));
    static aixos_tcb_t user_tcb;
    aixos_handle_t user;
    aixos_tcb_t *tcb;
    aixos_cap_t channel;
    aixos_cap_t connection;
    char *message = &user_buffer[0];
    char *reply = &user_buffer[64];
    char *received = &user_buffer[128];
    size_t *received_size = (size_t *)&user_buffer[256];
    aixos_syscall_request_t request = { 0U, { 0U, 0U, 0U, 0U, 0U } };

    path_reset_kernel();
    user = aixos_user_task_create_static("u", path_dummy_task, NULL,
                                         user_stack, sizeof(user_stack),
                                         2, &user_tcb);
    CHECK(user != AIXOS_HANDLE_INVALID);
    if (user == AIXOS_HANDLE_INVALID) {
        return;
    }
    tcb = aixos_tcb_from_handle(user);
    CHECK(tcb != NULL);
    if (tcb == NULL) {
        return;
    }
    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)user_buffer,
                                    sizeof(user_buffer),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_OK);
    aixos_test_set_current(tcb);

    CHECK(aixos_channel_create(0) == AIXOS_CAP_INVALID);
    CHECK(aixos_channel_destroy(0) == AIXOS_ERR_NOSYS);
    CHECK(aixos_connect(0) == AIXOS_CAP_INVALID);
    CHECK(aixos_disconnect(0) == AIXOS_ERR_NOSYS);
    CHECK(aixos_msg_send(0, message, 1U, reply, 1U) == AIXOS_ERR_NOSYS);
    CHECK(aixos_msg_receive(0, received, 1U, 0, &connection) ==
          AIXOS_ERR_NOSYS);
    CHECK(aixos_msg_reply(0, reply, 1U) == AIXOS_ERR_NOSYS);

    channel = aixos_user_channel_create(0);
    CHECK(channel >= 0);
    CHECK(aixos_user_msg_receive(channel, received, 0U, 0) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_user_connect(channel + 1) == AIXOS_ERR_PERM);
    connection = aixos_user_connect(channel);
    CHECK(connection >= 0);
    CHECK(aixos_user_msg_send(connection, message, 0U, reply,
                              4U) == AIXOS_ERR_INVAL);
    CHECK(aixos_user_msg_send(connection, message,
                              AIXOS_SYNC_MSG_MAX_SIZE + 1U, reply,
                              4U) == AIXOS_ERR_INVAL);
    CHECK(aixos_user_msg_send(connection, (void *)&request, 4U, reply,
                              4U) == AIXOS_ERR_FAULT);

    message[0] = 'p';
    message[1] = 'i';
    message[2] = 'n';
    message[3] = 'g';
    reply[0] = 0;
    CHECK(aixos_user_msg_send(connection, message, 4U, reply, 4U) ==
          AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_BLOCKED);
    CHECK(aixos_user_msg_reply(connection, message, 2U) == AIXOS_OK);
    CHECK(tcb->state == AIXOS_TASK_READY);
    CHECK(reply[0] == 'p' && reply[1] == 'i');
    CHECK(aixos_user_msg_reply(connection, message, 1U) ==
          AIXOS_ERR_NOT_FOUND);

    CHECK(aixos_user_msg_send(connection, message, 4U, NULL, 0U) ==
          AIXOS_OK);
    CHECK(aixos_user_msg_receive(channel, received, 8U, 0) == 4);
    CHECK(received[0] == 'p' && received[3] == 'g');

    *received_size = 0U;
    CHECK(aixos_user_mq_create(1U, 4U) >= 0);
    CHECK(aixos_user_disconnect(connection) == AIXOS_OK);
    CHECK(aixos_user_disconnect(connection) == AIXOS_ERR_PERM);
    CHECK(aixos_user_channel_destroy(channel) == AIXOS_OK);
    CHECK(aixos_user_channel_destroy(channel) == AIXOS_ERR_PERM);

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(user) == AIXOS_OK);
}

void test_api_parameter_matrix(void)
{
    aixos_handle_t invalid_handles[] = {
        AIXOS_HANDLE_INVALID,
        0,
        AIXOS_HANDLE_MAKE(250U, 1U),
        AIXOS_HANDLE_MAKE(1U, 0x00FFFFFEU),
    };
    uint8_t buffer[16] = { 0U };
    uint8_t small[2] = { 0U };
    size_t size = 0U;
    uint32_t matched = 0U;
    uint32_t priority = 0U;
    aixos_mq_info_t mq_info;
    aixos_mempool_t pool;
    uintptr_t pool_storage[8];
    aixos_handle_t current_task;

    path_reset_kernel();
    current_task = aixos_task_create("api", path_dummy_task, NULL, 256U, 2);
    CHECK(current_task != AIXOS_HANDLE_INVALID);
    aixos_test_set_current(aixos_tcb_from_handle(current_task));

    for (size_t i = 0U; i < sizeof(invalid_handles) / sizeof(invalid_handles[0]); i++) {
        aixos_handle_t h = invalid_handles[i];
        CHECK(aixos_sem_wait(h, 0U) == AIXOS_ERR_INVAL);
        CHECK(aixos_sem_post(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_sem_delete(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_sem_get_count(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_mutex_lock(h, 0U) == AIXOS_ERR_INVAL);
        CHECK(aixos_mutex_unlock(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_mutex_delete(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_mq_send(h, buffer, 1U, 0U) == AIXOS_ERR_INVAL);
        CHECK(aixos_mq_recv(h, buffer, sizeof(buffer), &size, 0U) ==
              AIXOS_ERR_INVAL);
        CHECK(aixos_mq_get_info(h, &mq_info) == AIXOS_ERR_INVAL);
        CHECK(aixos_mq_delete(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_event_set(h, 1U) == AIXOS_ERR_INVAL);
        CHECK(aixos_event_clear(h, 1U) == AIXOS_ERR_INVAL);
        CHECK(aixos_event_wait(h, 1U, AIXOS_EVENT_OR, 0U, &matched) ==
              AIXOS_ERR_INVAL);
        CHECK(aixos_event_delete(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_pipe_write(h, buffer, 1U, 0U) == AIXOS_ERR_INVAL);
        CHECK(aixos_pipe_read(h, buffer, 1U, 0U) == AIXOS_ERR_INVAL);
        CHECK(aixos_pipe_delete(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_timer_start(h, 1U) == AIXOS_ERR_INVAL);
        CHECK(aixos_timer_stop(h) == AIXOS_ERR_INVAL);
        CHECK(aixos_timer_delete(h) == AIXOS_ERR_INVAL);
    }

    CHECK(aixos_mq_create(0U, 1U) == AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_create(1U, 0U) == AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_create(1U, AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_create_static(1U, 1U, NULL, &size) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_create_static(1U, 1U, buffer, NULL) ==
          AIXOS_HANDLE_INVALID);

    aixos_handle_t mq = aixos_mq_create(2U, 4U);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    CHECK(aixos_mq_send(mq, NULL, 1U, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_mq_send(mq, buffer, 5U, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_mq_send_priority(mq, buffer, 1U,
                                 AIXOS_CFG_MQ_PRIORITY_MAX + 1U, 0U) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_mq_send(mq, buffer, 4U, 0U) == AIXOS_OK);
    CHECK(aixos_mq_recv_priority(mq, small, sizeof(small), &size, &priority,
                                 0U) == AIXOS_ERR_OVERFLOW);
    CHECK(size == 4U);
    CHECK(aixos_mq_recv_priority(mq, buffer, sizeof(buffer), &size, &priority,
                                 0U) == AIXOS_OK);
    CHECK(priority == 0U);
    CHECK(aixos_mq_recv(mq, buffer, sizeof(buffer), &size, 0U) ==
          AIXOS_ERR_AGAIN);
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);

    CHECK(aixos_pipe_create(0U) == AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_create_static(NULL, 4U) == AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_create_static(buffer, 0U) == AIXOS_HANDLE_INVALID);
    aixos_handle_t pipe = aixos_pipe_create(4U);
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    CHECK(aixos_pipe_write(pipe, NULL, 1U, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_write(pipe, buffer, AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U,
                           0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_read(pipe, NULL, 1U, 0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_read(pipe, buffer, AIXOS_CFG_MAX_IPC_COPY_BYTES + 1U,
                          0U) == AIXOS_ERR_INVAL);
    CHECK(aixos_pipe_write(pipe, buffer, 4U, 0U) == 4);
    CHECK(aixos_pipe_write(pipe, buffer, 1U, 0U) == 0);
    CHECK(aixos_pipe_read(pipe, buffer, sizeof(buffer), 0U) == 4);
    CHECK(aixos_pipe_read(pipe, buffer, sizeof(buffer), 0U) == 0);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);

    CHECK(aixos_event_wait(AIXOS_HANDLE_INVALID, 0U, AIXOS_EVENT_OR, 0U,
                           &matched) == AIXOS_ERR_INVAL);
    CHECK(aixos_event_wait(AIXOS_HANDLE_INVALID, 1U, 0U, 0U, &matched) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_event_wait(AIXOS_HANDLE_INVALID, 1U,
                           AIXOS_EVENT_OR | AIXOS_EVENT_AND, 0U,
                           &matched) == AIXOS_ERR_INVAL);
    CHECK(aixos_event_wait(AIXOS_HANDLE_INVALID, 1U, AIXOS_EVENT_OR, 0U,
                           NULL) == AIXOS_ERR_INVAL);

    CHECK(aixos_timer_create("bad", AIXOS_TIMER_ONESHOT, NULL, NULL) ==
          AIXOS_HANDLE_INVALID);
    CHECK(aixos_timer_create("bad", (aixos_timer_type_t)99,
                             path_dummy_task, NULL) == AIXOS_HANDLE_INVALID);

    CHECK(aixos_mempool_init(NULL, pool_storage, sizeof(pool_storage), 8U,
                             2U) == AIXOS_ERR_INVAL);
    CHECK(aixos_mempool_init(&pool, NULL, sizeof(pool_storage), 8U, 2U) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_mempool_init(&pool, pool_storage, sizeof(pool_storage), 0U,
                             2U) == AIXOS_ERR_INVAL);
    CHECK(aixos_mempool_init(&pool, (uint8_t *)pool_storage + 1U,
                             sizeof(pool_storage) - 1U, 8U, 2U) ==
          AIXOS_ERR_INVAL);
    CHECK(aixos_mempool_alloc(NULL) == NULL);
    CHECK(aixos_mempool_free(NULL, buffer) == AIXOS_ERR_INVAL);
    CHECK(aixos_mempool_owns(NULL, buffer) == 0);

    CHECK(aixos_mpu_region_valid(0U, 0U, AIXOS_MPU_READ) == 0);
    CHECK(aixos_mpu_region_valid(0U, AIXOS_CFG_MPU_MIN_REGION_SIZE,
                                 AIXOS_MPU_WRITE) == 0);
    CHECK(aixos_mpu_region_valid(1U, AIXOS_CFG_MPU_MIN_REGION_SIZE,
                                 AIXOS_MPU_READ) == 0);
    CHECK(aixos_mpu_region_valid((uintptr_t)buffer, 48U,
                                 AIXOS_MPU_READ) == 0);

    aixos_test_set_current(NULL);
    CHECK(aixos_task_delete(current_task) == AIXOS_OK);
}

void test_blackbox_user_workflows(void)
{
    aixos_handle_t sem;
    aixos_handle_t mq;
    aixos_handle_t event;
    aixos_handle_t pipe;
    aixos_handle_t notify_task;
    aixos_tcb_t *notify_tcb;
    uint8_t out[8];
    size_t size;
    uint32_t matched;
    uint32_t notify_value;

    path_reset_kernel();
    sem = aixos_sem_create(0);
    mq = aixos_mq_create(4U, sizeof(out));
    event = aixos_event_create();
    pipe = aixos_pipe_create(16U);
    notify_task = aixos_task_create("notify", path_dummy_task, NULL, 256U, 3);
    CHECK(sem != AIXOS_HANDLE_INVALID);
    CHECK(mq != AIXOS_HANDLE_INVALID);
    CHECK(event != AIXOS_HANDLE_INVALID);
    CHECK(pipe != AIXOS_HANDLE_INVALID);
    CHECK(notify_task != AIXOS_HANDLE_INVALID);
    notify_tcb = aixos_tcb_from_handle(notify_task);
    CHECK(notify_tcb != NULL);
    aixos_test_set_current(notify_tcb);

    for (uint32_t i = 0U; i < 256U; i++) {
        uint8_t payload[8] = {
            (uint8_t)i, (uint8_t)(i >> 1), (uint8_t)(i >> 2),
            (uint8_t)(0xA5U ^ i), 0U, 0U, 0U, 0U
        };
        uint32_t bit = UINT32_C(1) << (i % 16U);

        CHECK(aixos_sem_post(sem) == AIXOS_OK);
        CHECK(aixos_sem_wait(sem, 0U) == AIXOS_OK);
        CHECK(aixos_mq_send(mq, payload, sizeof(payload), 0U) == AIXOS_OK);
        memset(out, 0, sizeof(out));
        size = 0U;
        CHECK(aixos_mq_recv(mq, out, sizeof(out), &size, 0U) == AIXOS_OK);
        CHECK(size == sizeof(payload));
        CHECK(out[0] == payload[0] && out[3] == payload[3]);
        CHECK(aixos_event_set(event, bit) == AIXOS_OK);
        matched = 0U;
        CHECK(aixos_event_wait(event, bit, AIXOS_EVENT_AND | AIXOS_EVENT_CLEAR,
                               0U, &matched) == AIXOS_OK);
        CHECK(matched == bit);
        CHECK(aixos_pipe_write(pipe, payload, 3U, 0U) == 3);
        memset(out, 0, sizeof(out));
        CHECK(aixos_pipe_read(pipe, out, sizeof(out), 0U) == 3);
        CHECK(out[0] == payload[0] && out[2] == payload[2]);
        CHECK(aixos_task_notify(notify_task, i, AIXOS_NOTIFY_OVERWRITE) ==
              AIXOS_OK);
        notify_value = 0U;
        CHECK(aixos_task_notify_wait(0U, UINT32_MAX, &notify_value, 0U) ==
              AIXOS_OK);
        CHECK(notify_value == i);
    }

    aixos_test_set_current(NULL);
    CHECK(aixos_pipe_delete(pipe) == AIXOS_OK);
    CHECK(aixos_event_delete(event) == AIXOS_OK);
    CHECK(aixos_mq_delete(mq) == AIXOS_OK);
    CHECK(aixos_sem_delete(sem) == AIXOS_OK);
    CHECK(aixos_task_delete(notify_task) == AIXOS_OK);
}
