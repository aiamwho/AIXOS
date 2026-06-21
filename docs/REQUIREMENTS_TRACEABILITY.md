# AIXOS v1.0 Requirements Traceability

This matrix connects customer-visible RTOS requirements to source ownership and
verification evidence. It is intended to make release qualification auditable.

Status values:

- `Covered`: implemented and exercised by listed tests.
- `Partial`: implemented but missing complete target or stress evidence.
- `Planned`: not complete in v1.0.

## Core Kernel

| ID | Requirement | Source ownership | Verification | Status |
|---|---|---|---|---|
| REQ-OBJ-001 | Public handles shall reject stale generation values after object slot reuse. | `kernel/object.c`, `include/aixos/types.h` | `tests/test_object.c`, `make test` | Covered |
| REQ-OBJ-002 | Public object APIs shall reject wrong-type handles. | `kernel/object.c`, `kernel/ipc/*` | `tests/test_object.c`, `tests/test_kernel.c` | Covered |
| REQ-SCHED-001 | The scheduler shall select the highest-priority ready task. | `kernel/sched.c`, `kernel/task.c` | `tests/test_stress.c`, Renode smoke | Partial |
| REQ-SCHED-002 | Round-robin tasks at the same priority shall be time sliced. | `kernel/sched.c`, `kernel/task.c` | `tests/test_stress.c` | Partial |
| REQ-SCHED-003 | Blocking APIs shall return `AIXOS_ERR_LOCKED` when the scheduler lock would make blocking unsafe. | `kernel/task.c`, `kernel/ipc/*` | `tests/test_reliability.c`, `tests/test_stress.c` | Covered |
| REQ-TIME-001 | Millisecond timeouts shall be converted to wrap-safe kernel ticks. | `kernel/task.c`, `kernel/timer.c`, `kernel/timewheel.c` | `tests/test_stress.c` | Covered |
| REQ-TIME-002 | `timeout_ms == 0` shall be non-blocking. | `kernel/ipc/*`, `kernel/task.c` | `tests/test_kernel.c`, `tests/test_reliability.c` | Covered |
| REQ-TIME-003 | `timeout_ms == UINT32_MAX` shall wait indefinitely. | `kernel/task.c`, `kernel/ipc/*` | `tests/test_kernel.c`, `tests/test_stress.c` | Covered |

## Tasks and Stacks

| ID | Requirement | Source ownership | Verification | Status |
|---|---|---|---|---|
| REQ-TASK-001 | Dynamic task creation shall reject stacks smaller than `AIXOS_CFG_MIN_TASK_STACK_SIZE`. | `kernel/task.c` | `tests/test_reliability.c` | Covered |
| REQ-TASK-002 | Static task creation shall retain caller-owned TCB and stack until deletion. | `kernel/task.c`, `include/aixos/task.h` | `tests/test_stress.c`, `examples/smoke/main.c` | Covered |
| REQ-TASK-003 | Stack guard corruption shall be detected by `aixos_task_stack_check()`. | `kernel/task.c` | `tests/test_reliability.c` | Covered |
| REQ-TASK-004 | The first `aixos_tcb_t` field shall remain `stack_top` for architecture assembly. | `include/aixos/task.h`, `arch/*/portasm.s` | `TESTING.md`, cross-builds | Partial |
| REQ-TASK-005 | Task deletion shall reject unsafe deletion of tasks owning mutexes. | `kernel/task.c`, `kernel/ipc/mutex.c` | `tests/test_reliability.c` | Covered |

## IPC

| ID | Requirement | Source ownership | Verification | Status |
|---|---|---|---|---|
| REQ-SEM-001 | Semaphores shall support task wait, task post, ISR post, timeout, and delete. | `kernel/ipc/sem.c` | `tests/test_kernel.c`, `tests/test_reliability.c` | Covered |
| REQ-MUTEX-001 | Mutexes shall enforce ownership on unlock. | `kernel/ipc/mutex.c` | `tests/test_reliability.c` | Covered |
| REQ-MUTEX-002 | Mutexes shall apply priority inheritance for higher-priority waiters. | `kernel/ipc/mutex.c`, `kernel/sched.c` | `tests/test_reliability.c` | Covered |
| REQ-MQ-001 | Message queues shall reject oversized sends beyond configured message size. | `kernel/ipc/mq.c` | `tests/test_kernel.c` | Covered |
| REQ-MQ-002 | Message queues shall reject undersized receives without consuming the message. | `kernel/ipc/mq.c` | `tests/test_reliability.c` | Covered |
| REQ-EVT-001 | Event waits shall support AND, OR, and clear-on-consume modes. | `kernel/ipc/event.c` | `tests/test_kernel.c` | Covered |
| REQ-PIPE-001 | Pipes shall return transferred byte count or a negative error code. | `kernel/ipc/pipe.c` | `tests/test_kernel.c`, `tests/test_reliability.c` | Covered |
| REQ-NOTIFY-001 | Task notifications shall support direct task signaling and wait/take patterns. | `kernel/ipc/notify.c` | `tests/test_stress.c` | Covered |

## Memory and Diagnostics

| ID | Requirement | Source ownership | Verification | Status |
|---|---|---|---|---|
| REQ-HEAP-001 | Heap allocations shall satisfy `AIXOS_CFG_ALIGNMENT`. | `kernel/heap.c` | `tests/test_heap.c` | Covered |
| REQ-HEAP-002 | Adjacent heap frees shall coalesce. | `kernel/heap.c` | `tests/test_heap.c`, `tests/test_reliability.c` | Covered |
| REQ-HEAP-003 | Runtime heap lockdown shall disable direct application heap allocation. | `kernel/heap.c`, `kernel/task.c` | `tests/test_reliability.c` | Covered |
| REQ-MEMPOOL-001 | Fixed-block pools shall reject double free and foreign pointers. | `kernel/mempool.c` | `tests/test_heap.c` | Covered |
| REQ-TRACE-001 | Trace records shall be bounded by the configured ring buffer. | `kernel/trace.c` | `tests/test_reliability.c`, `tools/trace_viewer.mjs` | Partial |
| REQ-CRASH-001 | Crash records shall be ABI-sized and CRC-validated. | `kernel/crash.c`, `include/aixos/abi.h` | `tests/test_reliability.c`, `tools/diagnostics_decode.mjs` | Covered |

## MPU, User Mode, and Microkernel

| ID | Requirement | Source ownership | Verification | Status |
|---|---|---|---|---|
| REQ-MPU-001 | User task stacks shall be registered as read/write user regions. | `kernel/task.c`, `kernel/mpu.c`, `arch/*/port.c` | `tests/test_mpu.c`, `tests/test_microkernel.c` | Covered |
| REQ-MPU-002 | User regions shall be naturally aligned power-of-two ranges. | `kernel/mpu.c` | `tests/test_mpu.c` | Covered |
| REQ-MPU-003 | Writable user regions shall also be readable. | `kernel/mpu.c` | `tests/test_mpu.c` | Covered |
| REQ-MPU-004 | Kernel services shall copy user buffers through audited user-copy helpers. | `kernel/microkernel.c`, `kernel/mpu.c` | `tests/test_mpu.c`, `tests/test_microkernel.c` | Covered |
| REQ-CAP-001 | Capabilities shall enforce rights for user-accessible kernel objects. | `kernel/microkernel.c`, `kernel/namespace.c` | `tests/test_microkernel.c` | Partial |
| REQ-CAP-002 | Capabilities shall support close/release semantics. | `kernel/microkernel.c` | `tests/test_microkernel.c` | Partial |
| REQ-IPC-SYNC-001 | Synchronous message IPC shall support send, receive, reply, connect, and disconnect. | `kernel/microkernel.c` | `tests/test_microkernel.c` | Partial |

## Architecture Ports

| ID | Requirement | Source ownership | Verification | Status |
|---|---|---|---|---|
| REQ-ARM-001 | Cortex-M3 port shall build with warnings as errors. | `arch/arm/cortex-m3/` | `make arm` | Covered |
| REQ-ARM-002 | Cortex-M3 Renode smoke shall show ticking and runnable tasks. | `arch/arm/cortex-m3/`, `tests/renode_cortexm3.robot` | `make renode` | Partial |
| REQ-IRQ-001 | ISR nesting accounting shall be atomic, bounded by configuration, and defer scheduling until the outermost ISR exits. | `kernel/isr.c`, `arch/arm/cortex-m3/portasm.s` | `tests/test_reliability.c`, `make test`, `make arm` | Covered |
| REQ-RV-001 | RV32IM port shall build as ELF32 RISC-V and expose trap/heartbeat symbols. | `arch/risc-v/` | `make riscv-validate RISCV_PREFIX=riscv64-elf-` | Covered |
| REQ-RV-002 | RISC-V trap frames shall preserve x1-x31, `mepc`, and `mstatus`. | `arch/risc-v/portasm.s`, `examples/smoke/main.c` | `tests/renode_riscv.robot` | Partial |

## Evidence Gaps

The following items are intentionally tracked as gaps for v1.0 hardening:

- Hardware-in-the-loop tests for at least one Cortex-M and one RISC-V target.
- Measured interrupt latency, context-switch latency, IPC latency, and timer
  dispatch latency on hardware.
- Long-run timer, IPC, task, heap, and reset stress logs.
- Certification-ready requirements, assumptions of use, and safety manual.
- Full POSIX semantic conformance beyond the supported subset.
