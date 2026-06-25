# AIXOS v1.0 Architecture Guide

This guide summarizes the architecture delivered in the customer package. It is
written for engineers integrating, reviewing, or extending AIXOS.

## System Model

AIXOS is a small embedded RTOS kernel with fixed-priority scheduling, generation
checked object handles, bounded IPC primitives, optional POSIX compatibility,
and target ports for Cortex-M3 and RV32IM.

The kernel is organized around these responsibilities:

| Area | Source | Responsibility |
|---|---|---|
| Object model | `kernel/object.c` | Object pools, handle generation, type checks |
| Scheduler | `kernel/sched.c`, `kernel/task.c` | Ready queues, task states, timeout lists, priority handling |
| IPC | `kernel/ipc/` | Semaphore, mutex, message queue, event, pipe, notify |
| Timers | `kernel/timer.c`, `kernel/timewheel.c` | Software timers and timeout bookkeeping |
| Memory | `kernel/heap.c`, `kernel/mempool.c` | Heap allocator and fixed-block pools |
| MPU | `kernel/mpu.c` | User-task memory region metadata and software access checks |
| Diagnostics | `kernel/trace.c`, `kernel/crash.c` | Trace records, crash records, reset diagnostics |
| Microkernel services | `kernel/microkernel.c`, `kernel/namespace.c` | User/kernel domain checks, capabilities, namespace lookup |
| Architecture ports | `arch/` | Startup, context switching, trap/interrupt entry, linker scripts |
| POSIX compatibility | `compat/posix/`, `posix/` | POSIX-style API wrappers and headers |

## Boot and Initialization

The expected boot sequence is:

1. Architecture startup code initializes memory sections and the C runtime
   assumptions required by the port.
2. Kernel object and scheduler subsystems are initialized.
3. System tasks and application tasks are created.
4. Timers, IPC objects, and memory pools required by startup are created.
5. `aixos_start()` transfers control to the scheduler.

The sample firmware entry is `examples/smoke/main.c`. Customer firmware can
replace this through `APP_SRCS=/path/to/customer/main.c` as long as the same
kernel initialization order is preserved.

## Task and Scheduler Model

Tasks are represented by `aixos_tcb_t` in `include/aixos/task.h`. The first TCB
field is `stack_top`; architecture assembly depends on this offset and it must
not be moved.

The scheduler uses:

- Fixed integer priorities.
- Configurable time slicing through `AIXOS_CFG_TIME_SLICE_TICKS`.
- Compile-time selectable ready-queue backend through `AIXOS_CFG_SCHEDULER`.
- Explicit task states: ready, running, blocked, delayed, suspended, and stop.
- Timeout wait nodes for sleep and blocking IPC.
- Priority inheritance for mutex ownership.

The idle task must remain at priority `0`. The timer service task priority is
configured by `AIXOS_CFG_TIMER_TASK_PRIORITY`.

Two scheduler backends coexist:

- `AIXOS_CFG_SCHED_BITMAP`: one FIFO ready queue per priority plus a bitmap of
  non-empty priorities. This is the default deterministic path and favors
  larger runnable sets.
- `AIXOS_CFG_SCHED_SIMPLE`: a single priority-sorted ready list inspired by
  Zephyr's simple scheduler option. It favors small runnable sets and reduced
  memory footprint; priority insertion and requeue scan the ready list.

## Handle and Object Model

Public handles are 32-bit signed values:

- Low 8 bits: object slot index.
- High 24 bits: generation.

The generation check rejects stale handles after slot reuse. Object APIs also
verify the expected object type before operating on the slot.

## IPC Model

The IPC layer includes:

- Semaphores for counting resource availability.
- Mutexes with ownership and priority inheritance.
- Message queues with bounded copy size and priority-aware ordering.
- Event flags with AND/OR and optional clear-on-consume behavior.
- Pipes for byte-stream transfer.
- Task notification for lightweight task-directed signaling.

Blocking IPC operations must be called from task context. ISR-safe APIs are
provided only where explicitly declared.

## Timer Model

AIXOS tracks system time in scheduler ticks. Millisecond APIs are converted to
ticks with wrap-safe timeout comparisons. Software timers are dispatched outside
the tick ISR through the timer service path. Timer callbacks must remain
bounded and must not assume ISR context.

## Memory Model

The package provides:

- A heap allocator with alignment, corruption checks, and optional runtime
  allocation lock.
- Fixed-block memory pools for deterministic runtime allocation.
- Static object ownership rules for customer-provided storage.
- Per-user-task memory protection regions backed by Cortex-M MPU or RISC-V PMP
  where the target supports them.

Production firmware should prefer static creation and fixed-block pools after
initialization.

## Memory Protection Model

MPU support is enabled by `AIXOS_CFG_ENABLE_MPU`. User tasks receive a default
read/write, non-executable stack region. Product code can add additional
per-task user regions through `aixos_task_mpu_region_add()`.

Region constraints are intentionally portable across Cortex-M3 MPU and RV32 PMP:

- Region size must be a power of two.
- Region size must be at least `AIXOS_CFG_MPU_MIN_REGION_SIZE`.
- Region base must be naturally aligned to region size.
- Writable regions must also be readable.
- Each user task has at most `AIXOS_CFG_MPU_REGIONS_PER_TASK` regions.

The scheduler calls `aixos_arch_mpu_configure_task()` whenever the running task
changes. Kernel tasks run privileged and use the architecture privileged default
mapping. User tasks run with only the global executable code region plus their
task-specific MPU/PMP regions.

Syscall handlers treat user pointers as untrusted. Current-task transfers use
`aixos_copy_from_user()`, `aixos_copy_to_user()`, and `aixos_zero_to_user()` so
MPU/PMP range validation and the actual copy stay in one audited path. Delayed
kernel writes, such as a reply copied back to a blocked sender, validate against
the original sender task rather than the server task that performs the reply.

## Diagnostics

Trace entries and crash records are ABI-visible records guarded by compile-time
size checks. Customer firmware should persist or export crash records through
the product diagnostic channel before clearing them.

## Architecture Ports

The delivered ports are:

- `arch/arm/cortex-m3/`
- `arch/arm/aarch64/`
- `arch/risc-v/`
- `tests/host/` for native unit tests

Renode platform descriptions cover Cortex-M0, Cortex-M3, Cortex-M4,
Cortex-M33, and Cortex-A55 under `simulation/`; see
`docs/RENODE_ARM_PLATFORMS.md` for the compatibility matrix. The Cortex-M0,
Cortex-M3, Cortex-M4, and Cortex-M33 platforms are selectable through
`make config` or `AIXOS_PLATFORM=<name>` and have Renode smoke coverage.
Cortex-A55 has an AArch64 linked ELF port with Renode smoke coverage for
GIC/generic-timer ticks, kernel heartbeat, user task heartbeat, and syscall
error checks.

New ports must implement the architecture interface in
`arch/include/aixos/arch/arch.h`, provide startup code, supply a linker script,
and preserve scheduler and interrupt semantics documented in
`docs/PORTING_GUIDE.md`.

### Cortex-M Interrupt Model

The Cortex-M3 port uses `BASEPRI` for kernel critical sections instead of
globally masking all interrupts. `AIXOS_CFG_KERNEL_IRQ_PRIORITY` defaults to
`0x40`; interrupts configured with a numerically lower priority remain able to
preempt the kernel critical section path. SysTick defaults to the same priority
as the kernel threshold and PendSV defaults to `0xF0`, so context switching stays
deferred behind normal interrupt work.

ISR entry and exit maintain an atomic nesting counter. Rescheduling requested
from ISR context is deferred until the outermost ISR exits. The kernel also
tracks a high-watermark and records a crash record with reason
`AIXOS_CRASH_REASON_ISR_NESTING_OVERFLOW` when nesting exceeds
`AIXOS_CFG_ISR_NESTING_MAX`.

High-response ISRs above the kernel threshold are outside the kernel
service-API-safe zone. They may enter through the board/architecture ISR wrapper
for nesting accounting, but must not call AIXOS services, including
`*_from_isr`; use a lower-priority IRQ or task handoff for RTOS interaction.
