#include "aixos/mq.h"
#include "aixos/heap.h"
#include "aixos/task.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct {
    unsigned char *buffer;
    size_t *lengths;
    uint32_t *priorities;
    size_t max_msgs;
    size_t msg_size;
    size_t head;
    size_t tail;
    size_t count;
    aixos_handle_t handle;
    aixos_list_t send_wait;
    aixos_list_t recv_wait;
    uint8_t owns_buffers;
} aixos_mq_t;

static aixos_mq_t mq_pool[AIXOS_CFG_MAX_MQ];

static aixos_mq_t *mq_from_handle(aixos_handle_t handle)
{
    return (aixos_mq_t *)aixos_obj_from_handle(handle, AIXOS_OBJ_MQ);
}

static aixos_handle_t mq_create_common(size_t max_msgs, size_t msg_size,
                                       unsigned char *buffer, size_t *lengths,
                                       uint32_t *priorities,
                                       int owns_buffers)
{
    int i;
    int slot;
    aixos_mq_t *queue;
    aixos_arch_flags_t flags;
    if (aixos_in_isr() || max_msgs == 0U || msg_size == 0U ||
        msg_size > AIXOS_CFG_MAX_IPC_COPY_BYTES ||
        max_msgs > (size_t)-1 / msg_size ||
        max_msgs > (size_t)-1 / sizeof(size_t) ||
        buffer == NULL || lengths == NULL) {
        return AIXOS_HANDLE_INVALID;
    }
    flags = aixos_arch_int_disable();
    for (i = 0; i < AIXOS_CFG_MAX_MQ; i++) {
        if (mq_pool[i].handle == 0) {
            break;
        }
    }
    if (i == AIXOS_CFG_MAX_MQ) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    queue = &mq_pool[i];
    slot = aixos_slot_alloc(AIXOS_POOL_MQ, queue);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    memset(queue, 0, sizeof(*queue));
    memset(lengths, 0, max_msgs * sizeof(*lengths));
    if (priorities != NULL) {
        memset(priorities, 0, max_msgs * sizeof(*priorities));
    }
    queue->buffer = buffer;
    queue->lengths = lengths;
    queue->priorities = priorities;
    queue->max_msgs = max_msgs;
    queue->msg_size = msg_size;
    queue->handle = aixos_slot_handle(AIXOS_POOL_MQ, slot);
    queue->owns_buffers = owns_buffers != 0;
    aixos_list_init(&queue->send_wait);
    aixos_list_init(&queue->recv_wait);
    aixos_arch_int_restore(flags);
    return queue->handle;
}

aixos_handle_t aixos_mq_create(size_t max_msgs, size_t msg_size)
{
    unsigned char *buffer;
    size_t *lengths;
    uint32_t *priorities;
    aixos_handle_t handle;
    if (max_msgs == 0U || msg_size == 0U ||
        msg_size > AIXOS_CFG_MAX_IPC_COPY_BYTES ||
        max_msgs > (size_t)-1 / msg_size ||
        max_msgs > (size_t)-1 / sizeof(size_t)) {
        return AIXOS_HANDLE_INVALID;
    }
    buffer = (unsigned char *)aixos_malloc(max_msgs * msg_size);
    lengths = (size_t *)aixos_calloc(max_msgs, sizeof(size_t));
    priorities = (uint32_t *)aixos_calloc(max_msgs, sizeof(uint32_t));
    if (buffer == NULL || lengths == NULL || priorities == NULL) {
        aixos_free(buffer);
        aixos_free(lengths);
        aixos_free(priorities);
        return AIXOS_HANDLE_INVALID;
    }
    handle = mq_create_common(max_msgs, msg_size, buffer, lengths,
                              priorities, 1);
    if (handle == AIXOS_HANDLE_INVALID) {
        aixos_free(buffer);
        aixos_free(lengths);
        aixos_free(priorities);
    }
    return handle;
}

aixos_handle_t aixos_mq_create_static(size_t max_msgs, size_t msg_size,
                                      void *buffer, size_t *lengths)
{
    return mq_create_common(max_msgs, msg_size, (unsigned char *)buffer,
                            lengths, NULL, 0);
}

static int mq_send_common(aixos_handle_t handle, const void *message,
                          size_t size, uint32_t priority,
                          uint32_t timeout_ms, int from_isr)
{
    aixos_mq_t *queue;
    aixos_arch_flags_t flags;
    uint32_t start_tick = aixos_get_tick();
    uint32_t remaining = timeout_ms;
    if (message == NULL || priority > AIXOS_CFG_MQ_PRIORITY_MAX ||
        (aixos_in_isr() != 0) != (from_isr != 0)) {
        return from_isr ? AIXOS_ERR_CONTEXT : AIXOS_ERR_INVAL;
    }
    if (from_isr && size > AIXOS_CFG_ISR_COPY_MAX_BYTES) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    queue = mq_from_handle(handle);
    if (queue == NULL || size > queue->msg_size) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    while (queue->count == queue->max_msgs) {
        int result;
        if (from_isr || remaining == 0U) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_BUSY;
        }
        result = aixos_task_block_current(&queue->send_wait, queue,
                                          AIXOS_OBJ_MQ, remaining, flags);
        if (result != AIXOS_OK) {
            return result;
        }
        remaining = aixos_timeout_remaining_ms(start_tick, timeout_ms);
        flags = aixos_arch_int_disable();
        queue = mq_from_handle(handle);
        if (queue == NULL) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
        }
    }
    {
        size_t insert = queue->count;
        size_t logical;
        if (queue->priorities == NULL && priority != 0U) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
        }
        if (from_isr && priority != 0U &&
            queue->count > AIXOS_CFG_ISR_MQ_SHIFT_MAX) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_BUSY;
        }
        if (queue->priorities != NULL && priority != 0U) {
            for (logical = 0U; logical < queue->count; logical++) {
                size_t index = (queue->head + logical) % queue->max_msgs;
                if (queue->priorities[index] < priority) {
                    insert = logical;
                    break;
                }
            }
        }
        for (logical = queue->count; logical > insert; logical--) {
            size_t destination =
                (queue->head + logical) % queue->max_msgs;
            size_t source =
                (queue->head + logical - 1U) % queue->max_msgs;
            memcpy(&queue->buffer[destination * queue->msg_size],
                   &queue->buffer[source * queue->msg_size],
                   queue->msg_size);
            queue->lengths[destination] = queue->lengths[source];
            if (queue->priorities != NULL) {
                queue->priorities[destination] =
                    queue->priorities[source];
            }
        }
        queue->tail = (queue->head + insert) % queue->max_msgs;
    }
    memcpy(&queue->buffer[queue->tail * queue->msg_size], message, size);
    queue->lengths[queue->tail] = size;
    if (queue->priorities != NULL) {
        queue->priorities[queue->tail] = priority;
    }
    queue->tail = (queue->tail + 1U) % queue->max_msgs;
    queue->count++;
    if (!aixos_list_is_empty(&queue->recv_wait)) {
        aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(
            aixos_list_first(&queue->recv_wait), aixos_tcb_t, wait_node);
        aixos_task_wake(tcb, AIXOS_OK);
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_mq_send(aixos_handle_t handle, const void *message, size_t size,
                  uint32_t timeout_ms)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return mq_send_common(handle, message, size, 0U, timeout_ms, 0);
}

int aixos_mq_send_priority(aixos_handle_t handle, const void *message,
                           size_t size, uint32_t priority,
                           uint32_t timeout_ms)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return mq_send_common(handle, message, size, priority, timeout_ms, 0);
}

int aixos_mq_send_from_isr(aixos_handle_t handle, const void *message,
                           size_t size)
{
    if (!aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return mq_send_common(handle, message, size, 0U, 0U, 1);
}

int aixos_mq_recv_priority(aixos_handle_t handle, void *buffer,
                           size_t capacity, size_t *size,
                           uint32_t *priority, uint32_t timeout_ms)
{
    aixos_mq_t *queue;
    aixos_arch_flags_t flags;
    uint32_t start_tick = aixos_get_tick();
    uint32_t remaining = timeout_ms;
    size_t message_size;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    if (buffer == NULL || size == NULL) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    queue = mq_from_handle(handle);
    if (queue == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    while (queue->count == 0U) {
        int result;
        if (remaining == 0U) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_AGAIN;
        }
        result = aixos_task_block_current(&queue->recv_wait, queue,
                                          AIXOS_OBJ_MQ, remaining, flags);
        if (result != AIXOS_OK) {
            return result;
        }
        remaining = aixos_timeout_remaining_ms(start_tick, timeout_ms);
        flags = aixos_arch_int_disable();
        queue = mq_from_handle(handle);
        if (queue == NULL) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
        }
    }
    message_size = queue->lengths[queue->head];
    *size = message_size;
    if (message_size > capacity) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_OVERFLOW;
    }
    memcpy(buffer, &queue->buffer[queue->head * queue->msg_size],
           message_size);
    if (priority != NULL) {
        *priority = queue->priorities != NULL ?
                    queue->priorities[queue->head] : 0U;
    }
    queue->head = (queue->head + 1U) % queue->max_msgs;
    queue->count--;
    if (!aixos_list_is_empty(&queue->send_wait)) {
        aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(
            aixos_list_first(&queue->send_wait), aixos_tcb_t, wait_node);
        aixos_task_wake(tcb, AIXOS_OK);
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_mq_recv(aixos_handle_t handle, void *buffer, size_t capacity,
                  size_t *size, uint32_t timeout_ms)
{
    return aixos_mq_recv_priority(handle, buffer, capacity, size, NULL,
                                  timeout_ms);
}

int aixos_mq_get_info(aixos_handle_t handle, aixos_mq_info_t *info)
{
    aixos_mq_t *queue;
    aixos_arch_flags_t flags;
    if (info == NULL) return AIXOS_ERR_INVAL;
    flags = aixos_arch_int_disable();
    queue = mq_from_handle(handle);
    if (queue == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    info->max_messages = queue->max_msgs;
    info->message_size = queue->msg_size;
    info->current_messages = queue->count;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_mq_delete(aixos_handle_t handle)
{
    aixos_mq_t *queue;
    unsigned char *buffer;
    size_t *lengths;
    uint32_t *priorities;
    int owns_buffers;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    queue = mq_from_handle(handle);
    if (queue == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (!aixos_list_is_empty(&queue->send_wait) ||
        !aixos_list_is_empty(&queue->recv_wait)) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_BUSY;
    }
    buffer = queue->buffer;
    lengths = queue->lengths;
    priorities = queue->priorities;
    owns_buffers = queue->owns_buffers;
    aixos_slot_free(AIXOS_POOL_MQ, (int)AIXOS_HANDLE_IDX(handle));
    memset(queue, 0, sizeof(*queue));
    aixos_arch_int_restore(flags);
    if (owns_buffers) {
        aixos_free(buffer);
        aixos_free(lengths);
        aixos_free(priorities);
    }
    return AIXOS_OK;
}

#ifdef AIXOS_HOST_TEST
int aixos_test_mq_add_waiter(aixos_handle_t handle, aixos_handle_t task,
                             int sender)
{
    aixos_mq_t *queue;
    aixos_tcb_t *tcb;
    aixos_list_t *wait_list;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    queue = mq_from_handle(handle);
    tcb = aixos_tcb_from_handle(task);
    if (queue == NULL || tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    wait_list = sender != 0 ? &queue->send_wait : &queue->recv_wait;
    aixos_sched_remove_task(tcb);
    tcb->state = AIXOS_TASK_BLOCKED;
    tcb->wait_obj = queue;
    tcb->wait_list = wait_list;
    tcb->wait_type = AIXOS_OBJ_MQ;
    tcb->wait_result = AIXOS_OK;
    aixos_list_add_tail(&tcb->wait_node, wait_list);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}
#endif
