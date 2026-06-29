# AIXOS v1.0 移植指南

本文说明把 AIXOS 移植到新 CPU、板卡或工具链所需工作。

## 移植检查表

1. 在 `arch/<family>/<target>/` 或 `arch/<target>/` 下新增目录。
2. 实现 `arch/include/aixos/arch/arch.h` 声明的架构 hook。
3. 提供启动代码、中断/trap 入口、context switch 汇编和 linker script。
4. 定义 tick source 和中断优先级策略。
5. 为目标 MPU、PMP 或等价保护单元实现 `aixos_arch_mpu_configure_task()`。
6. 保持 TCB `stack_top` 位于 offset 0。
7. 在 `Makefile` 中添加严格交叉构建目标。
8. 添加仿真或硬件 smoke 测试。
9. 运行 host、cross-build、static analysis 和目标测试。

## 架构端口职责

目标端口必须提供：

- 新任务初始栈帧构造。
- 上下文保存和恢复。
- 调度 yield 或 PendSV 等价触发。
- tick 中断初始化。
- 中断 enable、disable、restore 原语。
- ISR 上下文检测。
- 进入 C 代码前完成 data/BSS 初始化的启动路径。
- 启动代码和内存分配器所需 linker symbol。
- 当前任务的内存保护编程。

## Context Switch 合同

调度器假设旧任务在架构层保存上下文之前仍是 current task。架构层不能在保存 `stack_top` 前破坏 outgoing task 栈，也不能切换到新任务。

`aixos_tcb_t` 第一个字段是 `stack_top`。汇编可能用固定 offset 访问该字段，客户改动必须保持该布局合同。

## Tick 和 Timeout 合同

tick 中断必须：

- 每个 tick event 只推进一次内核时间。
- 当时间片或 timeout wakeup 需要时请求调度。
- 不直接从 tick ISR 派发 timer callback。
- 保存目标 ABI 要求的所有寄存器。

## 中断规则

架构端口必须支持 `API_CONTRACT.zh-CN.md` 的上下文矩阵：

- task-only API 必须拒绝 ISR 上下文。
- FromISR API 不能阻塞。
- 临界区必须恢复之前的中断状态。
- 若端口支持高于内核临界区屏蔽级别的中断，必须记录这些高响应 ISR 不在 kernel API-safe 优先级范围内。
- ISR nesting 计数必须对高优先级中断抢占保持原子，并保持 deferred scheduling 直到最外层 ISR 退出。

## Linker Script 要求

Linker script 必须提供所选端口需要的 code、rodata、data、BSS、stack 和 heap 内存区域，并保留 crash/trace 诊断所需 section。客户固件应随每个已验证构建归档 linker script。

## 内存保护要求

新端口必须在返回用户任务前应用任务内存保护。端口应提供：

- 用户取指的全局可执行代码 region。
- 来自 `task->mpu_regions` 的 per-task 数据 region。
- read/write、non-executable 用户栈保护。
- privileged kernel 对内核内存的访问，且不向 user mode 暴露内核内存。
- fault entry，把内存保护违规记录到 crash 路径。

Cortex-M 端口应编程 MPU region。RISC-V 端口应在 `mret` 到 U-mode 前编程 PMP entries。

## 验证

新端口至少应通过：

```sh
make test
make analyze
make <new-target>
```

发布确认还应增加：

- 可用仿真器上的 smoke test。
- 硬件 tick 和 heartbeat test。
- 中断嵌套和临界区测试。
- Context-switch 寄存器保存测试。
- 栈溢出和 crash-record 测试。
- 长时间 IPC 和 timer stress。
