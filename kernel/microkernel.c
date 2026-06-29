#include "aixos/microkernel.h"
#include "aixos/task.h"
#include "aixos/sem.h"
#include "aixos/mq.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "kernel/list.h"
#include "config/string.h"
#include <stdint.h>

static int cap_grant(aixos_tcb_t *tcb, aixos_handle_t object,
                     aixos_obj_type_t type, uint16_t rights)
{
    int index;
    for (index = 0; index < AIXOS_CFG_CAPS_PER_TASK; index++) {
        if (tcb->capabilities[index].used == 0U) {
            tcb->capabilities[index].object = object;
            tcb->capabilities[index].type = (uint8_t)type;
            tcb->capabilities[index].rights = rights;
            tcb->capabilities[index].used = 1U;
            return index;
        }
    }
    return AIXOS_ERR_NOMEM;
}

/* Capability grant interface used by namespace services. */
int cap_grant_direct(aixos_tcb_t *tcb, aixos_handle_t object,
                     aixos_obj_type_t type, uint16_t rights)
{
    return cap_grant(tcb, object, type, rights);
}

static int cap_resolve(aixos_tcb_t *tcb, aixos_cap_t cap,
                       aixos_obj_type_t type, uint16_t rights,
                       aixos_handle_t *object)
{
    if (tcb == NULL || cap < 0 || cap >= AIXOS_CFG_CAPS_PER_TASK ||
        tcb->capabilities[cap].used == 0U ||
        tcb->capabilities[cap].type != (uint8_t)type ||
        (tcb->capabilities[cap].rights & rights) != rights) {
        return AIXOS_ERR_PERM;
    }
    *object = tcb->capabilities[cap].object;
    return AIXOS_OK;
}

static int cap_close(aixos_tcb_t *tcb, aixos_cap_t cap)
{
    if (tcb == NULL || cap < 0 || cap >= AIXOS_CFG_CAPS_PER_TASK ||
        tcb->capabilities[cap].used == 0U) {
        return AIXOS_ERR_PERM;
    }
    tcb->capabilities[cap].used = 0U;
    tcb->capabilities[cap].object = AIXOS_HANDLE_INVALID;
    tcb->capabilities[cap].rights = 0U;
    tcb->capabilities[cap].type = AIXOS_OBJ_UNUSED;
    return AIXOS_OK;
}

/* ── Sync Message IPC Tables ─────────────────────────────────── */
#define AIXOS_MAX_CHANNELS     8
#define AIXOS_MAX_CONNECTIONS  16
#define AIXOS_MAX_PENDING_MSGS 12
#define AIXOS_MAX_SERVER_REPLY AIXOS_SYNC_MSG_MAX_SIZE

typedef struct {
    uint8_t used;
    uint8_t flags;
    aixos_handle_t owner;
    aixos_list_t connections;   /* list of connection nodes */
    aixos_list_t pending_msgs;  /* pending message nodes for this channel */
    /* receiver waiting on this channel (NULL if none) */
    aixos_tcb_t *receiver;
} channel_t;

typedef struct {
    uint8_t used;
    aixos_cap_t ch_cap;         /* channel capability */
    uint8_t ch_idx;             /* channel index */
    aixos_handle_t owner;
    aixos_list_t node;          /* node in channel's connection list */
} connection_t;

typedef struct {
    uint8_t used;
    uint8_t msg_buf[AIXOS_SYNC_MSG_MAX_SIZE];
    size_t msg_size;
    uint8_t reply_buf[AIXOS_MAX_SERVER_REPLY];
    size_t reply_size;
    size_t reply_capacity;
    uint16_t sender_conn_idx;   /* connection index of sender */
    aixos_tcb_t *sender;        /* blocked sender task */
    void *reply_user_ptr;       /* Sender reply buffer address in user space. */
    aixos_list_t node;          /* node in channel's pending_msgs list */
} pending_msg_t;

static channel_t channels[AIXOS_MAX_CHANNELS];
static connection_t connections[AIXOS_MAX_CONNECTIONS];
static pending_msg_t pending_msgs[AIXOS_MAX_PENDING_MSGS];

static int channel_alloc(aixos_handle_t owner)
{
    int i;
    for (i = 0; i < AIXOS_MAX_CHANNELS; i++) {
        if (channels[i].used == 0U) {
            channels[i].used = 1;
            channels[i].flags = 0;
            channels[i].owner = owner;
            aixos_list_init(&channels[i].connections);
            aixos_list_init(&channels[i].pending_msgs);
            channels[i].receiver = NULL;
            return i;
        }
    }
    return -1;
}

static int connection_alloc(aixos_handle_t owner)
{
    int i;
    for (i = 0; i < AIXOS_MAX_CONNECTIONS; i++) {
        if (connections[i].used == 0U) {
            connections[i].used = 1;
            connections[i].owner = owner;
            return i;
        }
    }
    return -1;
}

static pending_msg_t *pending_msg_alloc(void)
{
    int i;
    for (i = 0; i < AIXOS_MAX_PENDING_MSGS; i++) {
        if (pending_msgs[i].used == 0U) {
            pending_msgs[i].used = 1;
            pending_msgs[i].sender = NULL;
            pending_msgs[i].msg_size = 0;
            pending_msgs[i].reply_size = 0;
            return &pending_msgs[i];
        }
    }
    return NULL;
}

/* ── Channel Operations ──────────────────────────────────────── */
aixos_cap_t aixos_channel_create(int flags)
{
    /* This is a kernel-only API. User tasks go through syscall. */
    (void)flags;
    return AIXOS_CAP_INVALID;
}

int aixos_channel_destroy(aixos_cap_t ch)
{
    (void)ch;
    return AIXOS_ERR_NOSYS;
}

aixos_cap_t aixos_connect(aixos_cap_t channel)
{
    (void)channel;
    return AIXOS_CAP_INVALID;
}

int aixos_disconnect(aixos_cap_t conn)
{
    (void)conn;
    return AIXOS_ERR_NOSYS;
}

int aixos_msg_send(aixos_cap_t conn, const void *msg, size_t msg_size,
                   void *reply, size_t reply_capacity)
{
    /* kernel-to-kernel send - not yet implemented for kernel tasks */
    (void)conn; (void)msg; (void)msg_size; (void)reply; (void)reply_capacity;
    return AIXOS_ERR_NOSYS;
}

int aixos_msg_receive(aixos_cap_t ch, void *msg, size_t capacity,
                      int flags, aixos_cap_t *sender_conn)
{
    /* kernel-to-kernel receive - not yet implemented for kernel tasks */
    (void)ch; (void)msg; (void)capacity; (void)flags; (void)sender_conn;
    return AIXOS_ERR_NOSYS;
}

int aixos_msg_reply(aixos_cap_t conn, const void *reply, size_t reply_size)
{
    (void)conn; (void)reply; (void)reply_size;
    return AIXOS_ERR_NOSYS;
}

static int user_memory_check_for_task(const aixos_tcb_t *tcb,
                                      const void *address, size_t size,
                                      int write_access)
{
    uintptr_t start;
    if (tcb == NULL || tcb->domain != AIXOS_DOMAIN_USER || address == NULL) {
        return AIXOS_ERR_FAULT;
    }
    start = (uintptr_t)address;
    if (size == 0U || size > UINTPTR_MAX - start) {
        return AIXOS_ERR_FAULT;
    }
    if (!aixos_mpu_task_allows(tcb, start, size, write_access, 0)) {
        return AIXOS_ERR_FAULT;
    }
    return AIXOS_OK;
}

int aixos_user_memory_check(const void *address, size_t size, int write_access)
{
    return user_memory_check_for_task(g_cur_task, address, size, write_access);
}

int aixos_copy_from_user(void *kernel_dst, const void *user_src, size_t size)
{
    int result;
    if (size == 0U) {
        return AIXOS_OK;
    }
    if (kernel_dst == NULL) {
        return AIXOS_ERR_FAULT;
    }
    result = aixos_user_memory_check(user_src, size, 0);
    if (result != AIXOS_OK) {
        return result;
    }
    memcpy(kernel_dst, user_src, size);
    return AIXOS_OK;
}

static int copy_to_user_task(const aixos_tcb_t *task, void *user_dst,
                             const void *kernel_src, size_t size)
{
    int result;
    if (size == 0U) {
        return AIXOS_OK;
    }
    if (kernel_src == NULL) {
        return AIXOS_ERR_FAULT;
    }
    result = user_memory_check_for_task(task, user_dst, size, 1);
    if (result != AIXOS_OK) {
        return result;
    }
    memcpy(user_dst, kernel_src, size);
    return AIXOS_OK;
}

int aixos_copy_to_user(void *user_dst, const void *kernel_src, size_t size)
{
    return copy_to_user_task(g_cur_task, user_dst, kernel_src, size);
}

int aixos_zero_to_user(void *user_dst, size_t size)
{
    int result;
    if (size == 0U) {
        return AIXOS_OK;
    }
    result = aixos_user_memory_check(user_dst, size, 1);
    if (result != AIXOS_OK) {
        return result;
    }
    memset(user_dst, 0, size);
    return AIXOS_OK;
}

static int request_check(const aixos_syscall_request_t *request)
{
#ifdef AIXOS_HOST_TEST
    return request != NULL ? AIXOS_OK : AIXOS_ERR_FAULT;
#else
    return aixos_user_memory_check(request, sizeof(*request), 0);
#endif
}

/* System call dispatch table. */
typedef int32_t (*aixos_syscall_handler_t)(aixos_tcb_t *caller, const aixos_syscall_request_t *request);

/* Forward declarations for all handler functions */
static int32_t sc_yield(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_sleep(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_task_self(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_task_exit(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_clock_get(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_sem_create(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_sem_wait(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_sem_post(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_sem_delete(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_mq_create(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_mq_send(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_mq_recv(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_mq_delete(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_cap_close(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_channel_create(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_channel_destroy(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_connect(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_disconnect(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_msg_send(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_msg_receive(aixos_tcb_t *caller, const aixos_syscall_request_t *req);
static int32_t sc_msg_reply(aixos_tcb_t *caller, const aixos_syscall_request_t *req);

static const aixos_syscall_handler_t syscall_table[AIXOS_SC_COUNT] = {
    [AIXOS_SC_YIELD]          = sc_yield,
    [AIXOS_SC_SLEEP]          = sc_sleep,
    [AIXOS_SC_TASK_SELF]      = sc_task_self,
    [AIXOS_SC_TASK_EXIT]      = sc_task_exit,
    [AIXOS_SC_CLOCK_GET]      = sc_clock_get,
    [AIXOS_SC_SEM_CREATE]     = sc_sem_create,
    [AIXOS_SC_SEM_WAIT]       = sc_sem_wait,
    [AIXOS_SC_SEM_POST]       = sc_sem_post,
    [AIXOS_SC_SEM_DELETE]     = sc_sem_delete,
    [AIXOS_SC_MQ_CREATE]      = sc_mq_create,
    [AIXOS_SC_MQ_SEND]        = sc_mq_send,
    [AIXOS_SC_MQ_RECV]        = sc_mq_recv,
    [AIXOS_SC_MQ_DELETE]      = sc_mq_delete,
    [AIXOS_SC_CAP_CLOSE]      = sc_cap_close,
    [AIXOS_SC_CHANNEL_CREATE] = sc_channel_create,
    [AIXOS_SC_CHANNEL_DESTROY]= sc_channel_destroy,
    [AIXOS_SC_CONNECT]        = sc_connect,
    [AIXOS_SC_DISCONNECT]     = sc_disconnect,
    [AIXOS_SC_MSG_SEND]       = sc_msg_send,
    [AIXOS_SC_MSG_RECEIVE]    = sc_msg_receive,
    [AIXOS_SC_MSG_REPLY]      = sc_msg_reply,
};

int32_t aixos_syscall_dispatch(const aixos_syscall_request_t *request)
{
    aixos_tcb_t *caller = g_cur_task;

    if (caller == NULL || caller->domain != AIXOS_DOMAIN_USER) {
        return AIXOS_ERR_PERM;
    }
    if (request_check(request) != AIXOS_OK ||
        request->number >= AIXOS_SC_COUNT) {
        return AIXOS_ERR_FAULT;
    }
    /* Dispatch through the system call table. */
    aixos_syscall_handler_t handler = syscall_table[request->number];
    if (handler != NULL) {
        return handler(caller, request);
    }
    return AIXOS_ERR_NOSYS;
}
/* ================================================================= */
/*  System call handler function implementations                      */
/* ================================================================= */

static int32_t sc_yield(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    (void)caller; (void)req;
    return aixos_task_yield();
}

static int32_t sc_sleep(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    (void)caller;
    return aixos_task_sleep((uint32_t)req->args[0]);
}

static int32_t sc_task_self(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    (void)req;
    return caller->handle;
}

static int32_t sc_task_exit(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    (void)req;
    return aixos_task_delete(caller->handle);
}

static int32_t sc_clock_get(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    (void)caller; (void)req;
    return (int32_t)aixos_get_tick();
}

static int32_t sc_sem_create(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t created = aixos_sem_create((int)req->args[0]);
    if (created == AIXOS_HANDLE_INVALID) {
        return AIXOS_ERR_NOMEM;
    }
    aixos_cap_t cap = cap_grant(caller, created, AIXOS_OBJ_SEM,
                                AIXOS_CAP_WAIT | AIXOS_CAP_SIGNAL |
                                AIXOS_CAP_CONTROL);
    if (cap < 0) {
        (void)aixos_sem_delete(created);
    }
    return cap;
}

static int32_t sc_sem_wait(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t object;
    int result = cap_resolve(caller, (aixos_cap_t)req->args[0],
                             AIXOS_OBJ_SEM, AIXOS_CAP_WAIT, &object);
    return result == AIXOS_OK ?
        aixos_sem_wait(object, (uint32_t)req->args[1]) : result;
}

static int32_t sc_sem_post(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t object;
    int result = cap_resolve(caller, (aixos_cap_t)req->args[0],
                             AIXOS_OBJ_SEM, AIXOS_CAP_SIGNAL, &object);
    return result == AIXOS_OK ? aixos_sem_post(object) : result;
}

static int32_t sc_sem_delete(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t object;
    aixos_cap_t cap = (aixos_cap_t)req->args[0];
    int result = cap_resolve(caller, cap, AIXOS_OBJ_SEM,
                             AIXOS_CAP_CONTROL, &object);
    if (result != AIXOS_OK) {
        return result;
    }
    result = aixos_sem_delete(object);
    if (result == AIXOS_OK) {
        (void)cap_close(caller, cap);
    }
    return result;
}

static int32_t sc_mq_create(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t created = aixos_mq_create((size_t)req->args[0],
                                              (size_t)req->args[1]);
    if (created == AIXOS_HANDLE_INVALID) {
        return AIXOS_ERR_NOMEM;
    }
    aixos_cap_t cap = cap_grant(caller, created, AIXOS_OBJ_MQ,
                                AIXOS_CAP_READ | AIXOS_CAP_WRITE |
                                AIXOS_CAP_CONTROL);
    if (cap < 0) {
        (void)aixos_mq_delete(created);
    }
    return cap;
}

static int32_t sc_mq_send(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t object;
    unsigned char message[AIXOS_CFG_MAX_IPC_COPY_BYTES];
    size_t size = (size_t)req->args[2];
    int result = cap_resolve(caller, (aixos_cap_t)req->args[0],
                             AIXOS_OBJ_MQ, AIXOS_CAP_WRITE, &object);
    if (result != AIXOS_OK) {
        return result;
    }
    if (size == 0U || size > AIXOS_CFG_MAX_IPC_COPY_BYTES) {
        return AIXOS_ERR_INVAL;
    }
    result = aixos_copy_from_user(message, (const void *)req->args[1], size);
    return result == AIXOS_OK ?
        aixos_mq_send(object, message, size, (uint32_t)req->args[3]) :
        result;
}

static int32_t sc_mq_recv(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t object;
    unsigned char message[AIXOS_CFG_MAX_IPC_COPY_BYTES];
    size_t received;
    size_t capacity = (size_t)req->args[2];
    size_t kernel_capacity;
    int result = cap_resolve(caller, (aixos_cap_t)req->args[0],
                             AIXOS_OBJ_MQ, AIXOS_CAP_READ, &object);
    if (result != AIXOS_OK) {
        return result;
    }
    result = aixos_user_memory_check((void *)req->args[1], capacity, 1);
    if (result != AIXOS_OK) {
        return result;
    }
    kernel_capacity = capacity > AIXOS_CFG_MAX_IPC_COPY_BYTES ?
                      AIXOS_CFG_MAX_IPC_COPY_BYTES : capacity;
    received = 0U;
    result = aixos_mq_recv(object, message, kernel_capacity, &received,
                           (uint32_t)req->args[3]);
    if (result == AIXOS_OK) {
        result = aixos_copy_to_user((void *)req->args[1], message, received);
        if (result != AIXOS_OK) {
            return result;
        }
    }
    if (result == AIXOS_OK && req->args[4] != 0U) {
        result = aixos_copy_to_user((void *)req->args[4], &received,
                                    sizeof(received));
        if (result != AIXOS_OK) {
            return result;
        }
    }
    return result;
}

static int32_t sc_mq_delete(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t object;
    aixos_cap_t cap = (aixos_cap_t)req->args[0];
    int result = cap_resolve(caller, cap, AIXOS_OBJ_MQ,
                             AIXOS_CAP_CONTROL, &object);
    if (result != AIXOS_OK) {
        return result;
    }
    result = aixos_mq_delete(object);
    if (result == AIXOS_OK) {
        (void)cap_close(caller, cap);
    }
    return result;
}

static int32_t sc_cap_close(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    return cap_close(caller, (aixos_cap_t)req->args[0]);
}

static int32_t sc_channel_create(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    (void)req;
    int ch_idx = channel_alloc(caller->handle);
    if (ch_idx < 0) return AIXOS_ERR_NOMEM;
    aixos_cap_t cap = cap_grant(caller, (aixos_handle_t)(uintptr_t)ch_idx,
                    AIXOS_OBJ_CHANNEL, AIXOS_CAP_READ | AIXOS_CAP_WRITE | AIXOS_CAP_CONTROL);
    if (cap < 0) { channels[ch_idx].used = 0; return AIXOS_ERR_NOMEM; }
    return cap;
}

static int32_t sc_channel_destroy(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t obj;
    aixos_cap_t cap = (aixos_cap_t)req->args[0];
    int result = cap_resolve(caller, cap, AIXOS_OBJ_CHANNEL, AIXOS_CAP_CONTROL, &obj);
    if (result != AIXOS_OK) return result;
    int idx = (int)(uintptr_t)obj;
    if (idx >= 0 && idx < AIXOS_MAX_CHANNELS && channels[idx].used) {
        channels[idx].used = 0;
        (void)cap_close(caller, cap);
        return AIXOS_OK;
    }
    return AIXOS_ERR_INVAL;
}

static int32_t sc_connect(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t obj;
    int result = cap_resolve(caller, (aixos_cap_t)req->args[0],
                             AIXOS_OBJ_CHANNEL, AIXOS_CAP_READ, &obj);
    if (result != AIXOS_OK) return result;
    int ch_idx = (int)(uintptr_t)obj;
    if (ch_idx < 0 || ch_idx >= AIXOS_MAX_CHANNELS || !channels[ch_idx].used)
        return AIXOS_ERR_INVAL;
    int conn_idx = connection_alloc(caller->handle);
    if (conn_idx < 0) return AIXOS_ERR_NOMEM;
    connections[conn_idx].ch_cap = (aixos_cap_t)req->args[0];
    connections[conn_idx].ch_idx = (uint8_t)ch_idx;
    aixos_list_add_tail(&connections[conn_idx].node, &channels[ch_idx].connections);
    aixos_cap_t cap = cap_grant(caller, (aixos_handle_t)(uintptr_t)conn_idx,
                    AIXOS_OBJ_CONNECTION, AIXOS_CAP_WRITE | AIXOS_CAP_SIGNAL | AIXOS_CAP_CONTROL);
    if (cap < 0) {
        aixos_list_del(&connections[conn_idx].node);
        connections[conn_idx].used = 0;
        return AIXOS_ERR_NOMEM;
    }
    return cap;
}

static int32_t sc_disconnect(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t obj;
    aixos_cap_t cap = (aixos_cap_t)req->args[0];
    int result = cap_resolve(caller, cap, AIXOS_OBJ_CONNECTION, AIXOS_CAP_CONTROL, &obj);
    if (result != AIXOS_OK) return result;
    int conn_idx = (int)(uintptr_t)obj;
    if (conn_idx >= 0 && conn_idx < AIXOS_MAX_CONNECTIONS && connections[conn_idx].used) {
        aixos_list_del(&connections[conn_idx].node);
        connections[conn_idx].used = 0;
        (void)cap_close(caller, cap);
        return AIXOS_OK;
    }
    return AIXOS_ERR_INVAL;
}

static int32_t sc_msg_send(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t conn_obj;
    aixos_cap_t cap = (aixos_cap_t)req->args[0];
    int result = cap_resolve(caller, cap, AIXOS_OBJ_CONNECTION, AIXOS_CAP_WRITE, &conn_obj);
    if (result != AIXOS_OK) return result;
    int conn_idx = (int)(uintptr_t)conn_obj;
    if (conn_idx < 0 || conn_idx >= AIXOS_MAX_CONNECTIONS || !connections[conn_idx].used)
        return AIXOS_ERR_INVAL;
    size_t msz = (size_t)req->args[2];
    if (msz == 0 || msz > AIXOS_SYNC_MSG_MAX_SIZE) return AIXOS_ERR_INVAL;
    pending_msg_t *pm = pending_msg_alloc();
    if (pm == NULL) return AIXOS_ERR_NOMEM;
    result = aixos_copy_from_user(pm->msg_buf, (const void *)req->args[1],
                                  msz);
    if (result != AIXOS_OK) { pm->used = 0; return result; }
    pm->msg_size = msz;
    pm->reply_capacity = (size_t)req->args[4];
    if (pm->reply_capacity > 0) {
        result = aixos_user_memory_check((void *)req->args[3], pm->reply_capacity, 1);
        if (result != AIXOS_OK) { pm->used = 0; return result; }
        pm->reply_user_ptr = (void *)req->args[3];
    } else {
        pm->reply_user_ptr = NULL;
    }
    pm->sender_conn_idx = (uint16_t)conn_idx;
    pm->sender = caller;
    int ch_idx = connections[conn_idx].ch_idx;
    aixos_list_add_tail(&pm->node, &channels[ch_idx].pending_msgs);
    aixos_arch_flags_t f = aixos_arch_int_disable();
    if (channels[ch_idx].receiver != NULL) {
        aixos_task_wake(channels[ch_idx].receiver, AIXOS_OK);
        channels[ch_idx].receiver = NULL;
    }
    caller->state = AIXOS_TASK_BLOCKED;
    caller->wait_obj = pm;
    caller->wait_list = NULL;
    caller->wait_type = AIXOS_OBJ_CONNECTION;
    caller->wait_result = AIXOS_OK;
    aixos_sched_remove_task(caller);
    aixos_reschedule_request();
    aixos_arch_int_restore(f);
    return caller->wait_result;
}

static int32_t sc_msg_receive(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t ch_obj;
    int result = cap_resolve(caller, (aixos_cap_t)req->args[0],
                             AIXOS_OBJ_CHANNEL, AIXOS_CAP_READ, &ch_obj);
    if (result != AIXOS_OK) return result;
    int ch_idx = (int)(uintptr_t)ch_obj;
    if (ch_idx < 0 || ch_idx >= AIXOS_MAX_CHANNELS || !channels[ch_idx].used)
        return AIXOS_ERR_INVAL;
    size_t capa = (size_t)req->args[2];
    if (capa == 0) return AIXOS_ERR_INVAL;
    result = aixos_user_memory_check((void *)req->args[1], capa, 1);
    if (result != AIXOS_OK) return result;
    pending_msg_t *pm = NULL;
    if (!aixos_list_is_empty(&channels[ch_idx].pending_msgs)) {
        aixos_list_t *first = aixos_list_first(&channels[ch_idx].pending_msgs);
        pm = AIXOS_CONTAINER_OF(first, pending_msg_t, node);
        aixos_list_del(&pm->node);
    }
    if (pm != NULL) {
        size_t copy = pm->msg_size < capa ? pm->msg_size : capa;
        result = aixos_copy_to_user((void *)req->args[1], pm->msg_buf,
                                    copy);
        if (result != AIXOS_OK) return result;
        return (int32_t)copy;
    }
    aixos_arch_flags_t f = aixos_arch_int_disable();
    channels[ch_idx].receiver = caller;
    caller->state = AIXOS_TASK_BLOCKED;
    caller->wait_obj = &channels[ch_idx];
    caller->wait_list = NULL;
    caller->wait_type = AIXOS_OBJ_CHANNEL;
    caller->wait_result = AIXOS_OK;
    aixos_sched_remove_task(caller);
    aixos_reschedule_request();
    aixos_arch_int_restore(f);
    if (!aixos_list_is_empty(&channels[ch_idx].pending_msgs)) {
        aixos_list_t *first = aixos_list_first(&channels[ch_idx].pending_msgs);
        pm = AIXOS_CONTAINER_OF(first, pending_msg_t, node);
        aixos_list_del(&pm->node);
        if (pm != NULL) {
            size_t copy = pm->msg_size < capa ? pm->msg_size : capa;
            result = aixos_copy_to_user((void *)req->args[1], pm->msg_buf,
                                        copy);
            if (result != AIXOS_OK) return result;
            return (int32_t)copy;
        }
    }
    return 0;
}

static int32_t sc_msg_reply(aixos_tcb_t *caller, const aixos_syscall_request_t *req)
{
    aixos_handle_t conn_obj;
    aixos_cap_t cap = (aixos_cap_t)req->args[0];
    unsigned char reply[AIXOS_MAX_SERVER_REPLY];
    int result = cap_resolve(caller, cap, AIXOS_OBJ_CONNECTION, AIXOS_CAP_SIGNAL, &conn_obj);
    if (result != AIXOS_OK) return result;
    int conn_idx = (int)(uintptr_t)conn_obj;
    if (conn_idx < 0 || conn_idx >= AIXOS_MAX_CONNECTIONS || !connections[conn_idx].used)
        return AIXOS_ERR_INVAL;
    size_t rsz = (size_t)req->args[2];
    if (rsz > AIXOS_MAX_SERVER_REPLY) return AIXOS_ERR_INVAL;
    if (rsz > 0) {
        result = aixos_copy_from_user(reply, (const void *)req->args[1], rsz);
        if (result != AIXOS_OK) return result;
    }
    int ch_idx = connections[conn_idx].ch_idx;
    pending_msg_t *pm = NULL;
    aixos_list_t *pos, *tmp;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    AIXOS_LIST_FOR_EACH_SAFE(pos, tmp, &channels[ch_idx].pending_msgs) {
        pending_msg_t *candidate = AIXOS_CONTAINER_OF(pos, pending_msg_t, node);
        if (candidate->sender_conn_idx == (uint16_t)conn_idx) {
            pm = candidate;
            aixos_list_del(&pm->node);
            break;
        }
    }
    if (pm == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_NOT_FOUND;
    }
    pm->reply_size = rsz;
    if (rsz > 0) {
        memcpy(pm->reply_buf, reply, rsz);
    }
    if (pm->reply_user_ptr != NULL && pm->reply_capacity > 0 && rsz > 0) {
        size_t copy = rsz < pm->reply_capacity ? rsz : pm->reply_capacity;
        result = copy_to_user_task(pm->sender, pm->reply_user_ptr,
                                   pm->reply_buf, copy);
        if (result != AIXOS_OK) {
            aixos_arch_int_restore(flags);
            return result;
        }
    }
    if (pm->sender != NULL) {
        pm->sender->wait_result = AIXOS_OK;
        aixos_task_wake(pm->sender, AIXOS_OK);
        pm->sender = NULL;
    }
    pm->used = 0;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int32_t aixos_syscall_invoke(const aixos_syscall_request_t *request)
{
#if defined(AIXOS_HOST_TEST)
    return aixos_syscall_dispatch(request);
#elif defined(__arm__) || defined(__thumb__)
    register const aixos_syscall_request_t *r0 __asm("r0") = request;
    __asm volatile("svc 0" : "+r"(r0) :: "memory");
    return (int32_t)(uintptr_t)r0;
#elif defined(__aarch64__)
    register const aixos_syscall_request_t *x0 __asm("x0") = request;
    __asm volatile("svc #0" : "+r"(x0) :: "memory");
    return (int32_t)(uintptr_t)x0;
#elif defined(__riscv)
    register const aixos_syscall_request_t *a0 __asm("a0") = request;
    __asm volatile("ecall" : "+r"(a0) :: "memory");
    return (int32_t)(uintptr_t)a0;
#else
    return AIXOS_ERR_NOSYS;
#endif
}

/* Fast system call path with direct register arguments. */
static inline int32_t __attribute__((unused)) aixos_syscall_invoke_fast(
    uint32_t number, uintptr_t a0, uintptr_t a1,
    uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
#if defined(AIXOS_HOST_TEST)
    aixos_syscall_request_t req = { number, { a0, a1, a2, a3, a4 } };
    return aixos_syscall_invoke(&req);
#elif defined(__riscv)
    register uintptr_t r_a0 __asm("a0") = (uintptr_t)number;
    register uintptr_t r_a1 __asm("a1") = a0;
    register uintptr_t r_a2 __asm("a2") = a1;
    register uintptr_t r_a3 __asm("a3") = a2;
    register uintptr_t r_a4 __asm("a4") = a3;
    register uintptr_t r_a5 __asm("a5") = a4;
    __asm volatile("ecall"
        : "+r"(r_a0)
        : "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a4), "r"(r_a5)
        : "memory");
    return (int32_t)r_a0;
#elif defined(__arm__) || defined(__thumb__)
    /* ARM uses r0-r3 for first 4 args, remaining on stack */
    register uintptr_t r0 __asm("r0") = (uintptr_t)number;
    register uintptr_t r1 __asm("r1") = a0;
    register uintptr_t r2 __asm("r2") = a1;
    register uintptr_t r3 __asm("r3") = a2;
    __asm volatile("svc 0"
        : "+r"(r0)
        : "r"(r1), "r"(r2), "r"(r3)
        : "memory");
    return (int32_t)r0;
#elif defined(__aarch64__)
    register uintptr_t x0 __asm("x0") = (uintptr_t)number;
    register uintptr_t x1 __asm("x1") = a0;
    register uintptr_t x2 __asm("x2") = a1;
    register uintptr_t x3 __asm("x3") = a2;
    register uintptr_t x4 __asm("x4") = a3;
    register uintptr_t x5 __asm("x5") = a4;
    __asm volatile("svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory");
    return (int32_t)x0;
#else
    aixos_syscall_request_t req = { number, { a0, a1, a2, a3, a4 } };
    return aixos_syscall_invoke(&req);
#endif
}

#ifdef AIXOS_HOST_TEST
int32_t aixos_test_syscall_invoke_fast(uint32_t number, uintptr_t a0,
                                       uintptr_t a1, uintptr_t a2,
                                       uintptr_t a3, uintptr_t a4)
{
    return aixos_syscall_invoke_fast(number, a0, a1, a2, a3, a4);
}
#endif

static int32_t invoke(uint32_t number, uintptr_t a0, uintptr_t a1,
                      uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
    aixos_syscall_request_t request = { number, { a0, a1, a2, a3, a4 } };
    return aixos_syscall_invoke(&request);
}

int aixos_user_yield(void) { return (int)invoke(AIXOS_SC_YIELD, 0, 0, 0, 0, 0); }
int aixos_user_sleep(uint32_t ms) { return (int)invoke(AIXOS_SC_SLEEP, ms, 0, 0, 0, 0); }
aixos_handle_t aixos_user_task_self(void) { return invoke(AIXOS_SC_TASK_SELF, 0, 0, 0, 0, 0); }
int aixos_user_task_exit(void) { return (int)invoke(AIXOS_SC_TASK_EXIT, 0, 0, 0, 0, 0); }
uint32_t aixos_user_clock_get(void) { return (uint32_t)invoke(AIXOS_SC_CLOCK_GET, 0, 0, 0, 0, 0); }
aixos_cap_t aixos_user_sem_create(int count) { return invoke(AIXOS_SC_SEM_CREATE, (uintptr_t)count, 0, 0, 0, 0); }
int aixos_user_sem_wait(aixos_cap_t cap, uint32_t timeout) { return (int)invoke(AIXOS_SC_SEM_WAIT, (uintptr_t)cap, timeout, 0, 0, 0); }
int aixos_user_sem_post(aixos_cap_t cap) { return (int)invoke(AIXOS_SC_SEM_POST, (uintptr_t)cap, 0, 0, 0, 0); }
int aixos_user_sem_delete(aixos_cap_t cap) { return (int)invoke(AIXOS_SC_SEM_DELETE, (uintptr_t)cap, 0, 0, 0, 0); }
aixos_cap_t aixos_user_mq_create(size_t count, size_t size) { return invoke(AIXOS_SC_MQ_CREATE, count, size, 0, 0, 0); }
int aixos_user_mq_send(aixos_cap_t cap, const void *message, size_t size, uint32_t timeout) { return (int)invoke(AIXOS_SC_MQ_SEND, (uintptr_t)cap, (uintptr_t)message, size, timeout, 0); }
int aixos_user_mq_recv(aixos_cap_t cap, void *buffer, size_t capacity, size_t *received, uint32_t timeout) { return (int)invoke(AIXOS_SC_MQ_RECV, (uintptr_t)cap, (uintptr_t)buffer, capacity, timeout, (uintptr_t)received); }
int aixos_user_mq_delete(aixos_cap_t cap) { return (int)invoke(AIXOS_SC_MQ_DELETE, (uintptr_t)cap, 0, 0, 0, 0); }
int aixos_user_cap_close(aixos_cap_t cap) { return (int)invoke(AIXOS_SC_CAP_CLOSE, (uintptr_t)cap, 0, 0, 0, 0); }

/* ── User-mode wrappers for sync message IPC ─────────────────── */
int aixos_user_channel_create(int flags) {
    return (int)invoke(AIXOS_SC_CHANNEL_CREATE, (uintptr_t)flags, 0, 0, 0, 0);
}
int aixos_user_channel_destroy(aixos_cap_t ch) {
    return (int)invoke(AIXOS_SC_CHANNEL_DESTROY, (uintptr_t)ch, 0, 0, 0, 0);
}
aixos_cap_t aixos_user_connect(aixos_cap_t channel) {
    return invoke(AIXOS_SC_CONNECT, (uintptr_t)channel, 0, 0, 0, 0);
}
int aixos_user_disconnect(aixos_cap_t conn) {
    return (int)invoke(AIXOS_SC_DISCONNECT, (uintptr_t)conn, 0, 0, 0, 0);
}
int aixos_user_msg_send(aixos_cap_t conn, const void *msg, size_t msg_size,
                        void *reply, size_t reply_capacity) {
    return (int)invoke(AIXOS_SC_MSG_SEND, (uintptr_t)conn, (uintptr_t)msg,
                       msg_size, (uintptr_t)reply, reply_capacity);
}
int aixos_user_msg_receive(aixos_cap_t ch, void *msg, size_t capacity,
                           int flags) {
    return (int)invoke(AIXOS_SC_MSG_RECEIVE, (uintptr_t)ch, (uintptr_t)msg,
                       capacity, (uintptr_t)flags, 0);
}
int aixos_user_msg_reply(aixos_cap_t conn, const void *reply, size_t reply_size) {
    return (int)invoke(AIXOS_SC_MSG_REPLY, (uintptr_t)conn, (uintptr_t)reply,
                       reply_size, 0, 0);
}
