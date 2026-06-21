#include "aixos/types.h"
#include "aixos/task.h"
#include "kernel/list.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"

#if AIXOS_CFG_ENABLE_TIME_WHEEL

/* 分层时间轮 - 用于高效超时管理 */

typedef struct {
    aixos_list_t slots[AIXOS_CFG_TW_SIZE1];
    uint32_t current_tick_low;
} aixos_tw_level1_t;

typedef struct {
    aixos_list_t slots[AIXOS_CFG_TW_SIZE2];
    uint32_t current_tick_high;
} aixos_tw_level2_t;

static aixos_tw_level1_t tw_level1;
static aixos_tw_level2_t tw_level2;

void aixos_timing_wheel_init(void)
{
    unsigned int i;
    for (i = 0U; i < AIXOS_CFG_TW_SIZE1; i++) {
        aixos_list_init(&tw_level1.slots[i]);
    }
    for (i = 0; i < AIXOS_CFG_TW_SIZE2; i++) {
        aixos_list_init(&tw_level2.slots[i]);
    }
    tw_level1.current_tick_low = 0U;
    tw_level2.current_tick_high = 0U;
}

void aixos_timing_wheel_insert(aixos_tcb_t *tcb, uint32_t wake_tick)
{
    uint32_t now = aixos_get_tick();
    uint32_t diff = wake_tick - now;
    uint32_t slot;
    
    if (diff < AIXOS_CFG_TW_SIZE1) {
        // 在第一层
        slot = (tw_level1.current_tick_low + diff) % AIXOS_CFG_TW_SIZE1;
        aixos_list_add_tail(&tcb->timeout_node, &tw_level1.slots[slot]);
    } else if (diff < (uint32_t)(AIXOS_CFG_TW_SIZE1 * AIXOS_CFG_TW_SIZE2)) {
        // 在第二层
        slot = (diff >> AIXOS_CFG_TW_BITS1) % AIXOS_CFG_TW_SIZE2;
        slot = (tw_level2.current_tick_high + slot) % AIXOS_CFG_TW_SIZE2;
        aixos_list_add_tail(&tcb->timeout_node, &tw_level2.slots[slot]);
    } else {
        // 超出范围，放在第二层最后一个槽
        slot = (tw_level2.current_tick_high + AIXOS_CFG_TW_SIZE2 - 1U) % AIXOS_CFG_TW_SIZE2;
        aixos_list_add_tail(&tcb->timeout_node, &tw_level2.slots[slot]);
    }
}

void aixos_timing_wheel_tick(uint32_t now)
{
    aixos_list_t *position;
    aixos_list_t *next;
    uint32_t slot = tw_level1.current_tick_low;
    
    // 处理第一层当前槽中到期的所有超时
    AIXOS_LIST_FOR_EACH_SAFE(position, next, &tw_level1.slots[slot]) {
        aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(position, aixos_tcb_t, timeout_node);
        if ((int32_t)(now - tcb->wake_tick) >= 0) {
            aixos_list_del(&tcb->timeout_node);
            aixos_list_init(&tcb->timeout_node);
            int result = tcb->state == AIXOS_TASK_DELAYED ?
                         AIXOS_OK : AIXOS_ERR_TIMEOUT;
            aixos_task_wake(tcb, result);
        }
    }
    
    // 推进第一层指针
    tw_level1.current_tick_low = (slot + 1U) % AIXOS_CFG_TW_SIZE1;
    
    // 如果第一层回卷，处理第二层
    if (tw_level1.current_tick_low == 0U) {
        uint32_t high_slot = tw_level2.current_tick_high;
        // 将第二层当前槽的所有任务转移到第一层
        AIXOS_LIST_FOR_EACH_SAFE(position, next, &tw_level2.slots[high_slot]) {
            aixos_tcb_t *tcb = AIXOS_CONTAINER_OF(position, aixos_tcb_t, timeout_node);
            aixos_list_del(&tcb->timeout_node);
            aixos_list_init(&tcb->timeout_node);
            // 重新计算剩余时间并插入第一层
            uint32_t remaining = tcb->wake_tick - now;
            if (remaining < AIXOS_CFG_TW_SIZE1) {
                uint32_t new_slot = (tw_level1.current_tick_low + remaining) % AIXOS_CFG_TW_SIZE1;
                aixos_list_add_tail(&tcb->timeout_node, &tw_level1.slots[new_slot]);
            } else {
                // 仍在第二层，重新插入
                uint32_t new_slot = (remaining >> AIXOS_CFG_TW_BITS1) % AIXOS_CFG_TW_SIZE2;
                new_slot = (tw_level2.current_tick_high + new_slot) % AIXOS_CFG_TW_SIZE2;
                aixos_list_add_tail(&tcb->timeout_node, &tw_level2.slots[new_slot]);
            }
        }
        tw_level2.current_tick_high = (high_slot + 1U) % AIXOS_CFG_TW_SIZE2;
    }
}

#else
void aixos_timing_wheel_init(void) {}
void aixos_timing_wheel_tick(uint32_t now) { (void)now; }
void aixos_timing_wheel_insert(aixos_tcb_t *tcb, uint32_t wake_tick)
{
    (void)tcb; (void)wake_tick;
}
#endif
