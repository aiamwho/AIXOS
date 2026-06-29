# AIXOS v1.0 开发指南

本文定义客户侧 AIXOS 开发的常规工程流程。

## 文档阅读顺序

| 文档 | 用途 |
|---|---|
| `README.zh-CN.md` | 包内容和构建目标概览 |
| `docs/HELLO_WORLD.zh-CN.md` | 最小任务和 UART 输出示例 |
| `docs/INTEGRATION_GUIDE.zh-CN.md` | 端到端产品集成流程 |
| `docs/API_REFERENCE.zh-CN.md` | 模块级公共 API 用法 |
| `API_CONTRACT.zh-CN.md` | API、ABI、所有权、timeout 和上下文规则 |
| `docs/REQUIREMENTS_TRACEABILITY.zh-CN.md` | 需求到测试追踪 |
| `docs/POSIX_COMPATIBILITY.zh-CN.md` | POSIX 支持和限制矩阵 |
| `docs/CONFIGURATION.zh-CN.md` | 产品配置和容量设置 |
| `docs/PORTING_GUIDE.zh-CN.md` | 新 CPU、板卡或工具链移植 |
| `TESTING.zh-CN.md` | 发布验证矩阵 |
| `docs/TROUBLESHOOTING.zh-CN.md` | 常见构建、运行、MPU 和 Renode 问题 |

## 源码布局

| 路径 | 用途 |
|---|---|
| `include/aixos/` | 公共内核头文件 |
| `kernel/` | 内核实现 |
| `kernel/ipc/` | IPC 原语实现 |
| `kernel/mpu.c` | 用户内存保护元数据和校验 |
| `arch/include/` | 架构抽象 |
| `arch/arm/cortex-m3/` | Cortex-M 端口 |
| `arch/arm/aarch64/` | Cortex-A55/AArch64 端口 |
| `arch/risc-v/` | RV32IM 端口 |
| `config/` | 编译期配置 |
| `examples/hello_world/` | 最小任务和 UART hook 示例 |
| `examples/smoke/` | 默认 smoke 固件应用 |
| `posix/`, `compat/posix/` | POSIX 兼容头文件和适配层 |
| `tests/` | host 和 Renode 回归测试 |
| `tools/` | 验证和构建辅助工具 |
| `benchmarks/` | latency 和 instruction benchmark 输入 |

## 构建前提

根据目标安装工具：

- host tests：Apple Clang、Clang 或 GCC。
- Cortex-M：`arm-none-eabi-gcc`。
- RV32IM：`riscv-none-elf-gcc` 或通过 `RISCV_PREFIX` 选择的 RV32 GCC。
- simulation：支持 robot testing 的 Renode。
- manifest：Node.js。

先运行：

```sh
make toolcheck
```

常用命令：

```sh
make test
make arm
make riscv RISCV_PREFIX=riscv64-elf-
make renode
make renode-riscv RISCV_PREFIX=riscv64-elf-
make analyze
make coverage
make manifest
make evidence-package RISCV_PREFIX=riscv64-elf-
make latency-bench RISCV_PREFIX=riscv64-elf-
make instruction-bench RISCV_PREFIX=riscv64-elf-
make clean
```

固件目标默认使用 `APP_SRCS ?= examples/smoke/main.c`。客户固件应通过 `APP_SRCS` 提供入口。

## 添加内核代码

1. 只有客户 API 才放公共声明。
2. 内部声明留在所属 kernel module。
3. 在 API 边界校验 task/ISR 上下文。
4. 使用现有 `AIXOS_ERR_*`，不要自造局部返回约定。
5. 保持 `AIXOS_CFG_MAX_IPC_COPY_BYTES` 等有界 copy 限制。
6. 修改共享内核行为前更新 host tests。
7. API 语义或 ABI 可见类型变化时更新 `API_CONTRACT.zh-CN.md` 和英文原文。
8. 公共 API 变化时更新 `API_REFERENCE.zh-CN.md` 和英文原文。
9. 集成步骤或源集合变化时更新 `INTEGRATION_GUIDE.zh-CN.md` 和英文原文。

## 公共 API 规则

公共函数应：

- 返回 `AIXOS_OK` 或负的 `AIXOS_ERR_*`。
- 拒绝无效 handle 和 stale generation。
- 用 `AIXOS_ERR_CONTEXT` 拒绝不支持的上下文。
- 避免对客户可控数据做无界循环。
- 除非 API 明确记录生命周期所有权，否则不要保留调用者 buffer。

## 内存和用户保护规则

生产代码应在初始化阶段创建动态对象。运行期优先使用静态对象、固定块池、调用者 buffer 和有界栈对象。不要在 ISR 中分配；`AIXOS_CFG_HEAP_LOCK_ON_START` 锁定后不要直接应用堆分配。

用户任务栈会自动加入 read/write、non-executable MPU region。额外用户 buffer 必须显式注册：

```c
aixos_task_mpu_region_add(task, (uintptr_t)buffer, buffer_size,
                          AIXOS_MPU_READ | AIXOS_MPU_WRITE);
```

buffer base 必须按 `buffer_size` 对齐，`buffer_size` 必须为 2 的幂。不要向用户授权内核对象、内核栈、对象池或 allocator metadata。

新增 syscall 或用户可见内核服务时，不能直接解引用或 `memcpy()` 用户指针。使用：

- `aixos_copy_from_user()` 输入。
- `aixos_copy_to_user()` 输出。
- `aixos_zero_to_user()` 清零用户可见存储。

## 并发规则

- 调度器锁定时不要调用阻塞 API。
- ISR 中不要阻塞。
- 调度器锁或中断锁只覆盖短临界区。
- 定时器回调保持有界。
- 任务删除必须由资源 owner 应用逻辑协调。

## 变更测试要求

共享内核变更应包含：

- 可在无真实 context switch 环境验证的 host unit test。
- 行为依赖架构切换、tick 或中断入口时，补充 Renode test 或确认已有覆盖。
- `make analyze` 静态分析。
- 受影响架构的交叉构建。

## 发布工件

每个客户固件发布应归档源码包版本或 checksum、`config/aixos_cfg.h`、`make toolcheck` 编译器身份、ELF、map、`make manifest` 输出、测试日志和客户补丁。

## 客户交付检查

1. 包中不包含生成的 `build/` 或 `test-results/`。
2. `README.md`、`API_CONTRACT.md`、`TESTING.md` 和 `docs/` 描述当前源码树。
3. 对交付架构运行 `TESTING.zh-CN.md` 中的验证命令。
4. 记录仿真器或硬件限制，不把它们作为隐藏假设。
5. 二进制证据单独归档编译器身份、manifest、RAM report 和已验证二进制。
