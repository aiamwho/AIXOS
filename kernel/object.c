#include "object.h"
#include "list.h"
#include "config/aixos_cfg.h"
#include "config/string.h"
#include "kernel/heap_internal.h"

#if AIXOS_CFG_MAX_SEM > 32 || AIXOS_CFG_MAX_MUTEX > 32 || \
    AIXOS_CFG_MAX_MQ > 32 || \
    AIXOS_CFG_MAX_EVENT > 32 || AIXOS_CFG_MAX_PIPE > 32 || \
    AIXOS_CFG_MAX_TIMER > 32
#error "Object free bitmap supports at most 32 slots per pool"
#endif

/* ── 对象池定义 ────────────────────────────── */
static aixos_slot_t slots_sem[AIXOS_CFG_MAX_SEM];
static aixos_slot_t slots_mutex[AIXOS_CFG_MAX_MUTEX];
static aixos_slot_t slots_mq[AIXOS_CFG_MAX_MQ];
static aixos_slot_t slots_event[AIXOS_CFG_MAX_EVENT];
static aixos_slot_t slots_pipe[AIXOS_CFG_MAX_PIPE];
static aixos_slot_t slots_timer[AIXOS_CFG_MAX_TIMER];

static aixos_pool_t pool_task   = { NULL, AIXOS_CFG_TASK_HANDLE_LIMIT, AIXOS_POOL_TASK, AIXOS_OBJ_TASK, "task", 0U };
static aixos_pool_t pool_sem    = { slots_sem, AIXOS_CFG_MAX_SEM, AIXOS_POOL_SEM, AIXOS_OBJ_SEM, "sem", 0U };
static aixos_pool_t pool_mutex  = { slots_mutex, AIXOS_CFG_MAX_MUTEX, AIXOS_POOL_MUTEX, AIXOS_OBJ_MUTEX, "mutex", 0U };
static aixos_pool_t pool_mq     = { slots_mq, AIXOS_CFG_MAX_MQ, AIXOS_POOL_MQ, AIXOS_OBJ_MQ, "mq", 0U };
static aixos_pool_t pool_event  = { slots_event, AIXOS_CFG_MAX_EVENT, AIXOS_POOL_EVENT, AIXOS_OBJ_EVENT, "event", 0U };
static aixos_pool_t pool_pipe   = { slots_pipe, AIXOS_CFG_MAX_PIPE, AIXOS_POOL_PIPE, AIXOS_OBJ_PIPE, "pipe", 0U };
static aixos_pool_t pool_timer  = { slots_timer, AIXOS_CFG_MAX_TIMER, AIXOS_POOL_TIMER, AIXOS_OBJ_TIMER, "timer", 0U };

static aixos_pool_t *pools[AIXOS_POOL_COUNT];

typedef struct {
    aixos_slot_t slots[AIXOS_CFG_TASK_SLOT_PAGE_SIZE];
    uint32_t free_bitmap;
} aixos_task_slot_page_t;

#define TASK_SLOT_PAGE_COUNT \
    (AIXOS_CFG_TASK_HANDLE_LIMIT / AIXOS_CFG_TASK_SLOT_PAGE_SIZE)

static aixos_task_slot_page_t *task_slot_pages[TASK_SLOT_PAGE_COUNT];

static aixos_slot_t *task_slot_at(int index)
{
    uint32_t page_index;
    uint32_t slot_index;
    if (index < 0 || index >= AIXOS_CFG_TASK_HANDLE_LIMIT) {
        return NULL;
    }
    page_index = (uint32_t)index / AIXOS_CFG_TASK_SLOT_PAGE_SIZE;
    slot_index = (uint32_t)index % AIXOS_CFG_TASK_SLOT_PAGE_SIZE;
    if (task_slot_pages[page_index] == NULL) {
        return NULL;
    }
    return &task_slot_pages[page_index]->slots[slot_index];
}

static uint32_t task_page_free_mask(void)
{
    return AIXOS_CFG_TASK_SLOT_PAGE_SIZE == 32 ?
           UINT32_MAX :
           (UINT32_C(1) << AIXOS_CFG_TASK_SLOT_PAGE_SIZE) - 1U;
}

static int task_slot_alloc(void *obj)
{
    uint32_t page_index;
    for (page_index = 0U; page_index < TASK_SLOT_PAGE_COUNT; page_index++) {
        aixos_task_slot_page_t *page = task_slot_pages[page_index];
        uint32_t slot_index;
        aixos_slot_t *slot;
        if (page == NULL) {
            page = (aixos_task_slot_page_t *)
                aixos_kernel_malloc(sizeof(*page));
            if (page == NULL) {
                return -1;
            }
            memset(page, 0, sizeof(*page));
            page->free_bitmap = task_page_free_mask();
            task_slot_pages[page_index] = page;
        }
        if (page->free_bitmap == 0U) {
            continue;
        }
        slot_index = (uint32_t)__builtin_ctz(page->free_bitmap);
        page->free_bitmap &= ~(UINT32_C(1) << slot_index);
        slot = &page->slots[slot_index];
        slot->used = 1U;
        slot->generation = (slot->generation + 1U) & 0x00FFFFFFU;
        if (slot->generation == 0U) {
            slot->generation = 1U;
        }
        slot->type = (uint8_t)AIXOS_OBJ_TASK;
        slot->obj = obj;
        return (int)(page_index * AIXOS_CFG_TASK_SLOT_PAGE_SIZE +
                     slot_index);
    }
    return -1;
}

/* 对象类型到池 ID 的映射 */
static int type_to_pool_id(aixos_obj_type_t type)
{
    switch (type) {
        case AIXOS_OBJ_TASK:  return AIXOS_POOL_TASK;
        case AIXOS_OBJ_SEM:   return AIXOS_POOL_SEM;
        case AIXOS_OBJ_MUTEX: return AIXOS_POOL_MUTEX;
        case AIXOS_OBJ_MQ:    return AIXOS_POOL_MQ;
        case AIXOS_OBJ_EVENT: return AIXOS_POOL_EVENT;
        case AIXOS_OBJ_PIPE:      return AIXOS_POOL_PIPE;
        case AIXOS_OBJ_TIMER:     return AIXOS_POOL_TIMER;
        default:                  return -1;
    }
}

void aixos_object_init(void)
{
    int i;
    memset(task_slot_pages, 0, sizeof(task_slot_pages));
    pools[AIXOS_POOL_TASK]  = &pool_task;
    pools[AIXOS_POOL_SEM]   = &pool_sem;
    pools[AIXOS_POOL_MUTEX] = &pool_mutex;
    pools[AIXOS_POOL_MQ]    = &pool_mq;
    pools[AIXOS_POOL_EVENT] = &pool_event;
    pools[AIXOS_POOL_PIPE]  = &pool_pipe;
    pools[AIXOS_POOL_TIMER] = &pool_timer;

    for (i = 0; i < AIXOS_POOL_COUNT; i++) {
        if (i == AIXOS_POOL_TASK) {
            continue;
        }
        memset(pools[i]->slots, 0, sizeof(aixos_slot_t) * pools[i]->max_count);
        pools[i]->free_bitmap = pools[i]->max_count == 32 ?
                                UINT32_MAX :
                                (UINT32_C(1) << pools[i]->max_count) - 1U;
    }
}

int aixos_slot_alloc(int pool_id, void *obj)
{
    aixos_pool_t *pool;
    uint32_t available;
    int i = 0;

    if (pool_id < 0 || pool_id >= AIXOS_POOL_COUNT) return -1;
    if (pool_id == AIXOS_POOL_TASK) {
        return task_slot_alloc(obj);
    }
    pool = pools[pool_id];
    available = pool->free_bitmap;
    if (available == 0U) {
        return -1;
    }
    while ((available & 1U) == 0U) {
        available >>= 1U;
        i++;
    }
    pool->free_bitmap &= ~(UINT32_C(1) << i);
    pool->slots[i].used = 1;
    pool->slots[i].generation =
        (pool->slots[i].generation + 1U) & 0x00FFFFFFU;
    if (pool->slots[i].generation == 0U) {
        pool->slots[i].generation = 1U;
    }
    pool->slots[i].type = (uint8_t)pool->obj_type;
    pool->slots[i].obj = obj;
    return i;
}

aixos_handle_t aixos_slot_handle(int pool_id, int index)
{
    aixos_pool_t *pool;
    aixos_slot_t *slot;
    if (pool_id < 0 || pool_id >= AIXOS_POOL_COUNT) return AIXOS_HANDLE_INVALID;
    pool = pools[pool_id];
    slot = pool_id == AIXOS_POOL_TASK ?
           task_slot_at(index) :
           (index >= 0 && index < pool->max_count ?
            &pool->slots[index] : NULL);
    if (slot == NULL || !slot->used) {
        return AIXOS_HANDLE_INVALID;
    }
    return AIXOS_HANDLE_MAKE(index, slot->generation);
}

void aixos_slot_free(int pool_id, int index)
{
    aixos_pool_t *pool;
    aixos_slot_t *slot;
    if (pool_id < 0 || pool_id >= AIXOS_POOL_COUNT) return;
    pool = pools[pool_id];
    slot = pool_id == AIXOS_POOL_TASK ?
           task_slot_at(index) :
           (index >= 0 && index < pool->max_count ?
            &pool->slots[index] : NULL);
    if (slot == NULL || !slot->used) return;

    slot->used = 0;
    slot->obj = NULL;
    if (pool_id == AIXOS_POOL_TASK) {
        uint32_t page_index =
            (uint32_t)index / AIXOS_CFG_TASK_SLOT_PAGE_SIZE;
        uint32_t slot_index =
            (uint32_t)index % AIXOS_CFG_TASK_SLOT_PAGE_SIZE;
        task_slot_pages[page_index]->free_bitmap |=
            UINT32_C(1) << slot_index;
    } else {
        pool->free_bitmap |= UINT32_C(1) << index;
    }
    /* generation 保持递增, 不自减 */
}

void *aixos_obj_from_handle(aixos_handle_t h, aixos_obj_type_t type)
{
    int idx = AIXOS_HANDLE_IDX(h);
    uint32_t gen = AIXOS_HANDLE_GEN(h);
    int pool_id = type_to_pool_id(type);
    aixos_pool_t *pool;
    aixos_slot_t *slot;

    if (pool_id < 0) return NULL;
    pool = pools[pool_id];
    slot = pool_id == AIXOS_POOL_TASK ?
           task_slot_at(idx) :
           (idx >= 0 && idx < pool->max_count ?
            &pool->slots[idx] : NULL);
    if (slot == NULL || !slot->used || slot->generation != gen ||
        slot->type != (uint8_t)type) {
        return NULL;
    }
    return slot->obj;
}

int aixos_handle_is_valid(aixos_handle_t h, aixos_obj_type_t type)
{
    int pool_id = type_to_pool_id(type);
    int idx = AIXOS_HANDLE_IDX(h);
    uint32_t gen = AIXOS_HANDLE_GEN(h);
    aixos_pool_t *pool;
    aixos_slot_t *slot;

    if (pool_id < 0 || h == AIXOS_HANDLE_INVALID) return 0;
    pool = pools[pool_id];
    slot = pool_id == AIXOS_POOL_TASK ?
           task_slot_at(idx) :
           (idx >= 0 && idx < pool->max_count ?
            &pool->slots[idx] : NULL);
    return slot != NULL && slot->used && slot->generation == gen &&
           slot->type == (uint8_t)type;
}

int aixos_pool_get_usage(int pool_id)
{
    aixos_pool_t *pool;
    uint32_t used;
    int count = 0;
    if (pool_id < 0 || pool_id >= AIXOS_POOL_COUNT) return 0;
    if (pool_id == AIXOS_POOL_TASK) {
        uint32_t page_index;
        for (page_index = 0U; page_index < TASK_SLOT_PAGE_COUNT;
             page_index++) {
            uint32_t free_bitmap;
            if (task_slot_pages[page_index] == NULL) {
                continue;
            }
            free_bitmap = task_slot_pages[page_index]->free_bitmap;
            count += AIXOS_CFG_TASK_SLOT_PAGE_SIZE -
                     __builtin_popcount(free_bitmap);
        }
        return count;
    }
    pool = pools[pool_id];
    used = ~pool->free_bitmap;
    if (pool->max_count < 32) {
        used &= (UINT32_C(1) << pool->max_count) - 1U;
    }
    while (used != 0U) {
        count += (int)(used & 1U);
        used >>= 1U;
    }
    return count;
}

#ifdef AIXOS_HOST_TEST
int aixos_test_set_slot_generation(int pool_id, int index,
                                   uint32_t generation)
{
    aixos_pool_t *pool;
    aixos_slot_t *slot;
    if (pool_id < 0 || pool_id >= AIXOS_POOL_COUNT) {
        return AIXOS_ERR_INVAL;
    }
    pool = pools[pool_id];
    slot = pool_id == AIXOS_POOL_TASK ?
           task_slot_at(index) :
           (index >= 0 && index < pool->max_count ?
            &pool->slots[index] : NULL);
    if (slot == NULL || slot->used) {
        return AIXOS_ERR_INVAL;
    }
    slot->generation = generation & 0x00FFFFFFU;
    return AIXOS_OK;
}
#endif
