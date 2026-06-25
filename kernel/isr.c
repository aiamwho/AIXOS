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

#if AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M0 || defined(__riscv)
static uint32_t atomic_load_u32(volatile uint32_t *value)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t result = *value;
    aixos_arch_int_restore(flags);
    return result;
}

static void atomic_store_u32(volatile uint32_t *value, uint32_t desired)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    *value = desired;
    aixos_arch_int_restore(flags);
}

static uint32_t atomic_add_fetch_u32(volatile uint32_t *value, uint32_t addend)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t result = *value + addend;
    *value = result;
    aixos_arch_int_restore(flags);
    return result;
}

static uint32_t atomic_sub_fetch_u32(volatile uint32_t *value, uint32_t subtrahend)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t result = *value - subtrahend;
    *value = result;
    aixos_arch_int_restore(flags);
    return result;
}

static uint32_t atomic_fetch_add_u32(volatile uint32_t *value, uint32_t addend)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t previous = *value;
    *value = previous + addend;
    aixos_arch_int_restore(flags);
    return previous;
}

static uint32_t atomic_exchange_u32(volatile uint32_t *value, uint32_t desired)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t previous = *value;
    *value = desired;
    aixos_arch_int_restore(flags);
    return previous;
}

static int atomic_compare_exchange_u32(volatile uint32_t *value,
                                       uint32_t *expected,
                                       uint32_t desired)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t current = *value;
    int matched = current == *expected;
    if (matched) {
        *value = desired;
    } else {
        *expected = current;
    }
    aixos_arch_int_restore(flags);
    return matched;
}

#define AIXOS_ATOMIC_LOAD(value, order) \
    atomic_load_u32((value))
#define AIXOS_ATOMIC_STORE(value, desired, order) \
    atomic_store_u32((value), (desired))
#define AIXOS_ATOMIC_ADD_FETCH(value, addend, order) \
    atomic_add_fetch_u32((value), (addend))
#define AIXOS_ATOMIC_SUB_FETCH(value, subtrahend, order) \
    atomic_sub_fetch_u32((value), (subtrahend))
#define AIXOS_ATOMIC_FETCH_ADD(value, addend, order) \
    atomic_fetch_add_u32((value), (addend))
#define AIXOS_ATOMIC_EXCHANGE(value, desired, order) \
    atomic_exchange_u32((value), (desired))
#define AIXOS_ATOMIC_COMPARE_EXCHANGE(value, expected, desired, weak, success, failure) \
    atomic_compare_exchange_u32((value), (expected), (desired))
#else
#define AIXOS_ATOMIC_LOAD(value, order) \
    __atomic_load_n((value), (order))
#define AIXOS_ATOMIC_STORE(value, desired, order) \
    __atomic_store_n((value), (desired), (order))
#define AIXOS_ATOMIC_ADD_FETCH(value, addend, order) \
    __atomic_add_fetch((value), (addend), (order))
#define AIXOS_ATOMIC_SUB_FETCH(value, subtrahend, order) \
    __atomic_sub_fetch((value), (subtrahend), (order))
#define AIXOS_ATOMIC_FETCH_ADD(value, addend, order) \
    __atomic_fetch_add((value), (addend), (order))
#define AIXOS_ATOMIC_EXCHANGE(value, desired, order) \
    __atomic_exchange_n((value), (desired), (order))
#define AIXOS_ATOMIC_COMPARE_EXCHANGE(value, expected, desired, weak, success, failure) \
    __atomic_compare_exchange_n((value), (expected), (desired), (weak), \
                                (success), (failure))
#endif

static void isr_update_high_watermark(uint32_t level)
{
    uint32_t high = AIXOS_ATOMIC_LOAD(&isr_nesting_high_watermark,
                                      __ATOMIC_RELAXED);
    while (level > high &&
           !AIXOS_ATOMIC_COMPARE_EXCHANGE(&isr_nesting_high_watermark, &high,
                                          level, 0, __ATOMIC_RELAXED,
                                          __ATOMIC_RELAXED)) {
    }
}

static void isr_record_nesting_overflow(uint32_t level)
{
    uint32_t previous = AIXOS_ATOMIC_FETCH_ADD(&isr_nesting_overflows, 1U,
                                               __ATOMIC_RELAXED);
    if (previous == 0U) {
        aixos_crash_record_store_extended(
            0U, AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW, 0U, 0U, 0U,
            level, AIXOS_CFG_ISR_NESTING_MAX,
            AIXOS_ATOMIC_LOAD(&isr_nesting_high_watermark, __ATOMIC_RELAXED));
    }
#if AIXOS_CFG_ISR_NESTING_PANIC
    aixos_panic("ISR nesting overflow", AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW);
#endif
}

void aixos_isr_enter(void)
{
    uint32_t level = AIXOS_ATOMIC_ADD_FETCH(&isr_nesting, 1U,
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
        old_level = AIXOS_ATOMIC_LOAD(&isr_nesting, __ATOMIC_SEQ_CST);
        if (old_level == 0U) {
            return;
        }
        new_level = old_level - 1U;
    } while (!AIXOS_ATOMIC_COMPARE_EXCHANGE(&isr_nesting, &old_level,
                                            new_level, 0, __ATOMIC_SEQ_CST,
                                            __ATOMIC_SEQ_CST));

    if (new_level == 0U &&
        AIXOS_ATOMIC_LOAD(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) == 0U &&
        AIXOS_ATOMIC_EXCHANGE(&reschedule_pending, 0U, __ATOMIC_SEQ_CST) != 0U) {
        aixos_arch_context_switch();
    }
}

int aixos_in_isr(void)
{
    return AIXOS_ATOMIC_LOAD(&isr_nesting, __ATOMIC_SEQ_CST) != 0U;
}

void aixos_reschedule_request(void)
{
    if (AIXOS_ATOMIC_LOAD(&isr_nesting, __ATOMIC_SEQ_CST) != 0U ||
        AIXOS_ATOMIC_LOAD(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) != 0U) {
        AIXOS_ATOMIC_STORE(&reschedule_pending, 1U, __ATOMIC_SEQ_CST);
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
    (void)AIXOS_ATOMIC_ADD_FETCH(&scheduler_lock_nesting, 1U,
                                 __ATOMIC_SEQ_CST);
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
    if (AIXOS_ATOMIC_LOAD(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (AIXOS_ATOMIC_SUB_FETCH(&scheduler_lock_nesting, 1U,
                               __ATOMIC_SEQ_CST) == 0U &&
        AIXOS_ATOMIC_EXCHANGE(&reschedule_pending, 0U,
                              __ATOMIC_SEQ_CST) != 0U) {
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
    return AIXOS_ATOMIC_LOAD(&scheduler_lock_nesting, __ATOMIC_SEQ_CST);
}

int aixos_sched_is_locked(void)
{
    return AIXOS_ATOMIC_LOAD(&scheduler_lock_nesting, __ATOMIC_SEQ_CST) != 0U;
}

uint32_t aixos_isr_nesting_level(void)
{
    return AIXOS_ATOMIC_LOAD(&isr_nesting, __ATOMIC_SEQ_CST);
}

uint32_t aixos_isr_nesting_high_watermark(void)
{
    return AIXOS_ATOMIC_LOAD(&isr_nesting_high_watermark, __ATOMIC_SEQ_CST);
}

uint32_t aixos_isr_nesting_overflow_count(void)
{
    return AIXOS_ATOMIC_LOAD(&isr_nesting_overflows, __ATOMIC_SEQ_CST);
}

void aixos_isr_stats_reset(void)
{
    if (AIXOS_ATOMIC_LOAD(&isr_nesting, __ATOMIC_SEQ_CST) == 0U) {
        AIXOS_ATOMIC_STORE(&isr_nesting_high_watermark, 0U, __ATOMIC_SEQ_CST);
        AIXOS_ATOMIC_STORE(&isr_nesting_overflows, 0U, __ATOMIC_SEQ_CST);
    }
}
