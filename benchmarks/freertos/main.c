#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

volatile uint32_t freertos_heartbeat;
volatile uint32_t freertos_messages;
volatile uint32_t freertos_ticks;
volatile uint32_t freertos_switches;
volatile uint32_t freertos_errors;

static QueueHandle_t benchmark_queue;

#if defined(__riscv)
static void register_stress(void)
{
    register uint32_t s2 __asm("s2") = UINT32_C(0x11223344);
    register uint32_t s3 __asm("s3") = UINT32_C(0x55667788);
    register uint32_t s4 __asm("s4") = UINT32_C(0xA5A55A5A);

    __asm volatile("" : "+r"(s2), "+r"(s3), "+r"(s4));
    vTaskDelay(1U);
    __asm volatile("" : "+r"(s2), "+r"(s3), "+r"(s4));
    if (s2 != UINT32_C(0x11223344) ||
        s3 != UINT32_C(0x55667788) ||
        s4 != UINT32_C(0xA5A55A5A)) {
        freertos_errors++;
    }
}
#endif

void freertos_trace_switch(void)
{
    freertos_switches++;
}

void freertos_assert_failed(const char *file, int line)
{
    (void)file;
    (void)line;
    freertos_errors++;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationTickHook(void)
{
    freertos_ticks++;
}

static void producer_task(void *arg)
{
    uint32_t value = 0U;
    (void)arg;
    for (;;) {
        value++;
        freertos_heartbeat++;
        if (xQueueSend(benchmark_queue, &value, 0U) != pdPASS) {
            freertos_errors++;
        }
#if defined(__riscv)
        register_stress();
#endif
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

static void consumer_task(void *arg)
{
    uint32_t value;
    (void)arg;
    for (;;) {
        if (xQueueReceive(benchmark_queue, &value, portMAX_DELAY) == pdPASS) {
            freertos_messages++;
        } else {
            freertos_errors++;
        }
    }
}

int main(void)
{
    benchmark_queue = xQueueCreate(4U, sizeof(uint32_t));
    if (benchmark_queue == NULL ||
        xTaskCreate(producer_task, "producer", 128U, NULL, 3U, NULL) != pdPASS ||
        xTaskCreate(consumer_task, "consumer", 128U, NULL, 2U, NULL) != pdPASS) {
        freertos_errors++;
        for (;;) {
        }
    }
    vTaskStartScheduler();
    freertos_errors++;
    for (;;) {
    }
}
