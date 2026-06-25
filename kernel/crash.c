#include "aixos/crash.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"
#include "aixos/version.h"
#include "aixos/arch/arch.h"
#include <stddef.h>

#ifdef AIXOS_HOST_TEST
static volatile aixos_crash_record_t crash_record;
#else
static volatile aixos_crash_record_t crash_record
    __attribute__((section(".noinit.crash"), used));
#endif

static uint32_t crash_crc32(const aixos_crash_record_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    uint32_t crc = UINT32_MAX;
    size_t i;
    size_t j;
    for (i = offsetof(aixos_crash_record_t, version);
         i < offsetof(aixos_crash_record_t, crc32); i++) {
        crc ^= bytes[i];
        for (j = 0U; j < 8U; j++) {
            crc = (crc >> 1U) ^
                  (UINT32_C(0xEDB88320) & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return ~crc;
}

int aixos_crash_record_validate(const aixos_crash_record_t *record)
{
    if (record == NULL || record->magic != AIXOS_CFG_CRASH_MAGIC ||
        record->version != AIXOS_CRASH_RECORD_VERSION ||
        record->size != sizeof(*record)) {
        return AIXOS_ERR_CORRUPT;
    }
    return record->crc32 == crash_crc32(record) ? AIXOS_OK :
           AIXOS_ERR_CORRUPT;
}

void aixos_crash_record_store_extended(uint32_t architecture, uint32_t reason,
                                       uint32_t program_counter,
                                       uint32_t fault_address,
                                       uint32_t stack_pointer,
                                       uint32_t fault_status,
                                       uint32_t fault_status2,
                                       uint32_t auxiliary)
{
    aixos_crash_record_t next = {0};
    const aixos_crash_record_t *previous = aixos_crash_record_get();
    next.version = AIXOS_CRASH_RECORD_VERSION;
    next.size = (uint16_t)sizeof(next);
    next.sequence = previous != NULL ? previous->sequence + 1U : 1U;
    next.build_id = AIXOS_BUILD_ID;
    next.architecture = architecture;
    next.reason = reason;
    next.program_counter = program_counter;
    next.fault_address = fault_address;
    next.stack_pointer = stack_pointer;
    next.task_handle = g_cur_task != NULL ?
                       (uint32_t)g_cur_task->handle : UINT32_MAX;
    next.tick = aixos_get_tick();
    next.fault_status = fault_status;
    next.fault_status2 = fault_status2;
    next.auxiliary = auxiliary;
    next.crc32 = crash_crc32(&next);
    crash_record.magic = 0U;
    crash_record = next;
    crash_record.magic = AIXOS_CFG_CRASH_MAGIC;
}

void aixos_crash_record_store(uint32_t architecture, uint32_t reason,
                              uint32_t program_counter,
                              uint32_t fault_address,
                              uint32_t stack_pointer)
{
    aixos_crash_record_store_extended(architecture, reason, program_counter,
                                      fault_address, stack_pointer,
                                      0U, 0U, 0U);
}

const aixos_crash_record_t *aixos_crash_record_get(void)
{
    const aixos_crash_record_t *record =
        (const aixos_crash_record_t *)&crash_record;
    return aixos_crash_record_validate(record) == AIXOS_OK ? record : NULL;
}

void aixos_crash_record_clear(void)
{
    crash_record.magic = 0U;
}

void aixos_arm_fault_handler(uint32_t *frame, uint32_t reason)
{
    uint32_t pc = frame != NULL ? frame[6] : 0U;
#ifdef AIXOS_HOST_TEST
    uint32_t fault_address = 0U;
    uint32_t cfsr = 0U;
    uint32_t hfsr = 0U;
    uint32_t mmfar = 0U;
#elif AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M3 || \
      AIXOS_CFG_PLATFORM == AIXOS_CFG_PLATFORM_CORTEX_M4
    uint32_t fault_address = *(volatile uint32_t *)UINT32_C(0xE000ED38);
    uint32_t cfsr = *(volatile uint32_t *)UINT32_C(0xE000ED28);
    uint32_t hfsr = *(volatile uint32_t *)UINT32_C(0xE000ED2C);
    uint32_t mmfar = *(volatile uint32_t *)UINT32_C(0xE000ED34);
#else
    uint32_t fault_address = 0U;
    uint32_t cfsr = 0U;
    uint32_t hfsr = 0U;
    uint32_t mmfar = 0U;
#endif
    /* Controlled reset after recording crash information. */
    aixos_crash_record_store_extended(1U, reason, pc, fault_address,
                                      (uint32_t)(uintptr_t)frame,
                                      cfsr, hfsr, mmfar);
    /* 短暂延迟确保存储完成 */
    for (volatile int i = 0; i < 100000; i++) {
        __asm volatile("nop");
    }
#if AIXOS_CFG_ENABLE_PANIC_RESET
    aixos_system_reset();
#else
    for (;;) {
        __asm volatile("wfi");
    }
#endif
    for (;;) { __asm volatile("wfi"); } /* unreachable */
}

/* Controlled system reset. */
void aixos_system_reset(void)
{
    /* 尝试触发软件复位 */
#if defined(__aarch64__)
    __asm volatile("wfi");
#elif defined(__arm__) || defined(__thumb__)
    /* ARM: 使用 SCB AIRCR 寄存器 */
    volatile uint32_t *aircr = (volatile uint32_t *)UINT32_C(0xE000ED0C);
    *aircr = 0x05FA0004U;  /* VECTKEY + SYSRESETREQ */
#elif defined(__riscv)
    /* RISC-V: 通常由 SoC 特定的复位寄存器控制 */
    /* 如果支持，读取 mcause 并执行 wfi 等待复位 */
    __asm volatile("wfi");
#endif
    /* 如果复位失败，死循环 */
    for (;;) {
        __asm volatile("wfi");
    }
}

/* Kernel panic path. */
void aixos_panic(const char *msg, uint32_t reason)
{
    (void)msg;
    /* 禁用所有中断 */
    (void)aixos_arch_int_disable();
    /* 记录崩溃 */
    aixos_crash_record_store(
        0U, reason, 0U, 0U, 0U);
    /* 延迟等待记录完成 */
    for (volatile int i = 0; i < 1000; i++) {
        __asm volatile("nop");
    }
#if AIXOS_CFG_ENABLE_PANIC_RESET
    aixos_system_reset();
#else
    for (;;) {
        __asm volatile("wfi");
    }
#endif
    for (;;) { __asm volatile("wfi"); } /* unreachable */
}
