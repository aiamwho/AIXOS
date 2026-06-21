/*
 * AIXOS Hello World application.
 *
 * Board projects can override aixos_board_uart_init() and
 * aixos_board_uart_putc() in their BSP to send messages to a real UART.
 */

#include <stddef.h>
#include <stdint.h>

#include "aixos/aixos.h"
#include "aixos/namespace.h"
#include "arch/include/aixos/arch/arch.h"
#include "kernel/timewheel.h"

#define HELLO_STACK_SIZE 512U

volatile uint32_t hello_world_heartbeat = 0U;
volatile uint32_t test_heartbeat = 0U;

static aixos_tcb_t hello_tcb;
static uint8_t hello_stack[HELLO_STACK_SIZE] __attribute__((aligned(8)));

void __attribute__((weak)) aixos_board_uart_init(void)
{
    /*
     * Customer BSP example:
     * - enable GPIO and USART clocks
     * - configure TX pin alternate function
     * - program baud rate, data bits, stop bits, and TX enable
     */
}

void __attribute__((weak)) aixos_board_uart_putc(char c)
{
    (void)c;
    /*
     * Customer BSP example:
     * - wait until UART TX register is empty
     * - write c to the TX data register
     */
}

static void hello_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            aixos_board_uart_putc('\r');
        }
        aixos_board_uart_putc(*text);
        text++;
    }
}

static void hello_task(void *arg)
{
    (void)arg;

    hello_write("AIXOS hello world task started\n");

    for (;;) {
        hello_world_heartbeat++;
        test_heartbeat++;
        hello_write("hello from AIXOS\n");
        aixos_task_sleep(1000U);
    }
}

int main(void)
{
    aixos_board_uart_init();

    aixos_heap_init(NULL, 0);
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

    (void)aixos_task_create_static("hello", hello_task, NULL,
                                   hello_stack, sizeof(hello_stack),
                                   3, &hello_tcb);

    aixos_arch_system_init();
    aixos_start();
}
