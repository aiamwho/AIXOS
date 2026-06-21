#include "test.h"
#include "aixos/aixos.h"
#include "aixos/mpu.h"
#include "kernel/sched.h"

extern void aixos_test_mpu_apply_count_reset(void);
extern unsigned int aixos_test_mpu_apply_count_get(void);

static uint8_t user_stack[256] __attribute__((aligned(256)));
static aixos_tcb_t user_tcb;
static uint8_t user_buffer[64] __attribute__((aligned(64)));
static uint8_t read_only_buffer[64] __attribute__((aligned(64)));
static uint8_t extra_buffer[64] __attribute__((aligned(64)));

static void mpu_dummy_task(void *arg)
{
    (void)arg;
}

void test_mpu_regions(void)
{
    aixos_handle_t user;
    aixos_handle_t kernel;
    aixos_tcb_t *tcb;

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();

    CHECK(aixos_mpu_region_valid((uintptr_t)user_buffer, sizeof(user_buffer),
                                 AIXOS_MPU_READ | AIXOS_MPU_WRITE) == 1);
    CHECK(aixos_mpu_region_valid((uintptr_t)user_buffer + 1U,
                                 sizeof(user_buffer),
                                 AIXOS_MPU_READ | AIXOS_MPU_WRITE) == 0);
    CHECK(aixos_mpu_region_valid((uintptr_t)user_buffer, 48U,
                                 AIXOS_MPU_READ | AIXOS_MPU_WRITE) == 0);
    CHECK(aixos_mpu_region_valid((uintptr_t)user_buffer, sizeof(user_buffer),
                                 AIXOS_MPU_WRITE) == 0);

    user = aixos_user_task_create_static("u", mpu_dummy_task, NULL,
                                         user_stack, sizeof(user_stack),
                                         2, &user_tcb);
    CHECK(user != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(user);
    CHECK(tcb != NULL);
    if (tcb == NULL) {
        return;
    }
    CHECK(tcb->mpu_region_count == 1U);
    CHECK(tcb->mpu_regions[0].base == (uintptr_t)user_stack);
    CHECK(tcb->mpu_regions[0].size == sizeof(user_stack));
    CHECK((tcb->mpu_regions[0].attr & AIXOS_MPU_WRITE) != 0U);

    aixos_test_set_current(tcb);
    CHECK(aixos_user_memory_check(user_stack + 16, 4, 1) == AIXOS_OK);
    CHECK(aixos_user_memory_check(user_buffer, 4, 1) == AIXOS_ERR_FAULT);

    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)user_buffer,
                                    sizeof(user_buffer),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_OK);
    CHECK(aixos_user_memory_check(user_buffer, 4, 1) == AIXOS_OK);

    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)read_only_buffer,
                                    sizeof(read_only_buffer),
                                    AIXOS_MPU_READ) == AIXOS_OK);
    CHECK(aixos_user_memory_check(read_only_buffer, 4, 0) == AIXOS_OK);
    CHECK(aixos_user_memory_check(read_only_buffer, 4, 1) == AIXOS_ERR_FAULT);
    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)extra_buffer,
                                    sizeof(extra_buffer),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_ERR_OVERFLOW);

    kernel = aixos_task_create("k", mpu_dummy_task, NULL, 256, 1);
    CHECK(kernel != AIXOS_HANDLE_INVALID);
    CHECK(aixos_task_mpu_region_add(kernel, (uintptr_t)extra_buffer,
                                    sizeof(extra_buffer),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_ERR_INVAL);

    aixos_sched_add_task(tcb);
    aixos_test_mpu_apply_count_reset();
    aixos_schedule();
    CHECK(aixos_test_mpu_apply_count_get() == 1U);
    CHECK(g_cur_task == tcb);

    aixos_test_set_current(NULL);
}

void test_user_copy_api(void)
{
    aixos_handle_t user;
    aixos_tcb_t *tcb;
    uint8_t kernel_src[8] = { 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U };
    uint8_t kernel_dst[8] = { 0U };

    aixos_heap_init(NULL, 0);
    aixos_object_init();
    aixos_task_init();
    aixos_sched_init();

    user = aixos_user_task_create_static("u", mpu_dummy_task, NULL,
                                         user_stack, sizeof(user_stack),
                                         2, &user_tcb);
    CHECK(user != AIXOS_HANDLE_INVALID);
    tcb = aixos_tcb_from_handle(user);
    CHECK(tcb != NULL);
    if (tcb == NULL) {
        return;
    }
    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)user_buffer,
                                    sizeof(user_buffer),
                                    AIXOS_MPU_READ | AIXOS_MPU_WRITE) ==
          AIXOS_OK);
    CHECK(aixos_task_mpu_region_add(user, (uintptr_t)read_only_buffer,
                                    sizeof(read_only_buffer),
                                    AIXOS_MPU_READ) == AIXOS_OK);

    user_buffer[0] = 9U;
    user_buffer[1] = 8U;
    user_buffer[2] = 7U;
    user_buffer[3] = 6U;
    read_only_buffer[0] = 3U;
    read_only_buffer[1] = 2U;

    aixos_test_set_current(tcb);
    CHECK(aixos_copy_from_user(kernel_dst, user_buffer, 4U) == AIXOS_OK);
    CHECK(kernel_dst[0] == 9U && kernel_dst[3] == 6U);
    CHECK(aixos_copy_to_user(user_buffer + 8U, kernel_src, 4U) == AIXOS_OK);
    CHECK(user_buffer[8] == 1U && user_buffer[11] == 4U);
    CHECK(aixos_zero_to_user(user_buffer + 8U, 4U) == AIXOS_OK);
    CHECK(user_buffer[8] == 0U && user_buffer[11] == 0U);

    CHECK(aixos_copy_from_user(kernel_dst, read_only_buffer, 2U) ==
          AIXOS_OK);
    CHECK(aixos_copy_to_user(read_only_buffer, kernel_src, 2U) ==
          AIXOS_ERR_FAULT);
    CHECK(aixos_zero_to_user(read_only_buffer, 2U) == AIXOS_ERR_FAULT);
    CHECK(aixos_copy_from_user(kernel_dst, extra_buffer, 2U) ==
          AIXOS_ERR_FAULT);
    CHECK(aixos_copy_to_user(extra_buffer, kernel_src, 2U) ==
          AIXOS_ERR_FAULT);
    CHECK(aixos_copy_from_user(NULL, user_buffer, 2U) == AIXOS_ERR_FAULT);
    CHECK(aixos_copy_to_user(user_buffer, NULL, 2U) == AIXOS_ERR_FAULT);
    CHECK(aixos_copy_from_user(kernel_dst, NULL, 0U) == AIXOS_OK);
    CHECK(aixos_copy_to_user(NULL, kernel_src, 0U) == AIXOS_OK);

    aixos_test_set_current(NULL);
}
