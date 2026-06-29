# AIXOS v1.0 集成指南

本文是客户侧把 AIXOS 集成到产品固件中的流程。假设客户拥有板级启动文件、产品 linker script、设备驱动和应用任务。

## 集成范围

AIXOS 应作为源码包使用。不要把生成的 `build/`、Renode 结果目录或本地工具报告复制进产品仓库。

最小源码集合：

| 路径 | 何时需要 |
|---|---|
| `include/aixos/` | 始终需要 |
| `config/aixos_cfg.h` | 始终需要，或替换为产品等价配置 |
| `kernel/` | 始终需要 |
| `kernel/ipc/` | 使用 IPC API 时需要 |
| `arch/include/` | 始终需要 |
| `arch/arm/cortex-m3/` | Cortex-M3 目标 |
| `arch/risc-v/` | RV32IM 目标 |
| `compat/posix/`, `posix/` | 需要 POSIX 兼容层时 |
| `examples/hello_world/` | 可选最小任务和 UART hook 示例 |
| `examples/smoke/` | 可选 smoke 应用 |
| `tests/`, `simulation/` | 只用于确认和回归 |

## 工具链设置

先运行工具盘点：

```sh
make toolcheck
```

默认 RISC-V 前缀是 `riscv-none-elf-`。如果本机使用其他前缀：

```sh
make riscv RISCV_PREFIX=riscv64-elf-
make riscv-validate RISCV_PREFIX=riscv64-elf-
```

如果工具链不在 `PATH`：

```sh
make riscv RISCV_TOOLCHAIN_DIR=/opt/xpack-riscv-none-elf-gcc
```

## 应用入口

包内 Makefile 默认构建 `APP_SRCS ?= examples/smoke/main.c`。产品固件应传入自己的入口：

```sh
make arm APP_SRCS=product/main.c
make riscv APP_SRCS=product/main.c RISCV_PREFIX=riscv64-elf-
```

新板卡 bring-up 可先使用最小 UART 示例：

```sh
make arm APP_SRCS=examples/hello_world/main.c
```

应用入口必须先初始化内核，再启动调度器。典型顺序：

```c
#include "aixos/aixos.h"
#include "aixos/namespace.h"
#include "kernel/timewheel.h"

int main(void)
{
    aixos_heap_init(product_heap, product_heap_size);
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

    /* 创建系统任务和应用任务。 */

    aixos_arch_system_init();
    aixos_start();
}
```

若启用了 `AIXOS_CFG_ENABLE_TIME_WHEEL`，必须在调度器启动前调用 `aixos_timing_wheel_init()`。跳过该步骤会破坏 timeout list，指令级仿真可能出现空地址访问。

## 静态和动态对象

生产固件优先使用静态创建和固定块池，这能让运行期行为更确定，并保持 `AIXOS_CFG_HEAP_LOCK_ON_START` 启用。

初始化期可使用动态创建：

```c
aixos_handle_t sem = aixos_sem_create(0);
aixos_handle_t task = aixos_task_create("worker", worker_entry, NULL, 512, 12);
```

长期对象推荐静态创建：

```c
static aixos_tcb_t worker_tcb;
static uint8_t worker_stack[512] __attribute__((aligned(8)));

aixos_handle_t worker = aixos_task_create_static("worker", worker_entry, NULL,
                                                 worker_stack,
                                                 sizeof(worker_stack),
                                                 12, &worker_tcb);
```

静态存储必须在对象删除或系统复位前保持有效。

## MPU 用户任务

当产品需要隔离应用代码和内核对象时，创建 user task：

```c
static aixos_tcb_t user_tcb;
static uint8_t user_stack[512] __attribute__((aligned(512)));
static uint8_t user_buffer[256] __attribute__((aligned(256)));

aixos_handle_t user = aixos_user_task_create_static("user", user_entry, NULL,
                                                    user_stack,
                                                    sizeof(user_stack),
                                                    20, &user_tcb);

aixos_task_mpu_region_add(user, (uintptr_t)user_buffer, sizeof(user_buffer),
                          AIXOS_MPU_READ | AIXOS_MPU_WRITE);
```

Region 规则：

- base 地址必须按 region size 对齐。
- size 必须是 2 的幂且不小于 `AIXOS_CFG_MPU_MIN_REGION_SIZE`。
- 可写 region 必须可读。
- 不要把内核 TCB、对象池、allocator metadata、内核栈或设备寄存器授权给用户任务，除非产品安全模型允许且移植层支持相关属性。

新增 syscall handler 时，用户 buffer 必须通过 `aixos_copy_from_user()`、`aixos_copy_to_user()` 或 `aixos_zero_to_user()` 传输。

## 板级移植集成

客户板级集成通常提供：

- Reset vector 和启动文件。
- 内存映射和 linker script。
- 内核 tick 启动前的时钟初始化。
- 中断向量表或 trap vector。
- 产品 heap 存储。
- 设备驱动和 ISR 注册。
- 可选板级 fault 导出路径。

新架构或板卡见 `PORTING_GUIDE.zh-CN.md`。

## ISR 集成

中断上下文只能调用明确标注为 `*_from_isr` 或 ISR-safe 的 API，且不能阻塞。Cortex-M3 上，数值低于 `AIXOS_CFG_KERNEL_IRQ_PRIORITY` 的高响应中断可以抢占内核临界区，但不能调用 AIXOS 服务 API；应只做硬件确认、时间戳和最小缓冲，把 RTOS 交互交给低优先级中断或任务。

典型 ISR handoff：

```c
void UART_IRQHandler(void)
{
    uint8_t byte = read_uart_byte();
    (void)aixos_pipe_write_from_isr(rx_pipe, &byte, 1);
}
```

## 发布确认

客户发布前至少运行：

```sh
make test
make test-mpu
make test-asan
make test-o2
make test-os
make analyze
make posix-api-check RISCV_PREFIX=riscv64-elf-
make arm
make riscv-validate RISCV_PREFIX=riscv64-elf-
make renode
make coverage
make ram-report RISCV_PREFIX=riscv64-elf-
make manifest RISCV_PREFIX=riscv64-elf-
make renode-arm-platforms RISCV_PREFIX=riscv64-elf-
make renode-riscv-stress RISCV_PREFIX=riscv64-elf-
make instruction-bench RISCV_PREFIX=riscv64-elf-
```

打包纯源码交付前运行 `make clean`。
