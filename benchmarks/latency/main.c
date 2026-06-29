#include <stdint.h>
#include "aixos/aixos.h"
#include "aixos/namespace.h"
#include "aixos/arch/arch.h"
#include "kernel/timewheel.h"

#define LATENCY_STACK_SIZE 512U

volatile uint32_t latency_heartbeat;
volatile uint32_t latency_errors;
volatile uint32_t latency_context_switch_samples;
volatile uint32_t latency_sem_roundtrip_samples;
volatile uint32_t latency_mq_roundtrip_samples;
volatile uint32_t latency_timer_samples;
volatile uint32_t test_heartbeat;

static aixos_handle_t latency_sem;
static aixos_handle_t latency_mq;
static aixos_handle_t latency_timer;

static aixos_tcb_t producer_tcb;
static aixos_tcb_t consumer_tcb;
static uint8_t producer_stack[LATENCY_STACK_SIZE] __attribute__((aligned(8)));
static uint8_t consumer_stack[LATENCY_STACK_SIZE] __attribute__((aligned(8)));

static void latency_timer_callback(void *arg)
{
    (void)arg;
    latency_timer_samples++;
}

static void producer_task(void *arg)
{
    uint32_t value = 0U;
    (void)arg;

    for (;;) {
        value++;
        latency_heartbeat++;
        test_heartbeat++;
        latency_context_switch_samples++;

        if (aixos_sem_post(latency_sem) != AIXOS_OK) {
            latency_errors++;
        }
        if (aixos_mq_send(latency_mq, &value, sizeof(value), 0U) != AIXOS_OK) {
            latency_errors++;
        }

        (void)aixos_task_sleep(1U);
    }
}

static void consumer_task(void *arg)
{
    uint32_t value;
    size_t size;
    (void)arg;

    for (;;) {
        if (aixos_sem_wait(latency_sem, UINT32_MAX) == AIXOS_OK) {
            latency_sem_roundtrip_samples++;
        } else {
            latency_errors++;
        }

        if (aixos_mq_recv(latency_mq, &value, sizeof(value), &size,
                          UINT32_MAX) == AIXOS_OK &&
            size == sizeof(value)) {
            latency_mq_roundtrip_samples++;
        } else {
            latency_errors++;
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

    latency_sem = aixos_sem_create(0);
    latency_mq = aixos_mq_create(4U, sizeof(uint32_t));
    latency_timer = aixos_timer_create("lat", AIXOS_TIMER_PERIODIC,
                                       latency_timer_callback, NULL);

    if (latency_sem == AIXOS_HANDLE_INVALID ||
        latency_mq == AIXOS_HANDLE_INVALID ||
        latency_timer == AIXOS_HANDLE_INVALID ||
        aixos_timer_start(latency_timer, 1U) != AIXOS_OK ||
        aixos_task_create_static("lat-p", producer_task, NULL,
                                 producer_stack, sizeof(producer_stack),
                                 3, &producer_tcb) == AIXOS_HANDLE_INVALID ||
        aixos_task_create_static("lat-c", consumer_task, NULL,
                                 consumer_stack, sizeof(consumer_stack),
                                 2, &consumer_tcb) == AIXOS_HANDLE_INVALID) {
        latency_errors++;
        for (;;) {
        }
    }

    aixos_arch_system_init();
    aixos_start();
}
