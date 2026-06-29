# AIXOS v1.0 架构指南

本文总结客户源码包交付的架构，面向集成、评审或扩展 AIXOS 的工程师。

## 系统模型

AIXOS 是小型嵌入式 RTOS 内核，提供固定优先级调度、带 generation 检查的对象 handle、有界 IPC、可选 POSIX 兼容层，以及 Cortex-M、Cortex-A55/AArch64、RV32IM 和 host test 端口。

| 领域 | 源码 | 职责 |
|---|---|---|
| 对象模型 | `kernel/object.c` | 对象池、handle generation、类型检查 |
| 调度器 | `kernel/sched.c`, `kernel/task.c` | ready queue、任务状态、timeout list、优先级处理 |
| IPC | `kernel/ipc/` | semaphore、mutex、message queue、event、pipe、notify |
| 定时器 | `kernel/timer.c`, `kernel/timewheel.c` | 软件定时器和 timeout 管理 |
| 内存 | `kernel/heap.c`, `kernel/mempool.c` | 堆和固定块池 |
| MPU | `kernel/mpu.c` | 用户任务 region 元数据和软件访问检查 |
| 诊断 | `kernel/trace.c`, `kernel/crash.c` | trace、crash record、reset 诊断 |
| Microkernel | `kernel/microkernel.c`, `kernel/namespace.c` | 用户/内核域、capability、namespace |
| 架构端口 | `arch/` | 启动、context switch、trap/interrupt、linker script |
| POSIX 兼容 | `compat/posix/`, `posix/` | POSIX 风格 API wrapper 和头文件 |

## 启动和初始化

期望启动顺序：

1. 架构启动代码初始化 data/BSS 和端口所需 C runtime 假设。
2. 初始化 heap、object、task、trace、timer、可选 namespace/time wheel 和 scheduler。
3. 创建系统任务和应用任务。
4. 创建启动期所需 IPC 对象、timer 和 memory pool。
5. `aixos_arch_system_init()` 完成架构层 tick/中断配置。
6. `aixos_start()` 把控制权交给调度器。

示例入口是 `examples/smoke/main.c`。客户可用 `APP_SRCS=/path/to/customer/main.c` 替换，但必须保持等价初始化顺序。

## 任务和调度模型

任务由 `include/aixos/task.h` 中的 `aixos_tcb_t` 表示。TCB 第一个字段必须是 `stack_top`，架构汇编依赖 offset 0，不能移动。

调度器使用：

- 固定整数优先级。
- `AIXOS_CFG_TIME_SLICE_TICKS` 配置时间片。
- `AIXOS_CFG_SCHEDULER` 选择 ready-queue 后端。
- ready、running、blocked、delayed、suspended、stop 等显式状态。
- sleep 和阻塞 IPC 使用 timeout wait node。
- mutex owner 使用优先级继承。

两个调度后端：

- `AIXOS_CFG_SCHED_BITMAP`：每优先级一个 FIFO ready queue，并用 bitmap 标记非空优先级。默认路径，适合较大 runnable 集。
- `AIXOS_CFG_SCHED_SIMPLE`：单个按优先级排序的 ready list，适合小 runnable 集和更低内存占用。

## Handle 和对象模型

公共 handle 是 32 位有符号值：

- 低 8 位：对象槽位索引。
- 高 24 位：generation。

Generation 检查会拒绝槽位复用后的旧 handle。对象 API 还会校验对象类型。

## IPC 和定时器模型

IPC 层包含 semaphore、mutex、message queue、event flag、pipe 和 task notification。阻塞 IPC 只能在任务上下文调用；只有明确声明的 API 可用于 ISR。

AIXOS 使用 scheduler tick 表示系统时间。毫秒 API 会折算为 tick，并使用 wrap-safe timeout 比较。软件定时器回调通过 timer service 路径执行，不直接在 tick ISR 中执行。

## 内存和内存保护模型

内存子系统提供：

- 带对齐、损坏检查和可选运行期锁定的 heap。
- 固定块池，用于确定性运行期分配。
- 静态对象所有权规则。
- 用户任务 per-task 内存保护 region，由 Cortex-M MPU 或 RISC-V PMP 支撑。

`AIXOS_CFG_ENABLE_MPU` 启用 MPU/PMP 支持。用户任务默认获得一个 read/write、non-executable stack region。产品可通过 `aixos_task_mpu_region_add()` 增加 region。

Portable region 约束：

- size 必须是 2 的幂。
- size 至少为 `AIXOS_CFG_MPU_MIN_REGION_SIZE`。
- base 必须按 size 自然对齐。
- 可写 region 必须可读。
- 每个用户任务最多 `AIXOS_CFG_MPU_REGIONS_PER_TASK` 个 region。

Syscall handler 把用户指针视为不可信，只能通过 `aixos_copy_from_user()`、`aixos_copy_to_user()` 和 `aixos_zero_to_user()` 转移数据。

## 架构端口

交付端口：

- `arch/arm/cortex-m3/`
- `arch/arm/aarch64/`
- `arch/risc-v/`
- `tests/host/`

Renode 平台描述覆盖 Cortex-M0、M3、M4、M33 和 Cortex-A55。Cortex-M0/M3/M4/M33 可通过 `make config` 或 `AIXOS_PLATFORM=<name>` 选择并有 Renode smoke 覆盖。Cortex-A55 有 AArch64 ELF、GIC/generic timer、kernel heartbeat、user task heartbeat 和 syscall error 检查。

新端口必须实现 `arch/include/aixos/arch/arch.h` 中的架构接口，提供启动代码、linker script，并保持 `PORTING_GUIDE.zh-CN.md` 中的调度和中断语义。

## Cortex-M 中断模型

Cortex-M3 端口使用 `BASEPRI` 保护内核临界区，而不是全局屏蔽中断。`AIXOS_CFG_KERNEL_IRQ_PRIORITY` 默认 `0x40`；数值更低的中断仍可抢占内核临界区，但处于 AIXOS service-API-safe 区域之外，不能调用包括 `*_from_isr` 在内的 AIXOS 服务。应使用低优先级 IRQ 或任务 handoff 进行 RTOS 交互。

ISR enter/exit 维护原子 nesting counter。从 ISR 请求的 reschedule 会推迟到最外层 ISR 退出。超过 `AIXOS_CFG_ISR_NESTING_MAX` 时，内核记录 `AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW` crash record。
