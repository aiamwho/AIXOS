#include "aixos/sem.h"
#include "aixos/task.h"
#include "kernel/object.h"
#include "kernel/sched.h"
#include "aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"

typedef struct {
    int count;
    aixos_handle_t handle;
    aixos_list_t wait_list;
} aixos_sem_t;

static aixos_sem_t sem_pool[AIXOS_CFG_MAX_SEM];

static aixos_sem_t *sem_from_handle(aixos_handle_t handle)
{
    return (aixos_sem_t *)aixos_obj_from_handle(handle, AIXOS_OBJ_SEM);
}

aixos_handle_t aixos_sem_create(int initial_count)
{
    int i;
    int slot;
    aixos_sem_t *sem;
    aixos_arch_flags_t flags;
    if (aixos_in_isr() || initial_count < 0 ||
        initial_count > AIXOS_CFG_SEM_MAX_COUNT) {
        return AIXOS_HANDLE_INVALID;
    }
    flags = aixos_arch_int_disable();
    for (i = 0; i < AIXOS_CFG_MAX_SEM; i++) {
        if (sem_pool[i].handle == 0) {
            break;
        }
    }
    if (i == AIXOS_CFG_MAX_SEM) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    sem = &sem_pool[i];
    slot = aixos_slot_alloc(AIXOS_POOL_SEM, sem);
    if (slot < 0) {
        aixos_arch_int_restore(flags);
        return AIXOS_HANDLE_INVALID;
    }
    memset(sem, 0, sizeof(*sem));
    sem->count = initial_count;
    sem->handle = aixos_slot_handle(AIXOS_POOL_SEM, slot);
    aixos_list_init(&sem->wait_list);
    aixos_arch_int_restore(flags);
    return sem->handle;
}

int aixos_sem_wait(aixos_handle_t handle, uint32_t timeout_ms)
{
    aixos_sem_t *sem;
    aixos_arch_flags_t flags;
    
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    
    flags = aixos_arch_int_disable();
    sem = sem_from_handle(handle);
    if (sem == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (sem->count > 0) {
        sem->count--;
        aixos_arch_int_restore(flags);
        return AIXOS_OK;
    }
    if (timeout_ms == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_TIMEOUT;
    }
    return aixos_task_block_current(&sem->wait_list, sem,
                                    AIXOS_OBJ_SEM, timeout_ms, flags);
}

static int sem_post_common(aixos_handle_t handle)
{
    aixos_sem_t *sem;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    sem = sem_from_handle(handle);
    if (sem == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (!aixos_list_is_empty(&sem->wait_list)) {
        aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(
            aixos_list_first(&sem->wait_list), aixos_tcb_t, wait_node);
        aixos_task_wake(tcb, AIXOS_OK);
    } else {
        if (sem->count == AIXOS_CFG_SEM_MAX_COUNT) {
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_OVERFLOW;
        }
        sem->count++;
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_sem_post(aixos_handle_t handle)
{
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return sem_post_common(handle);
}

int aixos_sem_post_from_isr(aixos_handle_t handle)
{
    if (!aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    return sem_post_common(handle);
}

int aixos_sem_delete(aixos_handle_t handle)
{
    aixos_sem_t *sem;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    sem = sem_from_handle(handle);
    if (sem == NULL) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (!aixos_list_is_empty(&sem->wait_list)) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_BUSY;
    }
    aixos_slot_free(AIXOS_POOL_SEM, (int)AIXOS_HANDLE_IDX(handle));
    memset(sem, 0, sizeof(*sem));
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_sem_get_count(aixos_handle_t handle)
{
    aixos_sem_t *sem;
    int count;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    sem = sem_from_handle(handle);
    count = sem != NULL ? sem->count : AIXOS_ERR_INVAL;
    aixos_arch_int_restore(flags);
    return count;
}
