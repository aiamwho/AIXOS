#ifndef AIXOS_MICROKERNEL_H
#define AIXOS_MICROKERNEL_H

#include "aixos/types.h"

typedef int32_t aixos_cap_t;
struct aixos_tcb;

#define AIXOS_CAP_INVALID ((aixos_cap_t)-1)

enum {
    AIXOS_CAP_READ    = 1U << 0,
    AIXOS_CAP_WRITE   = 1U << 1,
    AIXOS_CAP_WAIT    = 1U << 2,
    AIXOS_CAP_SIGNAL  = 1U << 3,
    AIXOS_CAP_CONTROL = 1U << 4,
};

typedef enum {
    AIXOS_SC_YIELD = 0,
    AIXOS_SC_SLEEP,
    AIXOS_SC_TASK_SELF,
    AIXOS_SC_TASK_EXIT,
    AIXOS_SC_CLOCK_GET,
    AIXOS_SC_SEM_CREATE,
    AIXOS_SC_SEM_WAIT,
    AIXOS_SC_SEM_POST,
    AIXOS_SC_SEM_DELETE,
    AIXOS_SC_MQ_CREATE,
    AIXOS_SC_MQ_SEND,
    AIXOS_SC_MQ_RECV,
    AIXOS_SC_MQ_DELETE,
    AIXOS_SC_CAP_CLOSE,
    /* Synchronous message IPC */
    AIXOS_SC_CHANNEL_CREATE,
    AIXOS_SC_CHANNEL_DESTROY,
    AIXOS_SC_CONNECT,
    AIXOS_SC_DISCONNECT,
    AIXOS_SC_MSG_SEND,
    AIXOS_SC_MSG_RECEIVE,
    AIXOS_SC_MSG_REPLY,
    AIXOS_SC_COUNT
} aixos_syscall_number_t;

typedef struct {
    uint32_t number;
    uintptr_t args[5];
} aixos_syscall_request_t;

aixos_handle_t aixos_user_task_create_static(
    const char *name,
    void (*entry)(void *),
    void *arg,
    void *stack,
    size_t stack_size,
    int priority,
    struct aixos_tcb *tcb
);

int aixos_task_is_user(aixos_handle_t task);
int aixos_user_memory_check(const void *address, size_t size, int write_access);
int aixos_copy_from_user(void *kernel_dst, const void *user_src, size_t size);
int aixos_copy_to_user(void *user_dst, const void *kernel_src, size_t size);
int aixos_zero_to_user(void *user_dst, size_t size);
int32_t aixos_syscall_dispatch(const aixos_syscall_request_t *request);
int32_t aixos_syscall_invoke(const aixos_syscall_request_t *request);
#ifdef AIXOS_HOST_TEST
int32_t aixos_test_syscall_invoke_fast(uint32_t number, uintptr_t a0,
                                       uintptr_t a1, uintptr_t a2,
                                       uintptr_t a3, uintptr_t a4);
#endif

int aixos_user_yield(void);
int aixos_user_sleep(uint32_t milliseconds);
aixos_handle_t aixos_user_task_self(void);
int aixos_user_task_exit(void);
uint32_t aixos_user_clock_get(void);
aixos_cap_t aixos_user_sem_create(int initial_count);
int aixos_user_sem_wait(aixos_cap_t sem, uint32_t timeout_ms);
int aixos_user_sem_post(aixos_cap_t sem);
int aixos_user_sem_delete(aixos_cap_t sem);
aixos_cap_t aixos_user_mq_create(size_t max_messages, size_t message_size);
int aixos_user_mq_send(aixos_cap_t mq, const void *message, size_t size,
                       uint32_t timeout_ms);
int aixos_user_mq_recv(aixos_cap_t mq, void *buffer, size_t capacity,
                       size_t *received, uint32_t timeout_ms);
int aixos_user_mq_delete(aixos_cap_t mq);
int aixos_user_cap_close(aixos_cap_t cap);

/* Capability grant interface used by namespace services. */
int cap_grant_direct(struct aixos_tcb *tcb, aixos_handle_t object,
                     aixos_obj_type_t type, uint16_t rights);

/* ── Synchronous Message IPC ──────────────────────────────────────── */

#define AIXOS_CHANNEL_INVALID    ((aixos_cap_t)-1)
#define AIXOS_CHANNEL_DEFAULT     0
#define AIXOS_SYNC_MSG_MAX_SIZE   256U

aixos_cap_t aixos_channel_create(int flags);
int aixos_channel_destroy(aixos_cap_t ch);
aixos_cap_t aixos_connect(aixos_cap_t channel);
int aixos_disconnect(aixos_cap_t conn);
int aixos_msg_send(aixos_cap_t conn, const void *msg, size_t msg_size,
                   void *reply, size_t reply_capacity);
int aixos_msg_receive(aixos_cap_t ch, void *msg, size_t capacity,
                      int flags, aixos_cap_t *sender_conn);
int aixos_msg_reply(aixos_cap_t conn, const void *reply, size_t reply_size);

/* User-mode wrappers for sync message IPC */
int aixos_user_channel_create(int flags);
int aixos_user_channel_destroy(aixos_cap_t ch);
aixos_cap_t aixos_user_connect(aixos_cap_t channel);
int aixos_user_disconnect(aixos_cap_t conn);
int aixos_user_msg_send(aixos_cap_t conn, const void *msg, size_t msg_size,
                        void *reply, size_t reply_capacity);
int aixos_user_msg_receive(aixos_cap_t ch, void *msg, size_t capacity,
                           int flags);
int aixos_user_msg_reply(aixos_cap_t conn, const void *reply,
                         size_t reply_size);

#endif
