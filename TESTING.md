# AIXOS v1.0 Testing and Verification Guide

This document defines the minimum verification flow for customer integration
and release qualification. Commands are run from the package root.

## Verification Layers

| Layer | Command | Purpose |
|---|---|---|
| Tool inventory | `make toolcheck` | Records compiler and emulator availability |
| Host unit tests | `make test` | Builds and runs native tests through `tests/host/arch_host.c` |
| MPU focused test | `make test-mpu` | Runs only MPU region and user-memory access checks |
| Host sanitizer tests | `make test-asan` | Runs host tests with ASan and UBSan |
| Host optimization variants | `make test-o2`, `make test-os` | Runs host tests with alternate optimization levels |
| ARM Cortex-M build | `make arm` | Builds the selected Cortex-M platform from `make config` or `AIXOS_PLATFORM` |
| Cortex-A55 build | `make arm AIXOS_PLATFORM=cortex-a55` | Builds and links the AArch64 ELF |
| ARM Cortex platform load check | `make renode-arm-platform-check` | Verifies Renode can load M0/M3/M4/M33/A55 platform descriptions |
| RV32IM build | `make riscv` | Builds strict RISC-V firmware |
| RV32IM ELF validation | `make riscv-validate` | Verifies ELF architecture and required symbols |
| ARM Cortex-M simulation | `make renode` | Runs Renode heartbeat and tick checks for the selected Cortex-M platform |
| ARM Cortex simulation | `make renode-arm-smoke` | Runs M0/M3/M4/M33/A55 Renode heartbeat and tick checks |
| Cortex-A55 instruction smoke | `make instruction-sim` | Runs A55 Renode instruction-level heartbeat, tick, user, and error metrics |
| RV32IM simulation | `make renode-riscv` | Runs Renode heartbeat, tick, trap, and register checks |
| RV32IM stress simulation | `make renode-riscv-stress` | Repeats the RISC-V Renode test five times |
| Static analysis | `make analyze` | Runs Clang static analysis |
| Coverage | `make coverage` | Generates LLVM coverage report |
| RAM report | `make ram-report` | Reports image memory use |
| Manifest | `make manifest` | Generates source and build metadata |
| Evidence package | `make evidence-package RISCV_PREFIX=riscv64-elf-` | Archives verification logs, reports, config, docs, ELF, and map files |
| Latency benchmark build | `make latency-bench RISCV_PREFIX=riscv64-elf-` | Builds latency benchmark firmware for Cortex-M3 and RV32IM |
| Full quality sweep | `make quality` | Runs the configured release-quality target set |

If the RISC-V compiler is installed with a non-default command prefix, pass it
to every RISC-V target, for example:

```sh
make riscv-validate RISCV_PREFIX=riscv64-elf-
make renode-riscv RISCV_PREFIX=riscv64-elf-
```

## Required Regression Coverage

The customer release qualification should include these behaviors:

- Object slot reuse invalidates stale generation handles.
- Heap allocations are aligned to `AIXOS_CFG_ALIGNMENT`.
- Adjacent heap frees coalesce and restore available capacity.
- Zero-timeout waits never block.
- A blocked task is present in exactly one object wait list and at most one
  timeout list.
- Posting an IPC object wakes one eligible waiter and removes its timeout
  entry.
- Blocking queue and pipe operations recheck their condition after wakeup.
- Task-only APIs reject ISR context and FromISR APIs reject task context.
- Timer callbacks do not execute from the tick ISR.
- A timed-out mutex waiter removes inherited priority from the owner.
- Deleting a task that owns a mutex is rejected.
- Queue receive rejects an undersized destination without consuming a message.
- Static task, queue, pipe, and fixed-block-pool storage is not freed by the
  kernel.
- Time-slice expiry requests a context switch without replacing `g_cur_task`
  before the architecture saves its context.
- Cortex-M3 PendSV reads `stack_top` at TCB offset zero.
- RISC-V trap frames preserve x1-x31, `mepc`, and `mstatus` with 16-byte stack
  alignment.
- Renode platform descriptions for Cortex-M0, Cortex-M3, Cortex-M4,
  Cortex-M33, and Cortex-A55 load without platform-description errors.

## Customer Release Criteria

A customer firmware release should not be accepted until all applicable items
are complete:

1. `make quality` passes in a clean checkout or release package.
2. Cortex-M3 and/or RV32IM firmware builds with the exact customer
   configuration.
3. Renode regression passes for the selected architecture.
4. Hardware-in-the-loop tests pass on at least one target board revision.
5. Worst-case interrupt latency and context-switch latency are measured on the
   target clock configuration.
6. Long-run stress tests cover timer, IPC, heap, task, and reset paths.
7. The release archive contains the ELF, map file, configuration, compiler
   identity, source manifest, and test logs.

## Evidence Package

Use:

```sh
make evidence-package RISCV_PREFIX=riscv64-elf-
```

The generated `build/evidence-package/` directory contains:

- command logs for toolcheck, host tests, analysis, cross-builds, coverage,
  RAM report, and manifest;
- qualified configuration and version headers;
- API contract, testing guide, requirements traceability, and POSIX
  compatibility documents;
- ARM and RISC-V ELF/map files for the evidence run.

Archive this directory outside the source package for customer release records.

## Latency Benchmark

Use:

```sh
make latency-bench RISCV_PREFIX=riscv64-elf-
```

The benchmark firmware exports symbols documented in
`benchmarks/latency/README.md`. v1.0 records sample counters first; hardware
cycle counters, GPIO pulse timing, or simulator-specific metric adapters should
be added per target.

## Known Verification Limits

- The host architecture shim does not execute real context switches.
- Host blocking tests inspect state transitions but do not execute real stack
  switches.
- Full IPC and timing interleavings require Renode and hardware tests.
- Physical-board validation is product-specific and not included in this source
  package.
- Cortex-A55 Renode checks validate platform load, AArch64 ELF loading,
  GIC/generic-timer ticks, kernel heartbeat, user task heartbeat, and zero user
  syscall errors. Hardware interrupt latency and cache/MMU behavior still need
  board-specific validation.
- The RISC-V toolchain can be supplied through `RISCV_TOOLCHAIN_DIR`.
- Renode availability depends on the local Renode installation and robot-server
  configuration.
