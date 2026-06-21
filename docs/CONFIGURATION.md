# AIXOS v1.0 Configuration Guide

Most product-level tuning is done through `make config`, which writes
`config/aixos_user_cfg.h`. The base defaults and validation checks remain in
`config/aixos_cfg.h`.

For non-interactive or CI builds, pass compile-time overrides through
`CONFIG_CFLAGS`, for example:

```sh
make arm CONFIG_CFLAGS=-DAIXOS_CFG_SCHEDULER=AIXOS_CFG_SCHED_SIMPLE
```

Use `make oldconfig` to print the current menu-configurable values without
changing the file.

## Priority and Task Limits

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_MAX_PRIORITY` | Number of priority levels |
| `AIXOS_CFG_IDLE_PRIORITY` | Idle task priority, currently required to be `0` |
| `AIXOS_CFG_TIMER_TASK_PRIORITY` | Timer service task priority |
| `AIXOS_CFG_TASK_HANDLE_LIMIT` | Task handle slot capacity |
| `AIXOS_CFG_TASK_SLOT_PAGE_SIZE` | Dynamic task slot allocation page size |
| `AIXOS_CFG_CAPS_PER_TASK` | Per-task capability table size |

The current handle format requires `AIXOS_CFG_TASK_HANDLE_LIMIT == 256`.

## Object Pool Sizing

Configure the maximum number of kernel objects with:

- `AIXOS_CFG_MAX_SEM`
- `AIXOS_CFG_MAX_MUTEX`
- `AIXOS_CFG_MAX_MQ`
- `AIXOS_CFG_MAX_EVENT`
- `AIXOS_CFG_MAX_PIPE`
- `AIXOS_CFG_MAX_TIMER`

For production builds, size pools from the product resource model and leave
margin for diagnostics and recovery paths.

## Timing

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_TIME_SLICE_TICKS` | Round-robin time slice |
| `AIXOS_CFG_SYSTICK_HZ` | Kernel tick frequency |
| `AIXOS_CFG_CPU_CLOCK_HZ` | CPU clock used by the Cortex-M SysTick setup |

Changing tick frequency affects timeout resolution, timer callback latency,
CPU overhead, and test expectations.

## Interrupt Response

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_KERNEL_IRQ_PRIORITY` | Cortex-M `BASEPRI` threshold used by kernel critical sections |
| `AIXOS_CFG_SYSTICK_IRQ_PRIORITY` | SysTick exception priority; must be maskable by the kernel threshold |
| `AIXOS_CFG_PENDSV_IRQ_PRIORITY` | PendSV exception priority; must be no more urgent than SysTick |
| `AIXOS_CFG_ISR_NESTING_MAX` | Software-supported ISR nesting budget before recording an overflow |
| `AIXOS_CFG_ISR_NESTING_PANIC` | Escalates ISR nesting overflow to `aixos_panic()` when enabled |

The default Cortex-M3 threshold is `0x40`. Interrupts with a numerically lower
priority value, such as `0x00` through `0x30` on common STM32F103-style
4-bit-priority parts, can preempt kernel critical sections and provide a
high-response lane. Those high-response ISRs must not call AIXOS kernel APIs;
they should clear hardware status, copy minimal data into a lock-free/product
owned buffer, and defer RTOS interaction to a lower-priority interrupt or task.

The default `AIXOS_CFG_ISR_NESTING_MAX` is `8`, which is a software diagnostic
budget, not a hardware proof. Product firmware must size the interrupt stack
from worst-case exception frames and compiler-saved registers, then adjust this
limit to match the validated stack budget.

## Scheduler Backend

| Macro | Value | Purpose |
|---|---|---|
| `AIXOS_CFG_SCHEDULER` | `AIXOS_CFG_SCHED_BITMAP` | Default bitmap multi-queue scheduler |
| `AIXOS_CFG_SCHEDULER` | `AIXOS_CFG_SCHED_SIMPLE` | Single sorted ready-list scheduler |

The bitmap backend uses one ready queue per priority plus a non-empty priority
bitmap. It keeps ready insertion, removal, and highest-priority selection
constant-time for the configured priority range, at the cost of more scheduler
RAM.

The simple backend is modeled after the small-system scheduler option used by
Zephyr's `CONFIG_SCHED_SIMPLE`: one priority-sorted ready list with the best
thread at the head. It reduces scheduler data and code size, while insertion
and priority requeue become linear in the number of ready tasks.

Run `python3 tools/scheduler_benchmark.py` to rebuild both Cortex-M3 variants
and refresh `build/reports/scheduler_compare.csv`.

## Stack Sizing

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_IDLE_STACK_SIZE` | Idle stack in `uint32_t` words |
| `AIXOS_CFG_TIMER_STACK_SIZE` | Timer service stack size |
| `AIXOS_CFG_DEFAULT_STACK_SIZE` | Default task stack size in bytes |
| `AIXOS_CFG_MIN_TASK_STACK_SIZE` | Architecture minimum initial-context stack size |
| `AIXOS_CFG_STACK_GUARD_BYTES` | Stack guard area |

Customer firmware should measure high-water stack use under worst-case
interrupt, timer, and IPC load before finalizing stack sizes.

## Heap and Memory Pools

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_HEAP_SIZE` | Internal heap size |
| `AIXOS_CFG_HEAP_MAGIC` | Heap integrity marker |
| `AIXOS_CFG_HEAP_LOCK_ON_START` | Locks runtime heap allocation after startup |
| `AIXOS_CFG_ALIGNMENT` | Heap and guard alignment |

Use fixed-block pools for deterministic runtime allocation. Keep heap use in
startup and controlled system-management paths.

## MPU and PMP Protection

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_ENABLE_MPU` | Enables per-task memory protection |
| `AIXOS_CFG_MPU_REGIONS_PER_TASK` | Portable per-user-task region count |
| `AIXOS_CFG_MPU_MIN_REGION_SIZE` | Minimum region size |
| `AIXOS_CFG_FLASH_BASE` | Cortex-M executable code region base |
| `AIXOS_CFG_FLASH_SIZE` | Cortex-M executable code region size |
| `AIXOS_CFG_RAM_BASE` | Cortex-M RAM base |
| `AIXOS_CFG_RAM_SIZE` | Cortex-M RAM size |
| `AIXOS_CFG_RISCV_RAM_BASE` | RISC-V executable RAM region base |
| `AIXOS_CFG_RISCV_RAM_SIZE` | RISC-V executable RAM region size |

The portable profile currently exposes three per-task regions. A user task uses
one region for its stack by default, leaving two regions for application-owned
user buffers or shared memory. Customer firmware that needs more user regions
must update both the portable limit and the architecture PMP/MPU programming
policy.

## Trace and Diagnostics

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_TRACE_ENABLE` | Enables trace recording |
| `AIXOS_CFG_TRACE_BUFFER_SIZE` | Trace ring size |
| `AIXOS_CFG_CPU_USAGE_ENABLE` | Enables CPU usage accounting |
| `AIXOS_CFG_CRASH_MAGIC` | Crash record magic |

Trace buffers consume RAM but are useful during integration. Production
firmware should set a trace policy that matches diagnostic and privacy
requirements.

## IPC and POSIX Limits

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_MAX_IPC_COPY_BYTES` | Maximum regular IPC copy size |
| `AIXOS_CFG_ISR_COPY_MAX_BYTES` | Maximum copy size from ISR APIs |
| `AIXOS_CFG_MQ_PRIORITY_MAX` | Maximum message queue priority |
| `AIXOS_CFG_POSIX_KEYS` | POSIX key capacity |
| `AIXOS_CFG_POSIX_RWLOCK_READERS` | RW lock reader table size |
| `AIXOS_CFG_POSIX_OPEN_MAX` | POSIX open object limit |
| `AIXOS_CFG_POSIX_TIMERS` | POSIX timer pool size |
| `AIXOS_CFG_POSIX_PIPE_CAPACITY` | Default POSIX pipe capacity |

Keep ISR copy limits small enough for interrupt-latency targets.

## Optional Subsystems

| Macro | Purpose |
|---|---|
| `AIXOS_CFG_ENABLE_SIGNALS` | Enables task signal APIs |
| `AIXOS_CFG_ENABLE_NAMESPACE` | Enables resource namespace APIs |
| `AIXOS_CFG_ENABLE_TIME_WHEEL` | Enables hierarchical timeout wheel |
| `AIXOS_CFG_ENABLE_PANIC_RESET` | Enables controlled panic reset path |

Disable unused subsystems only after compiling and testing the product
configuration.

## Minimal Profile

Set `AIXOS_CFG_PROFILE_MINIMAL=1` at compile time to reduce several object and
diagnostic capacities. Validate every product workload after enabling this
profile because capacity limits change.
