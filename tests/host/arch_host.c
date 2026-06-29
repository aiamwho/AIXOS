#include <stdint.h>
#include "aixos/arch/arch.h"

static unsigned int switch_requests;
static unsigned int mpu_apply_count;

aixos_arch_flags_t aixos_arch_int_disable(void)
{
    return 0;
}

void aixos_arch_int_restore(aixos_arch_flags_t flags)
{
    (void)flags;
}

void *aixos_arch_stack_init(void (*entry)(void *), void *stack_top, void *arg,
                            int user_mode)
{
    (void)entry;
    (void)arg;
    (void)user_mode;
    return stack_top;
}

void aixos_arch_context_switch(void)
{
    switch_requests++;
}

void aixos_test_switch_requests_reset(void)
{
    switch_requests = 0U;
}

unsigned int aixos_test_switch_requests_get(void)
{
    return switch_requests;
}

void aixos_arch_start_first_task(void)
{
#ifdef AIXOS_HOST_TEST
    return;
#else
    for (;;) {
    }
#endif
}

void aixos_arch_tick_handler(void)
{
}

void aixos_arch_system_init(void)
{
}

void aixos_arch_systick_enable(void)
{
}

void aixos_arch_mpu_configure_task(const struct aixos_tcb *task)
{
    (void)task;
    mpu_apply_count++;
}

void aixos_test_mpu_apply_count_reset(void)
{
    mpu_apply_count = 0U;
}

unsigned int aixos_test_mpu_apply_count_get(void)
{
    return mpu_apply_count;
}
