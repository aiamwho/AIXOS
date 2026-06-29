# AIXOS v1.0 需求追踪

本文把客户可见 RTOS 需求映射到源码归属和验证证据，用于让发布确认可审计。

状态含义：

- `Covered`：已实现并由列出的测试覆盖。
- `Partial`：已实现，但目标端或 stress 证据不完整。
- `Planned`：v1.0 未完成。

## 核心内核

| ID | 需求 | 源码归属 | 验证 | 状态 |
|---|---|---|---|---|
| REQ-OBJ-001 | 对象槽复用后，公共 handle 必须拒绝 stale generation。 | `kernel/object.c`, `include/aixos/types.h` | `tests/test_object.c`, `make test` | Covered |
| REQ-OBJ-002 | 公共对象 API 必须拒绝类型错误的 handle。 | `kernel/object.c`, `kernel/ipc/*` | `tests/test_object.c`, `tests/test_kernel.c` | Covered |
| REQ-SCHED-001 | 调度器必须选择最高优先级 ready task。 | `kernel/sched.c`, `kernel/task.c` | `tests/test_stress.c`, Renode smoke | Partial |
| REQ-SCHED-002 | 同优先级 round-robin 任务必须按时间片轮转。 | `kernel/sched.c`, `kernel/task.c` | `tests/test_stress.c` | Partial |
| REQ-SCHED-003 | 调度器锁定导致阻塞不安全时，阻塞 API 必须返回 `AIXOS_ERR_LOCKED`。 | `kernel/task.c`, `kernel/ipc/*` | `tests/test_reliability.c`, `tests/test_stress.c` | Covered |
| REQ-TIME-001 | 毫秒 timeout 必须转换为 wrap-safe kernel tick。 | `kernel/task.c`, `kernel/timer.c`, `kernel/timewheel.c` | `tests/test_stress.c`, `tests/test_path_coverage.c` | Covered |
| REQ-TIME-002 | `timeout_ms == 0` 必须非阻塞。 | `kernel/ipc/*`, `kernel/task.c` | `tests/test_kernel.c`, `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-TIME-003 | `timeout_ms == UINT32_MAX` 必须无限等待。 | `kernel/task.c`, `kernel/ipc/*` | `tests/test_kernel.c`, `tests/test_stress.c` | Covered |

## 任务和栈

| ID | 需求 | 源码归属 | 验证 | 状态 |
|---|---|---|---|---|
| REQ-TASK-001 | 动态任务创建必须拒绝小于 `AIXOS_CFG_MIN_TASK_STACK_SIZE` 的栈。 | `kernel/task.c` | `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-TASK-002 | 静态任务创建必须在任务生命周期内保留调用者 TCB 和栈。 | `kernel/task.c`, `include/aixos/task.h` | `tests/test_stress.c`, `examples/smoke/main.c`, `make api-boundary-sim` | Covered |
| REQ-TASK-003 | `aixos_task_stack_check()` 必须检测 stack guard 损坏。 | `kernel/task.c` | `tests/test_reliability.c` | Covered |
| REQ-TASK-004 | `aixos_tcb_t` 第一个字段必须保持 `stack_top`，供架构汇编使用。 | `include/aixos/task.h`, `arch/*/portasm.s` | `TESTING.md`, cross-builds | Partial |
| REQ-TASK-005 | 删除持有 mutex 的任务必须被拒绝。 | `kernel/task.c`, `kernel/ipc/mutex.c` | `tests/test_reliability.c` | Covered |

## IPC

| ID | 需求 | 源码归属 | 验证 | 状态 |
|---|---|---|---|---|
| REQ-SEM-001 | Semaphore 必须支持 task wait、task post、ISR post、timeout 和 delete。 | `kernel/ipc/sem.c` | `tests/test_kernel.c`, `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-MUTEX-001 | Mutex unlock 必须校验 owner。 | `kernel/ipc/mutex.c` | `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-MUTEX-002 | Mutex 必须对高优先级 waiter 应用优先级继承。 | `kernel/ipc/mutex.c`, `kernel/sched.c` | `tests/test_reliability.c` | Covered |
| REQ-MQ-001 | Message queue 必须拒绝超过配置消息大小的发送。 | `kernel/ipc/mq.c` | `tests/test_kernel.c`, `make api-boundary-sim` | Covered |
| REQ-MQ-002 | Message queue 必须在接收 buffer 太小时拒绝且不消费消息。 | `kernel/ipc/mq.c` | `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-EVT-001 | Event wait 必须支持 AND、OR 和 clear-on-consume。 | `kernel/ipc/event.c` | `tests/test_kernel.c`, `make api-boundary-sim` | Covered |
| REQ-PIPE-001 | Pipe 必须返回传输字节数或负错误码。 | `kernel/ipc/pipe.c` | `tests/test_kernel.c`, `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-NOTIFY-001 | Task notification 必须支持 direct task signaling 和 wait/take 模式。 | `kernel/ipc/notify.c` | `tests/test_stress.c`, `make api-boundary-sim` | Covered |

## 内存、诊断、MPU 和 microkernel

| ID | 需求 | 源码归属 | 验证 | 状态 |
|---|---|---|---|---|
| REQ-HEAP-001 | Heap allocation 必须满足 `AIXOS_CFG_ALIGNMENT`。 | `kernel/heap.c` | `tests/test_heap.c` | Covered |
| REQ-HEAP-002 | 相邻 heap free 必须合并。 | `kernel/heap.c` | `tests/test_heap.c`, `tests/test_reliability.c` | Covered |
| REQ-HEAP-003 | Runtime heap lockdown 必须禁用直接应用堆分配。 | `kernel/heap.c`, `kernel/task.c` | `tests/test_reliability.c`, `make api-boundary-sim` | Covered |
| REQ-MEMPOOL-001 | 固定块池必须拒绝 double free 和外部指针。 | `kernel/mempool.c` | `tests/test_heap.c`, `make api-boundary-sim` | Covered |
| REQ-TRACE-001 | Trace record 必须受 ring buffer 限制。 | `kernel/trace.c` | `tests/test_reliability.c`, `tools/trace_viewer.mjs` | Partial |
| REQ-CRASH-001 | Crash record 必须 ABI-size 固定并可 CRC 校验。 | `kernel/crash.c`, `include/aixos/abi.h` | `tests/test_reliability.c`, `tools/diagnostics_decode.mjs` | Covered |
| REQ-MPU-001 | 用户任务栈必须注册为 read/write user region。 | `kernel/task.c`, `kernel/mpu.c`, `arch/*/port.c` | `tests/test_mpu.c`, `tests/test_microkernel.c`, `make api-boundary-sim` | Covered |
| REQ-MPU-002 | 用户 region 必须是自然对齐的 2 的幂范围。 | `kernel/mpu.c` | `tests/test_mpu.c`, `make api-boundary-sim` | Covered |
| REQ-MPU-003 | 可写用户 region 必须可读。 | `kernel/mpu.c` | `tests/test_mpu.c`, `make api-boundary-sim` | Covered |
| REQ-MPU-004 | 内核服务必须通过受审计 user-copy helper 复制用户 buffer。 | `kernel/microkernel.c`, `kernel/mpu.c` | `tests/test_mpu.c`, `tests/test_microkernel.c`, `make api-boundary-sim` | Covered |
| REQ-CAP-001 | Capability 必须对用户可访问内核对象执行 rights 检查。 | `kernel/microkernel.c`, `kernel/namespace.c` | `tests/test_microkernel.c`, `tests/test_path_coverage.c` | Covered |
| REQ-CAP-002 | Capability 必须支持 close/release 语义。 | `kernel/microkernel.c` | `tests/test_microkernel.c`, `tests/test_path_coverage.c` | Covered |
| REQ-IPC-SYNC-001 | 同步消息 IPC 必须支持 send、receive、reply、connect、disconnect。 | `kernel/microkernel.c` | `tests/test_microkernel.c`, `tests/test_path_coverage.c` | Covered |

## 架构端口

| ID | 需求 | 源码归属 | 验证 | 状态 |
|---|---|---|---|---|
| REQ-ARM-001 | Cortex-M 端口必须以 warnings-as-errors 构建。 | `arch/arm/cortex-m3/` | `make arm` | Covered |
| REQ-ARM-002 | Cortex-M Renode smoke 必须显示 tick 和 runnable task。 | `arch/arm/cortex-m3/`, `tests/renode_cortexm3.robot` | `make renode` | Partial |
| REQ-IRQ-001 | ISR nesting 计数必须原子、受配置限制，并推迟调度到最外层 ISR 退出。 | `kernel/isr.c`, `arch/arm/cortex-m3/portasm.s` | `tests/test_reliability.c`, `make test`, `make arm` | Covered |
| REQ-RV-001 | RV32IM 端口必须构建为 ELF32 RISC-V 并暴露 trap/heartbeat 符号。 | `arch/risc-v/` | `make riscv-validate RISCV_PREFIX=riscv64-elf-` | Covered |
| REQ-RV-002 | RISC-V trap frame 必须保存 x1-x31、`mepc` 和 `mstatus`。 | `arch/risc-v/portasm.s`, `examples/smoke/main.c` | `tests/renode_riscv.robot` | Partial |

## 仿真器边界证据

`make api-boundary-sim RISCV_PREFIX=riscv64-elf-` 会构建 `examples/api_boundary/main.c`，并在 Cortex-M3 与 RV32IM Renode 上运行。2026-06-29 运行结果为每个目标完成 132 个内核边界检查和 797 个用户态 syscall wrapper 检查，failure 均为 0。生成报告为 `test-results/api-boundary/report.md` 和 `test-results/api-boundary/report.zh-CN.md`。

同一轮验证还在 `tests/test_path_coverage.c` 中扩展了 host 白盒和黑盒覆盖。Host 套件完成 23 个命名测试、`7207 checks, 0 failures`；覆盖率报告为 line coverage `78.11%`、branch coverage `61.19%`。

## 证据缺口

v1.0 hardening 仍跟踪以下缺口：

- 至少一个 Cortex-M 和一个 RISC-V 目标的 hardware-in-the-loop 测试。
- 硬件上的中断延迟、context-switch 延迟、IPC 延迟和 timer dispatch 延迟测量。
- 长时间 timer、IPC、task、heap 和 reset stress 日志。
- 认证级需求、使用假设和 safety manual。
- 超出支持子集的完整 POSIX 语义一致性。
