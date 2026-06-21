#include "aixos/arch/arch.h"
#include "aixos/types.h"

static volatile uint32_t isr_nesting;
static volatile uint32_t reschedule_pending;
static volatile uint32_t scheduler_lock_nesting;

void aixos_isr_enter(void)
{
    isr_nesting++;
}

void aixos_isr_exit(void)
{
    if (isr_nesting == 0U) {
        return;
    }
    isr_nesting--;
    if (isr_nesting == 0U && scheduler_lock_nesting == 0U &&
        reschedule_pending != 0U) {
        reschedule_pending = 0U;
        aixos_arch_context_switch();
    }
}

int aixos_in_isr(void)
{
    return isr_nesting != 0U;
}

void aixos_reschedule_request(void)
{
    if (isr_nesting != 0U || scheduler_lock_nesting != 0U) {
        reschedule_pending = 1U;
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
    scheduler_lock_nesting++;
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
    if (scheduler_lock_nesting == 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    scheduler_lock_nesting--;
    if (scheduler_lock_nesting == 0U && reschedule_pending != 0U) {
        reschedule_pending = 0U;
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
    return scheduler_lock_nesting;
}

int aixos_sched_is_locked(void)
{
    return scheduler_lock_nesting != 0U;
}
