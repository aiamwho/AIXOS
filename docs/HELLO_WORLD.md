# AIXOS v1.0 Hello World

This guide shows the smallest customer-facing application pattern:

- initialize the board UART,
- initialize AIXOS,
- create one static task,
- print a message once per second,
- expose a heartbeat symbol for simulator or debugger checks.

The source is:

```text
examples/hello_world/main.c
```

## Build

Cortex-M3:

```sh
make arm APP_SRCS=examples/hello_world/main.c
```

RV32IM with the default toolchain prefix:

```sh
make riscv APP_SRCS=examples/hello_world/main.c
```

RV32IM when the local compiler uses the `riscv64-elf-` prefix:

```sh
make riscv APP_SRCS=examples/hello_world/main.c RISCV_PREFIX=riscv64-elf-
```

## Application Structure

The example uses static task storage:

```c
static aixos_tcb_t hello_tcb;
static uint8_t hello_stack[512] __attribute__((aligned(8)));
```

and creates one task:

```c
aixos_task_create_static("hello", hello_task, NULL,
                         hello_stack, sizeof(hello_stack),
                         3, &hello_tcb);
```

The task prints a line and then sleeps:

```c
for (;;) {
    hello_world_heartbeat++;
    hello_write("hello from AIXOS\n");
    aixos_task_sleep(1000U);
}
```

`hello_world_heartbeat` can be watched in a debugger or simulator to confirm
that the scheduler and tick are running. The example also increments
`test_heartbeat` so the existing RISC-V ELF validation target can recognize
the image as a runnable firmware sample.

## UART Output Hook

AIXOS does not assume one universal UART peripheral. The Hello World example
therefore provides weak board hooks:

```c
void aixos_board_uart_init(void);
void aixos_board_uart_putc(char c);
```

The default weak implementations do nothing, so the example builds on every
supported target. A customer BSP should override both functions.

Typical STM32F103-style responsibilities:

```c
void aixos_board_uart_init(void)
{
    /* Enable GPIOA and USART1 clocks. */
    /* Configure PA9 as USART1_TX alternate function push-pull. */
    /* Configure baud rate, 8 data bits, no parity, 1 stop bit. */
    /* Enable USART transmitter. */
}

void aixos_board_uart_putc(char c)
{
    /* Wait for TX empty. */
    /* Write c to USART data register. */
}
```

Typical memory-mapped UART responsibilities:

```c
#define UART_BASE   0x40013800U
#define UART_SR     (*(volatile uint32_t *)(UART_BASE + 0x00U))
#define UART_DR     (*(volatile uint32_t *)(UART_BASE + 0x04U))
#define UART_TXE    (1U << 7)

void aixos_board_uart_putc(char c)
{
    while ((UART_SR & UART_TXE) == 0U) {
    }
    UART_DR = (uint32_t)(uint8_t)c;
}
```

Keep UART polling bounded for production code, or move logging to a buffered
driver task once the board support package is ready.

## Expected Output

With UART hooks implemented, the serial console should show:

```text
AIXOS hello world task started
hello from AIXOS
hello from AIXOS
hello from AIXOS
```

## Next Step

After Hello World runs, move to `examples/smoke/main.c` to exercise semaphores,
message queues, event flags, pipes, timers, user tasks, and MPU regions.
