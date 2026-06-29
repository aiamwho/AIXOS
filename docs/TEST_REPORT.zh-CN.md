# AIXOS 测试报告

日期：2026-06-29

## 范围

本报告覆盖当前 AIXOS 的白盒与黑盒验证结果，包括主机指令级测试、LLVM 覆盖率、静态分析和 Renode 仿真。

## 测试用例规模

- 主机测试入口：34 个
- 主机断言检查：7978 项
- 本轮新增覆盖范围：
  - crash、panic、reset、timer service、kernel start、task return trap
  - task signal、wake recheck、scheduler 与任务生命周期边界
  - POSIX thread 生命周期、TLS 析构、cond/rwlock/timer/mqueue 边界
  - mutex waiter 与优先级继承路径
  - pipe、message queue、event、notification、ISR/context 参数边界
  - heap/object 边界与 generation 处理
  - microkernel 用户态 syscall wrapper 与 fast syscall host dispatch

## 验证结果

| 命令 | 结果 |
| --- | --- |
| `make test` | 通过，7978 checks，0 failures |
| `make coverage` | 通过，报告生成于 `build/host-coverage/coverage.txt` |
| `make analyze` | 通过 |
| `make renode` | 通过，Cortex-M3 heartbeat=20 ticks=198 user=29 errors=0 |

## 覆盖率汇总

全量报告范围：`kernel compat/posix posix/src tests`

| 指标 | 覆盖率 |
| --- | ---: |
| Region | 64.75% |
| Function | 97.33% |
| Line | 90.49% |
| Branch | 67.24% |

产品代码范围：`kernel compat/posix posix/src`

| 指标 | 覆盖率 |
| --- | ---: |
| Region | 85.05% |
| Function | 99.76% |
| Line | 86.93% |
| Branch | 68.50% |

## 测试中修复的问题

- 修复 mutex 带 waiter 删除问题：`aixos_mutex_delete()` 现在返回 `AIXOS_ERR_BUSY`，不再在删除 mutex 对象时把所有权转移给等待任务。
- 为不可返回的固件路径增加 host-test-only 返回路径，使 crash/start/exit 路径可在仿真测试中覆盖，且不改变目标固件行为。
- 为 pipe、message queue、event 增加 host-test-only waiter 注入钩子，用于安全覆盖 wake 和 delete-busy 路径。
- fast syscall 在 host 测试中强制走 dispatcher，避免执行本机 `svc`/`ecall` 指令。

## 未达标项与原因

Function 覆盖率已经达到目标。Line 覆盖率已显著提升，但尚未达到 95%。剩余缺口主要集中在：

- POSIX 兼容层错误分支和多任务等待循环
- Microkernel 同步 IPC receiver/reply 边界路径
- 测试宏和 inline list helper 带来的 region/branch 统计膨胀
- 需要真实上下文切换或更多 host-only 钩子才能安全覆盖的阻塞路径

