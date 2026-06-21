#include "aixos/arch/arch.h"
#include "aixos/crash.h"
#include "aixos/task.h"
#include "aixos/types.h"
#include "config/aixos_cfg.h"

static volatile uint32_t isr_nesting;
static volatile uint32_t isr_nesting_high_watermark;
static volatile uint32_t isr_nesting_overflows;
static volatile uint32_t reschedule_pending;
static volatile uint32_t scheduler_lock_nesting;

static void isr_update_high_watermark(uint32_t level)
{
    uint32_t high = __atomic_load_n(&isr_nesting_high_watermark,
                                    __ATOMIC_RELAXED);
    while (level > high &&
           !__atomic_compare_exchange_n(&isr_nesting_high_watermark, &high,
                                        level, 0, __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED)) {
    }
}

static void isr_record_nesting_overflow(uint32_t level)
{
    uint32_t previous = __atomic_fetch_add(&isr_nesting_overflows, 1U,
                                           __ATOMIC_RELAXED);
    if (previous == 0U) {
        aixos_crash_record_store_extended(
            0U, AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW, 0U, 0U, 0U,
            level, AIXOS_CFG_ISR_NESTING_MAX,
            __atomic_load_n(&isr_nesting_high_watermark, __ATOMIC_RELAXED));
    }
#if AIXOS_CFG_ISR_NESTING_PANIC
    aixos_panic("ISR nesting overflow", AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW);
#endif
}

void aixos_isr_enter(void)
{
    uint32_t level = __atomic_add_fetch(&isr_nesting, 1U,
                                        __ATOMIC_SEQ_CST);
    isr_update_high_watermark(level);
    if (level > AIXOS_CFG_ISR_NESTING_MAX) {
        isr_record_nesting_overflow(level);
    }
}

void aixos_isr_exit(void)
{
    uint32_t old_level;
    uint32_t new_level;
    do {
        old_level = __atomic_load_n(&isr_nesting, __ATOMIC_SEQ_CST);
        if (old_level == 0U) {
            return;
        }
        new_level = old_level - 1U;
    } while (!__atomic_compare_exchange_n(&isr_nesting, &old_level,
                                          new_level, 0, __ATOMIC_SEQ_CST,
                                          __ATOMIC_SEQ_CST));

    if (new_level == 0U &&
        __atomic_load_n(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) == 0U &&
        __atomic_exchange_n(&reschedule_pending, 0U, __ATOMIC_SEQ_CST) != 0U) {
        aixos_arch_context_switch();
    }
}

int aixos_in_isr(void)
{
    return __atomic_load_n(&isr_nesting, __ATOMIC_SEQ_CST) != 0U;
}

void aixos_reschedule_request(void)
{
    if (__atomic_load_n(&isr_nesting, __ATOMIC_SEQ_CST) != 0U ||
        __atomic_load_n(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) != 0U) {
        __atomic_store_n(&reschedule_pending, 1U, __ATOMIC_SEQ_CST);
    } else {
        aixos_arch_context_switch();
    }
}

int aixos_sched_lock(void)
{
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    (void)__atomic_add_fetch(&scheduler_lock_nesting, 1U, __ATOMIC_SEQ_CST);
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

int aixos_sched_unlock(void)
{
    aixos_arch_flags_t flags;
    int switch_now = 0;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    flags = aixos_arch_int_disable();
    if (__atomic_load_n(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (__atomic_sub_fetch(&scheduler_lock_nesting, 1U,
                           __ATOMIC_SEQ_CST) == 0U &&
        __atomic_exchange_n(&reschedule_pending, 0U, __ATOMIC_SEQ_CST) != 0U) {
        switch_now = 1;
    }
    aixos_arch_int_restore(flags);
    if (switch_now != 0) {
        aixos_arch_context_switch();
    }
    return AIXOS_OK;
}

uint32_t aixos_sched_lock_count(void)
{
    return __atomic_load_n(&scheduler_lock_nesting, __ATOMIC_SEQ_CST);
}

int aixos_sched_is_locked(void)
{
    return __atomic_load_n(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) != 0U;
}

uint32_t aixos_isr_nesting_level(void)
{
    return __atomic_load_n(&isr_nesting, __ATOMIC_SEQ_CST);
}

uint32_t aixos_isr_nesting_high_watermark(void)
{
    return __atomic_load_n(&isr_nesting_high_watermark, __ATOMIC_SEQ_CST);
}

uint32_t aixos_isr_nesting_overflow_count(void)
{
    return __atomic_load_n(&isr_nesting_overflows, __ATOMIC_SEQ_CST);
}

void aixos_isr_stats_reset(void)
{
    if (__atomic_load_n(&isr_nesting, __ATOMIC_SEQ_CST) == 0U) {
        __atomic_store_n(&isr_nesting_high_watermark, 0U, __ATOMIC_SEQ_CST);
        __atomic_store_n(&isr_nesting_overflows, 0U, __ATOMIC_SEQ_CST);
    }
}
