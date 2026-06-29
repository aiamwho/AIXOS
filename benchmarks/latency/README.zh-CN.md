# AIXOS Latency Benchmark

该 benchmark 是用于采集可测延迟证据的入口。

它向 Renode、硬件调试器或 GPIO/timer 辅助测量暴露以下符号：

| Symbol | 含义 |
|---|---|
| `latency_heartbeat` | producer task 循环 heartbeat |
| `latency_errors` | benchmark 设置或运行期错误 |
| `latency_context_switch_samples` | sleep/yield 机会后的 producer 循环样本数 |
| `latency_sem_roundtrip_samples` | semaphore post/wait handoff 样本数 |
| `latency_mq_roundtrip_samples` | message queue send/receive 样本数 |
| `latency_timer_samples` | periodic timer callback 样本数 |
| `test_heartbeat` | 通用 RISC-V validation 兼容 heartbeat |

构建：

```sh
make -C benchmarks/latency arm
make -C benchmarks/latency riscv RISCV_PREFIX=riscv64-elf-
```

当前 benchmark 记录功能样本计数。cycle-accurate 或 wall-clock 延迟需要目标特定 counter、GPIO toggle 或仿真器 metric adapter。这些 adapter 应保留上表符号名，以便同一套报告工具比较 Renode 和硬件结果。
