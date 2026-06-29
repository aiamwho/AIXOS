# AIXOS v1.0 测试与验证指南

本文定义客户集成和发布确认的最低验证流程。所有命令从源码包根目录运行。

## 验证层级

| 层级 | 命令 | 目的 |
|---|---|---|
| 工具盘点 | `make toolcheck` | 记录编译器和仿真器可用性 |
| 主机单元测试 | `make test` | 构建并运行 native tests |
| MPU 专项 | `make test-mpu` | 只运行 MPU region 和 user-memory access 检查 |
| ASan/UBSan | `make test-asan` | 使用 AddressSanitizer 和 UBSan 运行 host tests |
| 优化变体 | `make test-o2`, `make test-os` | 使用 O2/Os 重新运行 host tests |
| ARM Cortex-M 构建 | `make arm` | 构建所选 Cortex-M 平台 |
| Cortex-A55 构建 | `make arm AIXOS_PLATFORM=cortex-a55` | 构建并链接 AArch64 ELF |
| ARM 平台加载检查 | `make renode-arm-platform-check` | 验证 M0/M3/M4/M33/A55 Renode 平台描述能加载 |
| RV32IM 构建 | `make riscv` | 构建 RISC-V 固件 |
| RV32IM ELF 校验 | `make riscv-validate` | 校验 ELF 架构和必要符号 |
| ARM Cortex-M 仿真 | `make renode` | 对所选 Cortex-M 运行 heartbeat/tick/user 检查 |
| ARM Cortex 全平台仿真 | `make renode-arm-smoke` | 对 M0/M3/M4/M33/A55 运行 smoke |
| Cortex-A55 指令级 smoke | `make instruction-sim` | 记录 A55 heartbeat、tick、user 和 error 指标 |
| RV32IM 仿真 | `make renode-riscv` | 检查 heartbeat、tick、trap 和寄存器 |
| RV32IM stress | `make renode-riscv-stress` | 重复 RISC-V Renode 测试 5 次 |
| API 边界仿真 | `make api-boundary-sim RISCV_PREFIX=riscv64-elf-` | 在 Cortex-M3 和 RV32IM Renode 上运行调度器启动后的 API 参数边界和功能边界检查，并生成 `test-results/api-boundary/report.md` 和 `test-results/api-boundary/report.zh-CN.md` |
| 静态分析 | `make analyze` | 运行 Clang 静态分析 |
| 覆盖率 | `make coverage` | 生成 LLVM 覆盖率报告 |
| RAM 报告 | `make ram-report` | 报告镜像内存使用 |
| Manifest | `make manifest` | 生成源码和构建元数据 |
| Evidence package | `make evidence-package RISCV_PREFIX=riscv64-elf-` | 归档验证日志、报告、配置、文档、ELF 和 map |
| Latency benchmark 构建 | `make latency-bench RISCV_PREFIX=riscv64-elf-` | 构建 Cortex-M3 和 RV32IM latency benchmark 固件 |
| Instruction benchmark | `make instruction-bench RISCV_PREFIX=riscv64-elf-` | 构建 AIXOS benchmark 并采集 A55、Cortex-M3、RV32IM Renode 指标；没有 `third_party/FreeRTOS-Kernel` 时 FreeRTOS case 标记 skipped |
| 全量质量门 | `make quality` | 运行 release-quality 目标集合 |

如果 RISC-V 编译器不是默认 `riscv-none-elf-` 前缀，应显式传参：

```sh
make riscv-validate RISCV_PREFIX=riscv64-elf-
make renode-riscv RISCV_PREFIX=riscv64-elf-
```

## 必需回归覆盖

客户发布确认至少应覆盖：

- 对象槽复用后 stale generation handle 被拒绝。
- 堆分配满足 `AIXOS_CFG_ALIGNMENT`。
- 相邻堆释放能合并并恢复容量。
- 零 timeout wait 不阻塞。
- 阻塞任务只处于一个对象等待队列，并最多处于一个 timeout list。
- IPC post 唤醒一个合格 waiter，并移除其 timeout entry。
- 阻塞 queue/pipe 操作在唤醒后重新检查条件。
- task-only API 拒绝 ISR 上下文，FromISR API 拒绝任务上下文。
- 定时器回调不在 tick ISR 中执行。
- mutex waiter 超时后撤销 owner 的继承优先级。
- 删除持有 mutex 的任务会被拒绝。
- 消息队列 receive 在目的 buffer 太小时不消费消息。
- 静态任务、队列、管道和固定块池存储不会被内核释放。
- time-slice 到期只请求 context switch，不在架构保存上下文前替换 `g_cur_task`。
- Cortex-M PendSV 在 TCB offset 0 读取 `stack_top`。
- RISC-V trap frame 保存 x1-x31、`mepc`、`mstatus` 并保持 16 字节栈对齐。
- Cortex-M0/M3/M4/M33/A55 Renode 平台描述可加载。
- Renode 运行期 API 边界检查覆盖 task lifecycle、semaphore、mutex、message queue、event、pipe、task notification、software timer、heap lockdown、fixed-block mempool、MPU validation 和 user syscall wrapper。
- 白盒路径测试覆盖 namespace 注册/capability/resource 生命周期、timing wheel 一级/二级插入和 cascade、microkernel 同步 IPC send/receive/reply/disconnect 路径，以及公共内核 API 的无效参数矩阵。
- 黑盒工作流测试反复覆盖 semaphore、message queue、event、pipe、notification 和 timeout 相关用户场景，包括代表性 payload 和边界值。

## 客户发布准入

1. 干净 checkout 或 release package 中 `make quality` 通过。
2. Cortex-M3 和/或 RV32IM 固件能按客户精确配置构建。
3. 所选架构 Renode 回归通过。
4. 至少一个目标板版本通过 hardware-in-the-loop 测试。
5. 在目标时钟配置上测量最坏中断延迟和 context-switch 延迟。
6. 长时间 stress 覆盖 timer、IPC、heap、task 和 reset 路径。
7. 发布归档包含 ELF、map、配置、编译器身份、source manifest 和测试日志。

## 当前本地验证快照

2026-06-29 本包已用以下工具重新验证：

- Apple clang 21.0.0：host tests。
- `arm-none-eabi-gcc` 16.1.0：Cortex-M 构建。
- `riscv64-elf-gcc` 16.1.0：RV32IM 构建。
- Renode 1.16.1.28836：ARM Cortex、Cortex-A55 和 RV32IM 仿真。

通过命令：

- `make quality RISCV_PREFIX=riscv64-elf-`
- `make test-mpu`
- `make latency-bench RISCV_PREFIX=riscv64-elf-`
- `make renode-arm-platforms RISCV_PREFIX=riscv64-elf-`
- `make renode-riscv-stress RISCV_PREFIX=riscv64-elf-`
- `make instruction-bench RISCV_PREFIX=riscv64-elf-`
- `make api-boundary-sim RISCV_PREFIX=riscv64-elf-`
- `make evidence-package RISCV_PREFIX=riscv64-elf-`

当前 host 回归套件包含 23 个命名测试函数，并在默认构建、ASan/UBSan、`-O2`、`-Os` 和 LLVM coverage 构建下均报告 `7207 checks, 0 failures`。新增白盒和黑盒覆盖位于 `tests/test_path_coverage.c`。

本轮 LLVM coverage 快照在 `kernel`、`compat/posix`、`posix/src` 和 `tests` 范围内报告：region coverage `63.11%`、function coverage `83.91%`、line coverage `78.11%`、branch coverage `61.19%`。

`instruction-bench` 结果：AIXOS Cortex-M3 benchmark `heartbeat=50`、`messages=50`、`errors=0`、`ticks=499`、`switches=151`；AIXOS RV32IM benchmark `heartbeat=46`、`messages=46`、`errors=0`、`ticks=499`、`switches=231`。由于本源码包不包含 `third_party/FreeRTOS-Kernel`，FreeRTOS benchmark case 记录为 skipped。

`api-boundary-sim` 结果：Cortex-M3 和 RV32IM 均完成 132 个内核边界检查、0 failure；每个目标还完成 797 个用户态 syscall wrapper 检查、0 user failure。即每个仿真目标 929 个边界检查，两个仿真目标合计 1858 个边界检查。报告路径为 `test-results/api-boundary/report.md` 和 `test-results/api-boundary/report.zh-CN.md`。

## 已知验证限制

- 主机架构 shim 不执行真实 context switch。
- host 阻塞测试检查状态转换，但不执行真实栈切换。
- 完整 IPC 和时序 interleaving 仍依赖 Renode 和硬件测试。
- 物理板验证是产品相关工作，不包含在源码包内。
- Cortex-A55 Renode 检查覆盖平台加载、ELF 加载、GIC/generic timer tick、kernel heartbeat、user task heartbeat 和零 user syscall error；硬件中断延迟和 cache/MMU 行为仍需板级验证。
- RISC-V 工具链可通过 `RISCV_TOOLCHAIN_DIR` 指定。
- Renode 可用性依赖本地 Renode 安装和 robot-server 配置。
- FreeRTOS benchmark 执行需要加入固定版本的 `third_party/FreeRTOS-Kernel`；否则 `instruction-bench` 只验证 AIXOS benchmark 路径并把 FreeRTOS 标记为 skipped。
