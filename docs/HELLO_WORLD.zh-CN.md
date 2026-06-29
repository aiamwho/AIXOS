# AIXOS v1.0 Hello World

本文展示最小客户应用模式：

- 初始化板级 UART；
- 初始化 AIXOS；
- 创建一个静态任务；
- 每秒打印一次消息；
- 暴露 heartbeat 符号，便于仿真器或调试器检查。

源码：

```text
examples/hello_world/main.c
```

## 构建

Cortex-M3：

```sh
make arm APP_SRCS=examples/hello_world/main.c
```

RV32IM 默认前缀：

```sh
make riscv APP_SRCS=examples/hello_world/main.c
```

RV32IM 使用 `riscv64-elf-` 前缀：

```sh
make riscv APP_SRCS=examples/hello_world/main.c RISCV_PREFIX=riscv64-elf-
```

## 应用结构

示例使用静态任务存储：

```c
static aixos_tcb_t hello_tcb;
static uint8_t hello_stack[512] __attribute__((aligned(8)));
```

并创建一个任务：

```c
aixos_task_create_static("hello", hello_task, NULL,
                         hello_stack, sizeof(hello_stack),
                         3, &hello_tcb);
```

任务打印一行并 sleep：

```c
for (;;) {
    hello_world_heartbeat++;
    hello_write("hello from AIXOS\n");
    aixos_task_sleep(1000U);
}
```

`hello_world_heartbeat` 可在调试器或仿真器中观察，用于确认调度器和 tick 正在运行。示例还会递增 `test_heartbeat`，使现有 RISC-V ELF validation 目标能识别该镜像为可运行固件样例。

## UART 输出 Hook

AIXOS 不假设固定 UART 外设。Hello World 示例提供 weak board hooks：

```c
void aixos_board_uart_init(void);
void aixos_board_uart_putc(char c);
```

默认 weak 实现为空，因此示例可在所有支持目标上构建。客户 BSP 应覆盖这两个函数。

典型 STM32F103 风格职责：

- 使能 GPIOA 和 USART1 时钟。
- 配置 PA9 为 USART1_TX alternate function push-pull。
- 配置波特率、8 数据位、无奇偶、1 停止位。
- 使能 USART transmitter。

典型 memory-mapped UART 输出：

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

生产代码应保持 UART polling 有界，或在 BSP 就绪后切到 buffered driver task。

## 预期输出

实现 UART hook 后，串口应显示：

```text
AIXOS hello world task started
hello from AIXOS
hello from AIXOS
hello from AIXOS
```

## 下一步

Hello World 跑通后，切换到 `examples/smoke/main.c`，验证 semaphore、message queue、event flag、pipe、timer、user task 和 MPU region。
