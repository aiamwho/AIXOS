#ifndef AIXOS_TIMEWHEEL_H
#define AIXOS_TIMEWHEEL_H

#include <stdint.h>
#include "aixos/task.h"

/*
 * Hierarchical timing wheel for efficient timeout management.
 */

#if AIXOS_CFG_ENABLE_TIME_WHEEL
void aixos_timing_wheel_init(void);
void aixos_timing_wheel_tick(uint32_t now);
void aixos_timing_wheel_insert(aixos_tcb_t *tcb, uint32_t wake_tick);
#else
static inline void aixos_timing_wheel_init(void) {}
static inline void aixos_timing_wheel_tick(uint32_t now) { (void)now; }
static inline void aixos_timing_wheel_insert(aixos_tcb_t *tcb, uint32_t wake_tick)
{
    (void)tcb; (void)wake_tick;
}
#endif

#endif /* AIXOS_TIMEWHEEL_H */
