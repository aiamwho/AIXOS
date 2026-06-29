# AIXOS v1.0 发布说明

日期：2026-06-29

## 发布类型

AIXOS v1.0 是客户源码发布版本。它作为源码包交付，包含面向客户的文档，不包含内部过程报告或生成的演示材料。

## 基线来源

v1.0 从已验证的 v0.9 源码包初始化，并提升到 v1.0 版本、API 和 ABI 标识。保留了 v0.9 的证据、追踪、POSIX 边界说明、latency benchmark 和 trace 可视化工作。

## 包含内容

- `include/aixos/` 公共头文件。
- `kernel/` 内核源码。
- Cortex-M、Cortex-A55/AArch64 和 RV32IM 架构移植层。
- host test 架构 shim。
- POSIX 兼容头文件和实现。
- Renode 仿真脚本。
- Benchmark 输入工程。
- 构建、分析、manifest、RAM report 和工具检查脚本。
- `docs/` 下的客户文档。

## 本版本新增/修复

- `make evidence-package` 用于收集可审计发布证据。
- `make config` 终端菜单，用于客户裁剪内核。
- 可选调度器后端：默认 bitmap multi-queue，或小系统 footprint 更低的 simple ready list。
- Cortex-M kernel IRQ priority threshold 可配置。
- ISR nesting 原子计数、高水位诊断、可配置 nesting 预算和 crash-record overflow 报告。
- `tools/scheduler_benchmark.py` 用于 Cortex-M3 调度器大小/指令比较。
- 需求到测试的 traceability matrix。
- POSIX 兼容和限制矩阵。
- Cortex-M3/RV32IM latency benchmark 固件入口。
- trace viewer，可把 trace JSON 转成 CSV 或 HTML。
- Benchmark 和 latency 应用现在会在调度器启动前初始化已启用的 namespace 和 timing wheel，和 smoke/Hello World 初始化顺序一致，避免指令级仿真中 timeout list 未初始化导致的异常访问。
- 白盒路径测试暴露远期 timeout 可能延迟唤醒后，已修复 timing wheel 二级 slot 放置和 cascade 处理。
- Host 白盒和黑盒回归已扩展 namespace/resource-manager 路径、timing-wheel level/cascade 路径、microkernel 同步 IPC 路径、公共 API 无效参数矩阵和重复用户工作流场景。2026-06-29 host 套件现报告 23 个命名测试、`7207 checks, 0 failures`。
- Runtime API 边界仿真已在 Cortex-M3 和 RV32IM Renode 上运行，每个目标 929 个边界检查，failure 为 0。
- 2026-06-29 本地验证通过 `make quality`、MPU 专项、latency benchmark 构建、ARM Cortex Renode 平台矩阵、RV32IM Renode stress、AIXOS instruction benchmark、API 边界仿真和 evidence-package 生成。

## 版本标识

| 标识 | 值 |
|---|---|
| `AIXOS_VERSION_MAJOR` | `1` |
| `AIXOS_VERSION_MINOR` | `0` |
| `AIXOS_VERSION_PATCH` | `0` |
| `AIXOS_API_VERSION` | `0x00010000` |
| `AIXOS_ABI_VERSION` | `0x00010000` |

## 已知后续项

- 在客户集成环境重新运行完整 `make quality`。
- 当工具链、Renode、平台文件或 benchmark workload 变化时，重新运行 `make renode-arm-platforms`、`make renode-riscv-stress` 和 `make instruction-bench`。
- 完成产品相关 hardware-in-the-loop 验证。
- 在目标硬件和仿真模型上验证 MPU/PMP fault 行为。
- 测量客户目标上的最坏中断延迟、context-switch 延迟、timer callback 延迟和内存高水位。
- 每次客户发布都归档 ELF、map、编译器身份、配置、source manifest 和测试日志。

## 兼容说明

应用应把 v1.0 视为新的 API/ABI 基线。任何基于早期内部快照编译的客户代码都应重新构建并重新测试。
