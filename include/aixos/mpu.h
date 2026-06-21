#ifndef AIXOS_MPU_H
#define AIXOS_MPU_H

#include "aixos/types.h"
#include "config/aixos_cfg.h"

#define AIXOS_MPU_READ     (1U << 0)
#define AIXOS_MPU_WRITE    (1U << 1)
#define AIXOS_MPU_EXEC     (1U << 2)
#define AIXOS_MPU_DEVICE   (1U << 3)

typedef struct {
    uintptr_t base;
    uint32_t size;
    uint32_t attr;
} aixos_mpu_region_t;

struct aixos_tcb;

int aixos_task_mpu_region_add(aixos_handle_t task, uintptr_t base,
                              size_t size, uint32_t attr);
int aixos_mpu_region_valid(uintptr_t base, size_t size, uint32_t attr);
void aixos_mpu_task_init(struct aixos_tcb *tcb);
int aixos_mpu_task_allows(const struct aixos_tcb *tcb, uintptr_t base,
                          size_t size, int write_access, int execute_access);

#endif /* AIXOS_MPU_H */
