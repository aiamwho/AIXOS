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
| API boundary simulation | `make api-boundary-sim RISCV_PREFIX=riscv64-elf-` | Runs scheduler-started API parameter and functional boundary checks on Cortex-M3 and RV32IM Renode targets; writes `test-results/api-boundary/report.md` and `test-results/api-boundary/report.zh-CN.md` |
| Static analysis | `make analyze` | Runs Clang static analysis |
| Coverage | `make coverage` | Generates LLVM coverage report |
| RAM report | `make ram-report` | Reports image memory use |
| Manifest | `make manifest` | Generates source and build metadata |
| Evidence package | `make evidence-package RISCV_PREFIX=riscv64-elf-` | Archives verification logs, reports, config, docs, ELF, and map files |
| Latency benchmark build | `make latency-bench RISCV_PREFIX=riscv64-elf-` | Builds latency benchmark firmware for Cortex-M3 and RV32IM |
| Instruction benchmark | `make instruction-bench RISCV_PREFIX=riscv64-elf-` | Builds AIXOS benchmark firmware and runs A55, Cortex-M3, and RV32IM Renode metric collection; FreeRTOS cases are skipped unless `third_party/FreeRTOS-Kernel` is present |
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
- Runtime API boundary checks cover task lifecycle, semaphore, mutex, message
  queue, event, pipe, task notification, software timer, heap lockdown,
  fixed-block mempool, MPU validation, and user syscall wrappers under Renode.
- White-box path tests cover namespace registration/capability/resource
  lifetime, timing-wheel level-1/level-2 insertion and cascade, microkernel
  synchronous IPC send/receive/reply/disconnect paths, and invalid-parameter
  matrices for public kernel APIs.
- Black-box workflow tests repeatedly exercise semaphore, message queue,
  event, pipe, notification, and timeout-oriented user scenarios with
  representative payload and boundary values.

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

## Current Local Verification Snapshot

The package was re-verified on 2026-06-29 with:

- Apple clang 21.0.0 for host tests.
- `arm-none-eabi-gcc` 16.1.0 for Cortex-M builds.
- `riscv64-elf-gcc` 16.1.0 for RV32IM builds.
- Renode 1.16.1.28836 for ARM Cortex, Cortex-A55, and RV32IM simulation.

Commands passed:

- `make quality RISCV_PREFIX=riscv64-elf-`
- `make test-mpu`
- `make latency-bench RISCV_PREFIX=riscv64-elf-`
- `make renode-arm-platforms RISCV_PREFIX=riscv64-elf-`
- `make renode-riscv-stress RISCV_PREFIX=riscv64-elf-`
- `make instruction-bench RISCV_PREFIX=riscv64-elf-`
- `make api-boundary-sim RISCV_PREFIX=riscv64-elf-`
- `make evidence-package RISCV_PREFIX=riscv64-elf-`

The host regression suite currently contains 23 named test functions and
reported `7207 checks, 0 failures` under the default, ASan/UBSan, `-O2`, `-Os`,
and LLVM coverage builds. The added white-box and black-box coverage is in
`tests/test_path_coverage.c`.

The LLVM coverage snapshot from this run reports total region coverage
`63.11%`, function coverage `83.91%`, line coverage `78.11%`, and branch
coverage `61.19%` across `kernel`, `compat/posix`, `posix/src`, and `tests`.

The `instruction-bench` run reported AIXOS Cortex-M3 benchmark
`heartbeat=50`, `messages=50`, `errors=0`, `ticks=499`, `switches=151`, and
AIXOS RV32IM benchmark `heartbeat=46`, `messages=46`, `errors=0`, `ticks=499`,
`switches=231`. FreeRTOS benchmark cases were skipped because
`third_party/FreeRTOS-Kernel` is not included in this source package.

The `api-boundary-sim` run reported 132 kernel boundary checks and 0 failures
on both Cortex-M3 and RV32IM, plus 797 user-mode syscall wrapper checks and 0
user failures on each target. That is 929 boundary checks per simulator target
and 1858 boundary checks across the two simulator targets. The generated
reports are
`test-results/api-boundary/report.md` and
`test-results/api-boundary/report.zh-CN.md`.

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
- FreeRTOS benchmark execution requires adding a pinned
  `third_party/FreeRTOS-Kernel` checkout; otherwise `instruction-bench` records
  FreeRTOS cases as skipped and only validates AIXOS benchmark paths.
