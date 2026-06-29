#include "aixos/pipe.h"
#include "aixos/heap.h"
#include "aixos/task.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct {
    unsigned char *buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t count;
    aixos_handle_t handle;
    aixos_list_t read_wait;
    aixos_list_t write_wait;
    uint8_t owns_buffer;
} aixos_pipe_t;

static aixos_pipe_t pipe_pool[AIXOS_CFG_MAX_PIPE];

static aixos_pipe_t *pipe_from_handle(aixos_handle_t handle)
{
    return (aixos_pipe_t *)aixos_obj_from_handle(handle, AIXOS_OBJ_PIPE);
}

static aixos_handle_t pipe_create_common(unsigned char *buffer, size_t size,
                                         int owns_buffer)
{
    int i;
    int slot;
    aixos_pipe_t *pipe;
    aixos_arch_flags_t flags;
    if (aixos_in_isr() || buffer == NULL || size == 0U) {
        return AIXOS_HANDLE_INVALID;
    }
    flags = aixos_arch_int_disable();
    for (i = 0; i < AIXOS_CFG_MAX_PIPE; i++) {
        if (pipe_pool[i].handle == 0) {
            break;
        }
    }
    if (i == AIXOS_CFG_MAX_PIPE) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    pipe = &pipe_pool[i];
    slot = aixos_slot_alloc(AIXOS_POOL_PIPE, pipe);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    memset(pipe, 0, sizeof(*pipe));
    pipe->buffer = buffer;
    pipe->size = size;
    pipe->handle = aixos_slot_handle(AIXOS_POOL_PIPE, slot);
    pipe->owns_buffer = owns_buffer != 0;
    aixos_list_init(&pipe->read_wait);
    aixos_list_init(&pipe->write_wait);
    aixos_arch_int_restore(flags);
    return pipe->handle;
}

aixos_handle_t aixos_pipe_create(size_t size)
{
    unsigned char *buffer;
    aixos_handle_t handle;
    if (size == 0U) {
        return AIXOS_HANDLE_INVALID;
    }
    buffer = (unsigned char *)aixos_malloc(size);
    if (buffer == NULL) {
        return AIXOS_HANDLE_INVALID;
    }
    handle = pipe_create_common(buffer, size, 1);
    if (handle == AIXOS_HANDLE_INVALID) {
        aixos_free(buffer);
    }
    return handle;
}

aixos_handle_t aixos_pipe_create_static(void *buffer, size_t size)
{
    return pipe_create_common((unsigned char *)buffer, size, 0);
}

static int pipe_write_common(aixos_handle_t handle, const void *data,
                             size_t length, uint32_t timeout_ms, int from_isr)
{
    aixos_pipe_t *pipe;
    size_t first;
    size_t writable;
    aixos_arch_flags_t flags;
    uint32_t start_tick = aixos_get_tick();
    uint32_t remaining = timeout_ms;
    if (data == NULL || length > AIXOS_CFG_MAX_IPC_COPY_BYTES) {
        return AIXOS_ERR_INVAL;
    }
    if (from_isr && length > AIXOS_CFG_ISR_COPY_MAX_BYTES) {
        return AIXOS_ERR_INVAL;
    }
    if ((aixos_in_isr() != 0) != (from_isr != 0)) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    pipe = pipe_from_handle(handle);
    if (pipe == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    writable = pipe->size - pipe->count;
    while (writable == 0U) {
        int result;
        if (from_isr || remaining == 0U) {
            aixos_arch_int_restore(flags);
            return 0;
        }
        result = aixos_task_block_current(&pipe->write_wait, pipe,
                                          AIXOS_OBJ_PIPE, remaining, flags);
        if (result != AIXOS_OK) {
            return result;
        }
        remaining = aixos_timeout_remaining_ms(start_tick, timeout_ms);
        flags = aixos_arch_int_disable();
        pipe = pipe_from_handle(handle);
        if (pipe == NULL) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
        }
        writable = pipe->size - pipe->count;
    }
    if (length > writable) {
        length = writable;
    }
    first = pipe->size - pipe->tail;
    if (first > length) {
        first = length;
    }
    memcpy(&pipe->buffer[pipe->tail], data, first);
    memcpy(pipe->buffer, (const unsigned char *)data + first, length - first);
    pipe->tail = (pipe->tail + length) % pipe->size;
    pipe->count += length;
    if (length != 0U && !aixos_list_is_empty(&pipe->read_wait)) {
        aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(
            aixos_list_first(&pipe->read_wait), aixos_tcb_t, wait_node);
        aixos_task_wake(tcb, AIXOS_OK);
    }
    aixos_arch_int_restore(flags);
    return (int)length;
}

static int pipe_read_common(aixos_handle_t handle, void *buffer, size_t length,
                            uint32_t timeout_ms, int from_isr)
{
    aixos_pipe_t *pipe;
    size_t first;
    aixos_arch_flags_t flags;
    uint32_t start_tick = aixos_get_tick();
    uint32_t remaining = timeout_ms;
    if (buffer == NULL || length > AIXOS_CFG_MAX_IPC_COPY_BYTES) {
        return AIXOS_ERR_INVAL;
    }
    if (from_isr && length > AIXOS_CFG_ISR_COPY_MAX_BYTES) {
        return AIXOS_ERR_INVAL;
    }
    if ((aixos_in_isr() != 0) != (from_isr != 0)) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    pipe = pipe_from_handle(handle);
    if (pipe == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    while (pipe->count == 0U) {
        int result;
        if (from_isr || remaining == 0U) {
            aixos_arch_int_restore(flags);
            return 0;
        }
        result = aixos_task_block_current(&pipe->read_wait, pipe,
                                          AIXOS_OBJ_PIPE, remaining, flags);
        if (result != AIXOS_OK) {
            return result;
        }
        remaining = aixos_timeout_remaining_ms(start_tick, timeout_ms);
        flags = aixos_arch_int_disable();
        pipe = pipe_from_handle(handle);
        if (pipe == NULL) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_INVAL;
        }
    }
    if (length > pipe->count) {
        length = pipe->count;
    }
    first = pipe->size - pipe->head;
    if (first > length) {
        first = length;
    }
    memcpy(buffer, &pipe->buffer[pipe->head], first);
    memcpy((unsigned char *)buffer + first, pipe->buffer, length - first);
    pipe->head = (pipe->head + length) % pipe->size;
    pipe->count -= length;
    if (length != 0U && !aixos_list_is_empty(&pipe->write_wait)) {
        aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(
            aixos_list_first(&pipe->write_wait), aixos_tcb_t, wait_node);
        aixos_task_wake(tcb, AIXOS_OK);
    }
    aixos_arch_int_restore(flags);
    return (int)length;
}

int aixos_pipe_write(aixos_handle_t handle, const void *data, size_t length,
                     uint32_t timeout_ms)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return pipe_write_common(handle, data, length, timeout_ms, 0);
}

int aixos_pipe_read(aixos_handle_t handle, void *buffer, size_t length,
                    uint32_t timeout_ms)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return pipe_read_common(handle, buffer, length, timeout_ms, 0);
}

int aixos_pipe_write_from_isr(aixos_handle_t handle, const void *data,
                              size_t length)
{
    if (!aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return pipe_write_common(handle, data, length, 0U, 1);
}

int aixos_pipe_delete(aixos_handle_t handle)
{
    aixos_pipe_t *pipe;
    unsigned char *buffer;
    int owns_buffer;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    pipe = pipe_from_handle(handle);
    if (pipe == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (!aixos_list_is_empty(&pipe->read_wait) ||
        !aixos_list_is_empty(&pipe->write_wait)) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_BUSY;
    }
    buffer = pipe->buffer;
    owns_buffer = pipe->owns_buffer;
    aixos_slot_free(AIXOS_POOL_PIPE, (int)AIXOS_HANDLE_IDX(handle));
    memset(pipe, 0, sizeof(*pipe));
    aixos_arch_int_restore(flags);
    if (owns_buffer) {
        aixos_free(buffer);
    }
    return AIXOS_OK;
}

#ifdef AIXOS_HOST_TEST
int aixos_test_pipe_add_waiter(aixos_handle_t handle, aixos_handle_t task,
                               int writer)
{
    aixos_pipe_t *pipe;
    aixos_tcb_t *tcb;
    aixos_list_t *wait_list;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    pipe = pipe_from_handle(handle);
    tcb = aixos_tcb_from_handle(task);
    if (pipe == NULL || tcb == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    wait_list = writer != 0 ? &pipe->write_wait : &pipe->read_wait;
    aixos_sched_remove_task(tcb);
    tcb->state = AIXOS_TASK_BLOCKED;
    tcb->wait_obj = pipe;
    tcb->wait_list = wait_list;
    tcb->wait_type = AIXOS_OBJ_PIPE;
    tcb->wait_result = AIXOS_OK;
    aixos_list_add_tail(&tcb->wait_node, wait_list);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}
#endif
