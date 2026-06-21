#include "aixos/mutex.h"
#include "aixos/task.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct aixos_mutex {
    aixos_handle_t owner;
    unsigned int lock_count;
    aixos_handle_t handle;
    aixos_list_t wait_list;
    aixos_list_t owner_node;
} aixos_mutex_t;

static aixos_mutex_t mutex_pool[AIXOS_CFG_MAX_MUTEX];

static aixos_mutex_t *mutex_from_handle(aixos_handle_t handle)
{
    return (aixos_mutex_t *)aixos_obj_from_handle(handle, AIXOS_OBJ_MUTEX);
}

static int task_effective_priority(aixos_tcb_t *tcb)
{
    int priority = tcb->base_priority;
    aixos_list_t *position;
    AIXOS_LIST_FOR_EACH(position, &tcb->held_mutexes) {
        aixos_mutex_t *mutex =
            AIXOS_CONTAINER_OF(position, aixos_mutex_t, owner_node);
        if (!aixos_list_is_empty(&mutex->wait_list)) {
            aixos_tcb_t *waiter = AIXOS_CONTAINER_OF(
                aixos_list_first(&mutex->wait_list), aixos_tcb_t, wait_node);
            if (waiter->priority > priority) {
                priority = waiter->priority;
            }
        }
    }
    return priority;
}

static void recompute_chain(aixos_tcb_t *tcb, unsigned int depth)
{
    int old_priority;
    int new_priority;
    if (tcb == NULL || depth >= AIXOS_CFG_TASK_HANDLE_LIMIT) {
        return;
    }
    old_priority = tcb->priority;
    new_priority = task_effective_priority(tcb);
    if (new_priority != old_priority) {
        tcb->priority = new_priority;
        tcb->sched_boosted = (new_priority > tcb->base_priority) ? 1U : 0U;
        aixos_sched_requeue_task(tcb, old_priority);
        // 如果此任务被阻塞在另一个互斥量上，也重新排列其等待队列位置
        if (tcb->wait_type == AIXOS_OBJ_MUTEX && tcb->wait_obj != NULL) {
            aixos_task_waiter_priority_changed(tcb);
        }
    }
    // 传递继承: 如果此任务在等另一个互斥量，沿链向上传播
    if (tcb->wait_type == AIXOS_OBJ_MUTEX && tcb->wait_obj != NULL) {
        aixos_mutex_t *waiting_on = (aixos_mutex_t *)tcb->wait_obj;
        aixos_tcb_t *owner = aixos_tcb_from_handle(waiting_on->owner);
        if (owner != NULL && owner != tcb) {
            recompute_chain(owner, depth + 1U);
        }
    }
    // 反向传播: 检查持有此任务在等的互斥量的其他任务
    if (!aixos_list_is_empty(&tcb->held_mutexes)) {
        aixos_list_t *position;
        AIXOS_LIST_FOR_EACH(position, &tcb->held_mutexes) {
            aixos_mutex_t *held = AIXOS_CONTAINER_OF(position, aixos_mutex_t, owner_node);
            if (!aixos_list_is_empty(&held->wait_list)) {
                aixos_list_t *wp;
                AIXOS_LIST_FOR_EACH(wp, &held->wait_list) {
                    aixos_tcb_t *waiter = AIXOS_CONTAINER_OF(wp, aixos_tcb_t, wait_node);
                    if (waiter->priority > tcb->priority) {
                        // 已经在 recompute_chain 中处理了
                    }
                }
            }
        }
    }
}

aixos_handle_t aixos_mutex_create(void)
{
    int i;
    int slot;
    aixos_mutex_t *mutex;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_HANDLE_INVALID;
    }
    flags = aixos_arch_int_disable();
    for (i = 0; i < AIXOS_CFG_MAX_MUTEX; i++) {
        if (mutex_pool[i].handle == 0) {
            break;
        }
    }
    if (i == AIXOS_CFG_MAX_MUTEX) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    mutex = &mutex_pool[i];
    slot = aixos_slot_alloc(AIXOS_POOL_MUTEX, mutex);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    memset(mutex, 0, sizeof(*mutex));
    mutex->owner = AIXOS_HANDLE_INVALID;
    mutex->handle = aixos_slot_handle(AIXOS_POOL_MUTEX, slot);
    aixos_list_init(&mutex->wait_list);
    aixos_list_init(&mutex->owner_node);
    aixos_arch_int_restore(flags);
    return mutex->handle;
}

int aixos_mutex_lock(aixos_handle_t handle, uint32_t timeout_ms)
{
    aixos_mutex_t *mutex;
    aixos_tcb_t *self = g_cur_task;
    aixos_arch_flags_t flags;
    if (aixos_in_isr() || self == NULL) {
        return AIXOS_ERR_CONTEXT;
    }
    
    flags = aixos_arch_int_disable();
    mutex = mutex_from_handle(handle);
    if (mutex == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (mutex->owner == AIXOS_HANDLE_INVALID) {
        mutex->owner = self->handle;
        mutex->lock_count = 1U;
        aixos_list_add_tail(&mutex->owner_node, &self->held_mutexes);
        aixos_arch_int_restore(flags);
        return AIXOS_OK;
    }
    if (mutex->owner == self->handle) {
        if (mutex->lock_count == UINT32_MAX) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_OVERFLOW;
        }
        mutex->lock_count++;
        aixos_arch_int_restore(flags);
        return AIXOS_OK;
    }
    if (timeout_ms == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_BUSY;
    }

    return aixos_task_block_current(&mutex->wait_list, mutex,
                                    AIXOS_OBJ_MUTEX,
                                    timeout_ms, flags);
}

int aixos_mutex_unlock(aixos_handle_t handle)
{
    aixos_mutex_t *mutex;
    aixos_tcb_t *self = g_cur_task;
    aixos_arch_flags_t flags;
    if (aixos_in_isr() || self == NULL) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    mutex = mutex_from_handle(handle);
    if (mutex == NULL || mutex->owner != self->handle) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    mutex->lock_count--;
    if (mutex->lock_count != 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_OK;
    }

    aixos_list_del(&mutex->owner_node);
    aixos_list_init(&mutex->owner_node);
    if (!aixos_list_is_empty(&mutex->wait_list)) {
        aixos_tcb_t *next = AIXOS_CONTAINER_OF(
            aixos_list_first(&mutex->wait_list), aixos_tcb_t, wait_node);
        mutex->owner = next->handle;
        mutex->lock_count = 1U;
        aixos_list_add_tail(&mutex->owner_node, &next->held_mutexes);
        aixos_task_wake(next, AIXOS_OK);
    } else {
        mutex->owner = AIXOS_HANDLE_INVALID;
    }
    recompute_chain(self, 0U);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_mutex_delete(aixos_handle_t handle)
{
    aixos_mutex_t *mutex;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    mutex = mutex_from_handle(handle);
    if (mutex == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    // Deleting a mutex with waiters transfers ownership to the next waiter.
    if (!aixos_list_is_empty(&mutex->wait_list)) {
        aixos_tcb_t *next = AIXOS_CONTAINER_OF(
            aixos_list_first(&mutex->wait_list), aixos_tcb_t, wait_node);
        // 如果有 owner，先将其从持有链表移除
        if (mutex->owner != AIXOS_HANDLE_INVALID) {
            aixos_tcb_t *owner = aixos_tcb_from_handle(mutex->owner);
            if (owner != NULL && !aixos_list_is_empty(&mutex->owner_node)) {
                aixos_list_del(&mutex->owner_node);
                aixos_list_init(&mutex->owner_node);
                recompute_chain(owner, 0U);
            }
        }
        // 将所有权转给最高优先级等待者
        mutex->owner = next->handle;
        mutex->lock_count = 1U;
        aixos_list_add_tail(&mutex->owner_node, &next->held_mutexes);
        aixos_task_wake(next, AIXOS_OK);
    } else if (mutex->owner != AIXOS_HANDLE_INVALID) {
        // 没有等待者但有 owner - 从 owner 的持有链表中移除
        aixos_tcb_t *owner = aixos_tcb_from_handle(mutex->owner);
        if (owner != NULL && !aixos_list_is_empty(&mutex->owner_node)) {
            aixos_list_del(&mutex->owner_node);
            aixos_list_init(&mutex->owner_node);
            recompute_chain(owner, 0U);
        }
    }
    // 最后清理 mutex
    aixos_slot_free(AIXOS_POOL_MUTEX, (int)AIXOS_HANDLE_IDX(handle));
    memset(mutex, 0, sizeof(*mutex));
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_mutex_task_can_delete(aixos_tcb_t *tcb)
{
    return tcb != NULL && aixos_list_is_empty(&tcb->held_mutexes);
}

void aixos_mutex_waiter_added(aixos_tcb_t *tcb)
{
    aixos_mutex_t *mutex;
    if (tcb == NULL || tcb->wait_type != AIXOS_OBJ_MUTEX ||
        tcb->wait_obj == NULL) {
        return;
    }
    mutex = (aixos_mutex_t *)tcb->wait_obj;
    recompute_chain(aixos_tcb_from_handle(mutex->owner), 0U);
}

void aixos_mutex_waiter_removed(aixos_tcb_t *tcb)
{
    aixos_mutex_t *mutex;
    aixos_tcb_t *owner;
    if (tcb == NULL || tcb->wait_type != AIXOS_OBJ_MUTEX ||
        tcb->wait_obj == NULL) {
        return;
    }
    mutex = (aixos_mutex_t *)tcb->wait_obj;
    owner = aixos_tcb_from_handle(mutex->owner);
    recompute_chain(owner, 0U);
}

void aixos_mutex_task_priority_changed(aixos_tcb_t *tcb)
{
    if (tcb == NULL) {
        return;
    }
    aixos_task_waiter_priority_changed(tcb);
    if (tcb->wait_type == AIXOS_OBJ_MUTEX && tcb->wait_obj != NULL &&
        tcb->wait_node.next != &tcb->wait_node &&
        tcb->wait_node.next != NULL) {
        recompute_chain(aixos_tcb_from_handle(
            ((aixos_mutex_t *)tcb->wait_obj)->owner), 0U);
    }
    recompute_chain(tcb, 0U);
}
