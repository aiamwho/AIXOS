# Renode ARM Cortex 平台矩阵

本包包含用于 ARM 指令级 bring-up 覆盖的 Renode 平台描述。Cortex-M0、Cortex-M3、Cortex-M4、Cortex-M33 是可选 AIXOS Cortex-M 目标，并具有 Renode smoke 覆盖。Cortex-A55 提供 AArch64 源码、可链接 ELF 输出、Renode 平台加载覆盖和可启动 Renode smoke 覆盖。

| Cortex core | Renode 平台 | Renode CPU 模型 | 当前 AIXOS 状态 |
|---|---|---|---|
| Cortex-M0 | `simulation/cortex_m0.repl` | `CPU.CortexM`, `cortex-m0` | 支持 ARMv6-M PRIMASK context-switch 路径 |
| Cortex-M3 | `simulation/stm32f103.repl` | `CPU.CortexM`, `cortex-m3` | 支持，默认目标 |
| Cortex-M4 | `simulation/cortex_m4.repl` | `CPU.CortexM`, `cortex-m4` | 通过 Cortex-M3 类 BASEPRI 路径支持 |
| Cortex-M33 | `simulation/cortex_m33.repl` | `CPU.CortexM`, `cortex-m33` | 支持基础 Renode smoke；该 Renode 模型上 MPU/fault-status 处理关闭 |
| Cortex-A55 | `simulation/cortex_a55.repl` | `CPU.ARMv8A`, `cortex-a55` | 支持 `clang --target=aarch64-none-elf`、`ld.lld`、GICv3/generic timer、heartbeat、ticks、user task heartbeat 和零 user syscall errors |

## 命令

加载检查所有 ARM Cortex Renode 平台：

```sh
make renode-arm-platform-check
```

运行 ARM smoke：

```sh
make renode-arm-smoke
```

`make renode-arm-platforms` 会同时运行平台加载检查和 smoke。

构建 Cortex-A55 AArch64 对象：

```sh
make arm AIXOS_PLATFORM=cortex-a55
```

运行 Cortex-A55 指令级 smoke：

```sh
make instruction-sim
```

运行 benchmark 指令级指标：

```sh
make instruction-bench RISCV_PREFIX=riscv64-elf-
```

该命令总是验证 AIXOS A55、Cortex-M3 和 RV32IM benchmark 路径。除非存在 `third_party/FreeRTOS-Kernel`，否则 FreeRTOS benchmark 行会标记为 skipped。
