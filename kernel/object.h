#ifndef AIXOS_OBJECT_H
#define AIXOS_OBJECT_H

#include "aixos/types.h"
#include "list.h"

/*
 * 内核对象池
 * 所有内核对象 (TCB/Sem/Mutex/MQ/Event/Pipe/Timer) 使用统一的对象管理:
 *   - 静态池分配 (避免 heap 碎片)
 *   - 世代计数句柄 (防 dangling handle)
 *   - O(1) 查找
 */

/* 对象池配置 (与 config/aixos_cfg.h 同步) */
#define AIXOS_POOL_TASK     0
#define AIXOS_POOL_SEM      1
#define AIXOS_POOL_MUTEX    2
#define AIXOS_POOL_MQ       3
#define AIXOS_POOL_EVENT    4
#define AIXOS_POOL_PIPE     5
#define AIXOS_POOL_TIMER    6
/* Signal and namespace use static tables, not object pools */
#define AIXOS_POOL_COUNT    7

/* 对象池槽位 */
typedef struct {
    uint8_t     type;           /* aixos_obj_type_t */
    uint8_t     used;           /* 1=已使用 */
    uint32_t    generation;     /* 24-bit generation stored in handle */
    void       *obj;            /* 指向实际对象内存 */
} aixos_slot_t;

/* 对象池 */
typedef struct {
    aixos_slot_t *slots;
    int           max_count;
    int           type_id;      /* AIXOS_POOL_xxx */
    aixos_obj_type_t obj_type;
    const char   *name;
    uint32_t      free_bitmap;
} aixos_pool_t;

/* 初始化对象池 */
void aixos_object_init(void);

/* 分配/释放槽位 */
int  aixos_slot_alloc(int pool_id, void *obj);
void aixos_slot_free(int pool_id, int index);
aixos_handle_t aixos_slot_handle(int pool_id, int index);

/* 句柄 <-> 槽位 转换 */
void *aixos_obj_from_handle(aixos_handle_t h, aixos_obj_type_t type);
int   aixos_handle_is_valid(aixos_handle_t h, aixos_obj_type_t type);

/* 获取对象信息 (调试用) */
int  aixos_pool_get_usage(int pool_id);

#ifdef AIXOS_HOST_TEST
int aixos_test_set_slot_generation(int pool_id, int index,
                                   uint32_t generation);
#endif

#endif /* AIXOS_OBJECT_H */
