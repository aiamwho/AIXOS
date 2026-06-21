# AIXOS Latency Benchmark

This benchmark is the v0.9 entry point for measurable latency evidence.

It exposes these symbols for Renode, hardware debugger, or GPIO/timer assisted
measurement:

| Symbol | Meaning |
|---|---|
| `latency_heartbeat` | Producer task loop heartbeat |
| `latency_errors` | Benchmark setup or runtime errors |
| `latency_context_switch_samples` | Producer loop samples after sleep/yield opportunities |
| `latency_sem_roundtrip_samples` | Semaphore post/wait handoff samples |
| `latency_mq_roundtrip_samples` | Message queue send/receive samples |
| `latency_timer_samples` | Periodic timer callback samples |
| `test_heartbeat` | Compatibility heartbeat for generic RISC-V validation |

Build:

```sh
make -C benchmarks/latency arm
make -C benchmarks/latency riscv RISCV_PREFIX=riscv64-elf-
```

The current benchmark records functional sample counts. Cycle-accurate or
wall-clock latency requires a target-specific counter, GPIO toggle, or simulator
metric adapter. Those adapters should preserve the symbol names above so the
same report tooling can compare Renode and hardware results.

