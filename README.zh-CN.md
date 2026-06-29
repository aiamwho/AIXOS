# AIXOS v1.0 中文说明

AIXOS v1.0 是一个面向嵌入式产品集成的 RTOS 内核源码包，支持 ARM
Cortex-M0/M3/M4/M33、Cortex-A55/AArch64、RV32IM 以及主机端测试构建。源码包包含内核源码、公共头文件、配置、架构移植层、POSIX 兼容层、测试、Renode 仿真文件、benchmark 输入工程和交付文档。

本包用于源码级集成。生成的 `build/` 输出、内部演进报告、竞品分析和演示材料不属于客户交付内容。

## 包内容

| 路径 | 用途 |
|---|---|
| `include/aixos/` | AIXOS 公共头文件和 ABI 可见类型 |
| `kernel/` | 调度、任务、堆、定时器、trace、crash、对象、IPC 和 microkernel 代码 |
| `arch/` | ARM Cortex-M、Cortex-A55/AArch64、RV32IM 和共享架构接口 |
| `config/` | 构建期配置默认值 |
| `compat/posix/`, `posix/` | POSIX 兼容实现和公共 POSIX 头文件 |
| `examples/hello_world/` | 最小任务和 UART hook 示例 |
| `examples/smoke/` | `make arm` 和 `make riscv` 默认 smoke 固件入口 |
| `tests/` | 主机端和 Renode 回归测试 |
| `simulation/` | Renode 平台和 smoke 脚本，覆盖 M0/M3/M4/M33/A55 |
| `benchmarks/` | AIXOS 和 FreeRTOS benchmark 输入工程 |
| `tools/` | 工具检查、静态分析、manifest、RAM 报告、Renode 配置和 RISC-V 工具链安装脚本 |
| `docs/` | 客户开发、架构、配置、移植和发布说明 |

## 版本信息

| 项目 | 值 |
|---|---|
| 产品版本 | `1.0.0` |
| `AIXOS_API_VERSION` | `0x00010000` |
| `AIXOS_ABI_VERSION` | `0x00010000` |
| Crash record version | `5` |
| 作者联系方式 | `csulxx@gmail.com` |

应用可包含 `aixos/aixos.h` 获取完整公共 API；如果希望减少依赖，也可以按模块包含单独头文件。

## 支持的构建目标

| 目标 | 命令 | 说明 |
|---|---|---|
| 主机测试 | `make test` | 构建并运行 `build/host/aixos_tests` |
| ARM Cortex-M 固件 | `make arm AIXOS_PLATFORM=cortex-m3` | 可选 `cortex-m0`、`cortex-m3`、`cortex-m4`、`cortex-m33` |
| Cortex-A55 固件 | `make arm AIXOS_PLATFORM=cortex-a55` | 使用 `clang --target=aarch64-none-elf` 和 `ld.lld` 链接 AArch64 ELF |
| RV32IM 固件 | `make riscv` | 使用 `riscv-none-elf-gcc`、`RISCV_PREFIX` 或 `RISCV_TOOLCHAIN_DIR` |
| ARM Cortex-M Renode | `make renode` | 构建并 smoke 测试所选 Cortex-M 平台 |
| ARM Cortex Renode 全平台 | `make renode-arm-platforms` | 加载检查并 smoke 测试 M0/M3/M4/M33/A55 |
| Cortex-A55 指令级 smoke | `make instruction-sim` | 运行 A55 Renode 指令级 smoke |
| RV32IM Renode | `make renode-riscv` | 构建、校验并仿真 RISC-V 镜像 |
| API 边界 Renode | `make api-boundary-sim RISCV_PREFIX=riscv64-elf-` | 在 Cortex-M3 和 RV32IM 上运行 API 参数边界和功能边界检查，在 `test-results/api-boundary/` 下生成中英文报告 |
| 静态分析 | `make analyze` | 通过 `tools/static_analysis.sh` 运行 Clang 静态分析 |
| 发布质量门 | `make quality` | 运行配置好的 release-quality 验证矩阵 |

默认固件入口是 `APP_SRCS ?= examples/smoke/main.c`。客户项目可以不修改源码包，直接传入自己的入口：

```sh
make arm APP_SRCS=/path/to/customer/main.c
make riscv APP_SRCS=/path/to/customer/main.c
```

RISC-V 工具链如果使用 `riscv64-elf-` 前缀：

```sh
make riscv RISCV_PREFIX=riscv64-elf-
make renode-riscv RISCV_PREFIX=riscv64-elf-
```

## 集成顺序

1. 审查 `config/aixos_cfg.h`，为目标产品设置内存、对象、定时器、trace、POSIX 和架构限制。
2. 在客户构建系统中引入 `include/`、`config/`、`kernel/`、`arch/include/`、选定 `arch/<target>/` 移植层以及需要的兼容层。
3. 提供目标 linker script 和启动流程。
4. 按文档初始化堆、对象、任务、trace、timer、可选 namespace/time wheel、调度器和架构层，再启动调度器。
5. 客户固件定版前运行 `TESTING.zh-CN.md` 中的主机、交叉构建、Renode 和静态分析检查。

## 文档

- [API 合同](API_CONTRACT.zh-CN.md)
- [API 参考](docs/API_REFERENCE.zh-CN.md)
- [Hello World](docs/HELLO_WORLD.zh-CN.md)
- [集成指南](docs/INTEGRATION_GUIDE.zh-CN.md)
- [需求追踪](docs/REQUIREMENTS_TRACEABILITY.zh-CN.md)
- [POSIX 兼容矩阵](docs/POSIX_COMPATIBILITY.zh-CN.md)
- [测试与验证](TESTING.zh-CN.md)
- [架构指南](docs/ARCHITECTURE.zh-CN.md)
- [开发指南](docs/DEVELOPMENT_GUIDE.zh-CN.md)
- [配置指南](docs/CONFIGURATION.zh-CN.md)
- [移植指南](docs/PORTING_GUIDE.zh-CN.md)
- [排障指南](docs/TROUBLESHOOTING.zh-CN.md)
- [发布说明](docs/RELEASE_NOTES.zh-CN.md)
- [中文文档索引](docs/README.zh-CN.md)

## 发布状态

本源码包是 v1.0 客户评估和集成版本。生产使用仍需要产品级硬件验证、最坏执行时间证据、长时间稳定性测试，以及与目标设备相匹配的安全或信息安全评估。

2026-06-29 本地验证已经覆盖主机质量门、ARM Cortex Renode smoke、RV32IM Renode stress、A55 指令级 smoke、AIXOS instruction benchmark，以及 Cortex-M3/RV32IM API 边界仿真。命令和仿真限制见 `TESTING.zh-CN.md`。
