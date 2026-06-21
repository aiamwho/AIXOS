# AIXOS v1.0 Development Guide

This guide defines the normal engineering workflow for customer-side AIXOS
development.

## Document Map

Read the customer documents in this order when starting a new product:

| Document | Purpose |
|---|---|
| `README.md` | Package contents and build target overview |
| `docs/HELLO_WORLD.md` | Minimal first task and UART output example |
| `docs/INTEGRATION_GUIDE.md` | End-to-end product integration procedure |
| `docs/API_REFERENCE.md` | Module-level public API usage |
| `API_CONTRACT.md` | API, ABI, ownership, timeout, and context rules |
| `docs/REQUIREMENTS_TRACEABILITY.md` | Requirement-to-test traceability |
| `docs/POSIX_COMPATIBILITY.md` | POSIX support and limitation matrix |
| `docs/CONFIGURATION.md` | Product configuration and sizing |
| `docs/PORTING_GUIDE.md` | New CPU, board, or toolchain porting |
| `TESTING.md` | Release verification matrix |
| `docs/TROUBLESHOOTING.md` | Common build, runtime, MPU, and Renode failures |

## Source Layout

| Path | Use |
|---|---|
| `include/aixos/` | Public kernel headers |
| `kernel/` | Kernel implementation |
| `kernel/ipc/` | IPC primitive implementation |
| `kernel/mpu.c` | User memory protection metadata and validation |
| `arch/include/` | Architecture abstraction |
| `arch/arm/cortex-m3/` | Cortex-M3 port |
| `arch/risc-v/` | RV32IM port |
| `config/` | Compile-time configuration |
| `examples/hello_world/` | Minimal first task and UART hook example |
| `examples/smoke/` | Default smoke-test firmware application |
| `posix/` | POSIX compatibility headers and source |
| `compat/posix/` | POSIX adapter layer |
| `tests/` | Host and Renode regression tests |
| `tools/` | Verification and build-support tools |
| `benchmarks/latency/` | Latency benchmark firmware entry |

## Build Prerequisites

Install the tools needed for the selected targets:

- C compiler for host tests, normally Apple Clang or Clang/GCC.
- `arm-none-eabi-gcc` for Cortex-M3 firmware.
- `riscv-none-elf-gcc` or another RV32 GCC selected with `RISCV_PREFIX` for
  RV32IM firmware.
- Renode with robot testing support for simulation targets.
- Node.js for `tools/build_manifest.mjs`.

Run:

```sh
make toolcheck
```

## Common Commands

```sh
make test
make arm
make riscv
make renode
make renode-riscv
make analyze
make coverage
make manifest
make evidence-package
make latency-bench
make clean
```

Use `RISCV_TOOLCHAIN_DIR` when the RISC-V compiler is not on `PATH`:

```sh
make riscv RISCV_TOOLCHAIN_DIR=/opt/xpack-riscv-none-elf-gcc
```

Use `RISCV_PREFIX` when the compiler is on `PATH` but uses a different command
prefix:

```sh
make riscv RISCV_PREFIX=riscv64-elf-
make renode-riscv RISCV_PREFIX=riscv64-elf-
```

The firmware targets use `APP_SRCS ?= examples/smoke/main.c`. Customer
firmware should supply its own entry point through `APP_SRCS`:

```sh
make arm APP_SRCS=/path/to/customer/main.c
make riscv APP_SRCS=/path/to/customer/main.c
```

## Adding Kernel Code

1. Add public declarations only when the function is part of the customer API.
2. Keep internal declarations in the owning kernel module.
3. Validate task/ISR context at API boundaries.
4. Use existing `AIXOS_ERR_*` values instead of inventing module-local return
   conventions.
5. Preserve bounded copy limits such as `AIXOS_CFG_MAX_IPC_COPY_BYTES`.
6. Update host tests before changing shared kernel behavior.
7. Update `API_CONTRACT.md` when API semantics or ABI-visible types change.
8. Update `docs/API_REFERENCE.md` when adding or changing public APIs.
9. Update `docs/INTEGRATION_GUIDE.md` when integration steps or required
   source sets change.

## Public API Rules

Public functions should:

- Return `AIXOS_OK` or a negative `AIXOS_ERR_*` value.
- Reject invalid handles and stale generation values.
- Reject unsupported contexts with `AIXOS_ERR_CONTEXT`.
- Avoid unbounded loops over customer-controlled data.
- Avoid retaining caller-owned buffers unless the API explicitly documents
  lifetime ownership.

## Memory Allocation Rules

Production code should create dynamic objects during initialization. Runtime
paths should prefer:

- Static object creation.
- Fixed-block pools.
- Caller-owned buffers.
- Bounded stack objects.

Do not allocate from ISR context. Do not use direct application heap allocation
after `AIXOS_CFG_HEAP_LOCK_ON_START` has locked the runtime heap.

## User Memory Protection Rules

User task stacks are automatically added as read/write, non-executable MPU
regions. Additional user buffers must be registered explicitly:

```c
aixos_task_mpu_region_add(task, (uintptr_t)buffer, buffer_size,
                          AIXOS_MPU_READ | AIXOS_MPU_WRITE);
```

The buffer base must be aligned to `buffer_size`, and `buffer_size` must be a
power of two. Use statically aligned storage for predictable customer firmware:

```c
static uint8_t user_buffer[256] __attribute__((aligned(256)));
```

Do not grant user access to kernel-owned objects, kernel stacks, object pools,
or allocator metadata.

When adding a syscall or user-facing kernel service, never dereference or
`memcpy()` a user pointer directly. Use:

- `aixos_copy_from_user()` for input buffers.
- `aixos_copy_to_user()` for output buffers.
- `aixos_zero_to_user()` when the kernel must clear user-visible storage.

Use `aixos_user_memory_check()` only when the service needs to remember a user
buffer for a later bounded transfer. The later transfer must still validate the
task that owns that buffer.

## Concurrency Rules

- Do not call blocking APIs while the scheduler is locked.
- Do not block from ISR context.
- Hold scheduler or interrupt locks only around short critical sections.
- Keep timer callbacks bounded.
- Ensure task deletion is coordinated by the application owner of resources.

## Test Requirements for Changes

Every shared kernel change should include:

- A host unit test where behavior can be checked without real context switches.
- A Renode test or existing Renode coverage when the behavior depends on
  architecture switching, ticks, or interrupt entry.
- Static analysis with `make analyze`.
- Cross-builds for all affected architectures.

## Release Artifacts

For each customer firmware release, archive:

- Source package revision or checksum.
- `config/aixos_cfg.h`.
- Compiler identity from `make toolcheck`.
- ELF and map file.
- `make manifest` output.
- Test logs and Renode results.
- Any customer-specific patches.

## Customer Handoff Checklist

Before handing a package to a customer:

1. Confirm the package contains no generated `build/` or `test-results/`
   directories.
2. Confirm `README.md`, `API_CONTRACT.md`, `TESTING.md`, and all files under
   `docs/` describe the current source tree.
3. Run the verification commands listed in `TESTING.md` for the architectures
   being delivered.
4. Record any simulator or hardware limitation as a verification note, not as
   a hidden assumption.
5. Archive the compiler identity, manifest, RAM report, and qualified binaries
   outside the source package when delivering binary evidence.
