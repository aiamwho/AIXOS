#include <stdint.h>
#include "aixos/aixos.h"
#include "aixos/namespace.h"
#include "aixos/arch/arch.h"
#include "kernel/timewheel.h"

volatile uint32_t aixos_bench_heartbeat;
volatile uint32_t aixos_bench_messages;
volatile uint32_t aixos_bench_errors;

static aixos_handle_t benchmark_queue;

#if defined(__riscv)
static void register_stress(void)
{
    register uint32_t s2 __asm("s2") = UINT32_C(0x11223344);
    register uint32_t s3 __asm("s3") = UINT32_C(0x55667788);
    register uint32_t s4 __asm("s4") = UINT32_C(0xA5A55A5A);

    __asm volatile("" : "+r"(s2), "+r"(s3), "+r"(s4));
    (void)aixos_task_sleep(1U);
    __asm volatile("" : "+r"(s2), "+r"(s3), "+r"(s4));
    if (s2 != UINT32_C(0x11223344) ||
        s3 != UINT32_C(0x55667788) ||
        s4 != UINT32_C(0xA5A55A5A)) {
        aixos_bench_errors++;
    }
}
#endif

static void producer_task(void *arg)
{
    uint32_t value = 0U;
    (void)arg;
    for (;;) {
        value++;
        aixos_bench_heartbeat++;
        if (aixos_mq_send(benchmark_queue, &value, sizeof(value), 0U) !=
            AIXOS_OK) {
            aixos_bench_errors++;
        }
#if defined(__riscv)
        register_stress();
#endif
        (void)aixos_task_sleep(10U);
    }
}

static void consumer_task(void *arg)
{
    uint32_t value;
    size_t size;
    (void)arg;
    for (;;) {
        if (aixos_mq_recv(benchmark_queue, &value, sizeof(value), &size,
                          UINT32_MAX) == AIXOS_OK &&
            size == sizeof(value)) {
            aixos_bench_messages++;
        } else {
            aixos_bench_errors++;
        }
    }
}

int main(void)
{
    aixos_heap_init(NULL, 0U);
    aixos_object_init();
    aixos_task_init();
    aixos_trace_init();
    aixos_timer_init();
#if AIXOS_CFG_ENABLE_NAMESPACE
    aixos_namespace_init();
#endif
#if AIXOS_CFG_ENABLE_TIME_WHEEL
    aixos_timing_wheel_init();
#endif
    aixos_sched_init();

    benchmark_queue = aixos_mq_create(4U, sizeof(uint32_t));
    if (benchmark_queue == AIXOS_HANDLE_INVALID ||
        aixos_task_create("producer", producer_task, NULL, 512U, 3) ==
            AIXOS_HANDLE_INVALID ||
        aixos_task_create("consumer", consumer_task, NULL, 512U, 2) ==
            AIXOS_HANDLE_INVALID) {
        aixos_bench_errors++;
        for (;;) {
        }
    }
    aixos_arch_system_init();
    aixos_start();
}
