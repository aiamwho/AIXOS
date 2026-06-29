# AIXOS v1.0 Troubleshooting Guide

This guide lists common integration failures and the checks that normally
identify the cause.

## Build Failures

### RISC-V compiler not found

Symptom:

```text
missing RISC-V compiler: riscv-none-elf-gcc
```

Fix:

```sh
make riscv RISCV_PREFIX=riscv64-elf-
```

or:

```sh
make riscv RISCV_TOOLCHAIN_DIR=/opt/xpack-riscv-none-elf-gcc
```

Run `make toolcheck` to record the installed compiler names.

### Linker overflow

Symptoms include `.text`, `.data`, `.bss`, heap, or stack sections not fitting
the linker memory map.

Checks:

- Review the target linker script under `arch/<target>/`.
- Run `make ram-report`.
- Reduce object limits in `config/aixos_cfg.h` only after confirming the
  product resource model.
- Recheck stack sizes against measured high-water marks.

## Startup and Scheduler Issues

### Firmware starts but no task runs

Checks:

- Confirm startup clears BSS and copies data before calling `main()`.
- Confirm `aixos_object_init()`, `aixos_sched_init()`, `aixos_task_init()`, and
  `aixos_timer_init()` run before `aixos_start()`.
- Confirm at least one application task was created successfully.
- Check that task priorities are below `AIXOS_CFG_MAX_PRIORITY`.
- Check that stack sizes are at least `AIXOS_CFG_MIN_TASK_STACK_SIZE`.

### Tick counter stays at zero

Checks:

- Confirm the target tick interrupt is enabled.
- Confirm CPU clock and tick configuration match the board clock.
- Confirm interrupt priority or trap masking allows the tick interrupt.
- On Cortex-M, check SysTick and PendSV setup.
- On RISC-V, check CLINT `mtimecmp`, `mie`, `mstatus`, and trap vector setup.

## Task and Stack Issues

### `aixos_task_create()` returns `AIXOS_HANDLE_INVALID`

Checks:

- Heap is initialized and large enough.
- `stack_size >= AIXOS_CFG_MIN_TASK_STACK_SIZE`.
- Task handle slots are available.
- Runtime heap lockdown is not blocking the attempted dynamic allocation.
- For production firmware, use `aixos_task_create_static()` when possible.

### Stack guard failure

Checks:

- Increase task stack size and rerun worst-case workload.
- Avoid large stack arrays in timer callbacks and ISR paths.
- Inspect recursive calls and deeply nested parser paths.
- Use `aixos_task_get_info()` to read stack diagnostics where supported.

## IPC Issues

### Non-blocking wait returns immediately

This is expected when `timeout_ms == 0`. Use `UINT32_MAX` for indefinite wait
or a positive millisecond timeout for bounded waits.

### Blocking API returns `AIXOS_ERR_LOCKED`

The scheduler is locked. Release scheduler locks before calling APIs that can
block, such as semaphore wait, mutex lock, queue receive, pipe read, or task
sleep.

### ISR API rejects a large message

ISR copy size is limited by `AIXOS_CFG_ISR_COPY_MAX_BYTES`. Keep ISR payloads
small and hand off larger work to a task through a semaphore, notification, or
short pipe write.

## MPU and User-Mode Issues

### `aixos_task_mpu_region_add()` returns `AIXOS_ERR_INVAL`

Checks:

- Region size is a power of two.
- Region size is at least `AIXOS_CFG_MPU_MIN_REGION_SIZE`.
- Base address is aligned to region size.
- Writable regions also include `AIXOS_MPU_READ`.
- The task handle is valid and refers to a user task.

### User buffer access returns `AIXOS_ERR_FAULT`

Checks:

- Register the exact buffer range with `aixos_task_mpu_region_add()`.
- Ensure the requested access size stays inside the registered range.
- Ensure write access is granted for output buffers.
- Do not pass kernel stack, TCB, heap metadata, or object pool addresses to
  user tasks.

## Renode Issues

### Cortex-M Renode test fails heartbeat or tick assertion

Checks:

- Rebuild with `make arm`.
- Confirm `examples/smoke/main.c` or customer `APP_SRCS` creates runnable
  tasks before `aixos_start()`.
- Confirm the ELF path in the Robot script points to the current build output.
- Confirm the platform file matches the linker memory map.

### RISC-V Renode runner times out before tests start

If the failure says:

```text
Couldn't access port file for Renode instance
```

then Robot did not reach the firmware test body. Check local Renode
installation, robot-server startup, temporary directory permissions, and port
file handling before treating it as a firmware regression.

### Instruction benchmark logs null-address memory accesses

Repeated warnings such as `ReadDoubleWord from non existing peripheral at 0x0`
inside `aixos_timing_wheel_tick()` usually mean an application enabled
`AIXOS_CFG_ENABLE_TIME_WHEEL` but did not call `aixos_timing_wheel_init()`
before starting the scheduler.

Fix the application initialization sequence:

```c
aixos_timer_init();
#if AIXOS_CFG_ENABLE_NAMESPACE
aixos_namespace_init();
#endif
#if AIXOS_CFG_ENABLE_TIME_WHEEL
aixos_timing_wheel_init();
#endif
aixos_sched_init();
```

## Diagnostics Collection

For customer support, collect:

- Exact command and full terminal output.
- Compiler version from `make toolcheck`.
- `config/aixos_cfg.h`.
- Linker script and map file.
- ELF file for the failing target.
- Renode Robot output or hardware log.
- Crash record and trace snapshot if available.
