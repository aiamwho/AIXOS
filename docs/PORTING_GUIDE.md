# AIXOS v1.0 Porting Guide

This guide describes the work required to bring AIXOS to a new CPU, board, or
toolchain.

## Porting Checklist

1. Add a directory under `arch/<family>/<target>/` or `arch/<target>/`.
2. Implement the architecture hooks declared in
   `arch/include/aixos/arch/arch.h`.
3. Provide startup code, interrupt/trap entry, context switch assembly, and a
   linker script.
4. Define the tick source and interrupt priority policy.
5. Implement `aixos_arch_mpu_configure_task()` for the target MPU, PMP, or
   equivalent protection unit.
6. Preserve the TCB `stack_top` offset at zero.
7. Add a strict cross-build target to `Makefile`.
8. Add simulation or hardware smoke tests.
9. Run host, cross-build, static-analysis, and target tests.

## Required Architecture Responsibilities

A target port must provide:

- Initial stack frame construction for new tasks.
- Context save and restore.
- Scheduler yield or PendSV-equivalent trigger.
- Tick interrupt initialization.
- Interrupt enable, disable, and restore primitives.
- ISR context detection.
- Startup path that enters C code with valid data and BSS sections.
- Linker symbols required by the startup code and memory allocator.
- Memory protection programming for the currently selected task.

## Context Switch Contract

The scheduler assumes the old task remains current until the architecture layer
has saved its context. The architecture layer must not corrupt the outgoing
task stack or switch to a new task before `stack_top` has been stored.

The first field of `aixos_tcb_t` is `stack_top`. Assembly code may access this
field by fixed offset, so customer changes must preserve the layout contract.

## Tick and Timeout Contract

The tick interrupt must:

- Advance kernel time exactly once per tick event.
- Request scheduling when time slicing or timeout wakeups require it.
- Avoid dispatching timer callbacks directly from the tick ISR.
- Preserve all registers required by the target ABI.

## Interrupt Rules

Architecture ports must support the public context matrix in
`API_CONTRACT.md`:

- Task-only APIs must reject ISR context.
- FromISR APIs must not block.
- Critical sections must restore the previous interrupt state.

## Linker Script Requirements

The linker script must provide memory regions for code, read-only data,
initialized data, BSS, stack, and heap as required by the selected port. It
should also preserve diagnostic sections needed by crash and trace handling.

Customer firmware should archive the linker script with each qualified build.

## Memory Protection Requirements

New ports must apply task memory protection before returning to a user task.
The port should provide:

- A global executable code region for user instruction fetch.
- Per-task data regions from `task->mpu_regions`.
- Read/write, non-executable user stack protection.
- Privileged kernel access to kernel memory without exposing kernel memory to
  user mode.
- Fault entry that records memory protection violations through the crash path.

Cortex-M ports should program MPU regions. RISC-V ports should program PMP
entries before `mret` to U-mode.

## Validation

At minimum, a new port should pass:

```sh
make test
make analyze
make <new-target>
```

For release qualification, add:

- Simulator smoke test, if a simulator is available.
- Hardware tick and heartbeat test.
- Interrupt nesting and critical-section tests.
- Context-switch register preservation test.
- Stack overflow and crash-record test.
- Long-run IPC and timer stress test.
