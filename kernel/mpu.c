#include "aixos/mpu.h"
#include "aixos/task.h"
#include "aixos/arch/arch.h"
#include "kernel/sched.h"

static int is_power_of_two(uintptr_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

int aixos_mpu_region_valid(uintptr_t base, size_t size, uint32_t attr)
{
    if (attr == 0U || (attr & ~(AIXOS_MPU_READ | AIXOS_MPU_WRITE |
                                AIXOS_MPU_EXEC | AIXOS_MPU_DEVICE)) != 0U) {
        return 0;
    }
    if (size < AIXOS_CFG_MPU_MIN_REGION_SIZE ||
        size > UINT32_MAX || !is_power_of_two((uintptr_t)size)) {
        return 0;
    }
    if (size > UINTPTR_MAX - base) {
        return 0;
    }
    if ((base & ((uintptr_t)size - 1U)) != 0U) {
        return 0;
    }
    if ((attr & AIXOS_MPU_WRITE) != 0U && (attr & AIXOS_MPU_READ) == 0U) {
        return 0;
    }
    return 1;
}

void aixos_mpu_task_init(aixos_tcb_t *tcb)
{
    if (tcb == NULL) {
        return;
    }
    tcb->mpu_region_count = 0U;
#if AIXOS_CFG_ENABLE_MPU
    if (tcb->domain == AIXOS_DOMAIN_USER &&
        aixos_mpu_region_valid((uintptr_t)tcb->stack_base, tcb->stack_size,
                               AIXOS_MPU_READ | AIXOS_MPU_WRITE)) {
        tcb->mpu_regions[0].base = (uintptr_t)tcb->stack_base;
        tcb->mpu_regions[0].size = tcb->stack_size;
        tcb->mpu_regions[0].attr = AIXOS_MPU_READ | AIXOS_MPU_WRITE;
        tcb->mpu_region_count = 1U;
    }
#endif
}

int aixos_task_mpu_region_add(aixos_handle_t task, uintptr_t base,
                              size_t size, uint32_t attr)
{
#if AIXOS_CFG_ENABLE_MPU
    aixos_tcb_t *tcb;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    if (!aixos_mpu_region_valid(base, size, attr)) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    tcb = aixos_tcb_from_handle(task);
    if (tcb == NULL || tcb->domain != AIXOS_DOMAIN_USER) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    if (tcb->mpu_region_count >= AIXOS_CFG_MPU_REGIONS_PER_TASK) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_OVERFLOW;
    }
    tcb->mpu_regions[tcb->mpu_region_count].base = base;
    tcb->mpu_regions[tcb->mpu_region_count].size = (uint32_t)size;
    tcb->mpu_regions[tcb->mpu_region_count].attr = attr;
    tcb->mpu_region_count++;
    if (tcb == g_cur_task) {
        aixos_arch_mpu_configure_task(tcb);
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
#else
    (void)task;
    (void)base;
    (void)size;
    (void)attr;
    return AIXOS_OK;
#endif
}

int aixos_mpu_task_allows(const aixos_tcb_t *tcb, uintptr_t base,
                          size_t size, int write_access, int execute_access)
{
    uint32_t i;
    uintptr_t end;
    if (tcb == NULL || tcb->domain != AIXOS_DOMAIN_USER || size == 0U ||
        size > UINTPTR_MAX - base) {
        return 0;
    }
    end = base + size;
    for (i = 0U; i < tcb->mpu_region_count; i++) {
        const aixos_mpu_region_t *region = &tcb->mpu_regions[i];
        uintptr_t region_end = region->base + region->size;
        if (base < region->base || end > region_end) {
            continue;
        }
        if (write_access != 0 && (region->attr & AIXOS_MPU_WRITE) == 0U) {
            return 0;
        }
        if (execute_access != 0 && (region->attr & AIXOS_MPU_EXEC) == 0U) {
            return 0;
        }
        if (write_access == 0 && execute_access == 0 &&
            (region->attr & AIXOS_MPU_READ) == 0U) {
            return 0;
        }
        return 1;
    }
    return 0;
}
