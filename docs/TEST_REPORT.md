# AIXOS Test Report

Date: 2026-06-29

## Scope

This report covers the current white-box and black-box validation pass for AIXOS using host instruction-level tests, LLVM coverage, static analysis, and Renode simulation.

## Test Inventory

- Host unit/integration test entries: 34
- Host assertions/checks: 7978
- New coverage expansion areas:
  - Crash, panic, reset, timer service, kernel start, task return trap
  - Task signals, wake recheck, scheduler and lifecycle edges
  - POSIX thread lifecycle, TLS destructors, cond/rwlock/timer/mqueue edges
  - Mutex waiter and priority inheritance paths
  - Pipe, message queue, event, notification, ISR/context parameter boundaries
  - Heap/object boundary and generation handling
  - Microkernel user syscall wrappers and fast syscall host dispatch

## Results

| Command | Result |
| --- | --- |
| `make test` | PASS, 7978 checks, 0 failures |
| `make coverage` | PASS, report generated at `build/host-coverage/coverage.txt` |
| `make analyze` | PASS |
| `make renode` | PASS, Cortex-M3 heartbeat=20 ticks=198 user=29 errors=0 |

## Coverage Summary

Full report target: `kernel compat/posix posix/src tests`

| Metric | Coverage |
| --- | ---: |
| Regions | 64.75% |
| Functions | 97.33% |
| Lines | 90.49% |
| Branches | 67.24% |

Product-code-only target: `kernel compat/posix posix/src`

| Metric | Coverage |
| --- | ---: |
| Regions | 85.05% |
| Functions | 99.76% |
| Lines | 86.93% |
| Branches | 68.50% |

## Fixes Made During Testing

- Fixed mutex deletion with waiters: `aixos_mutex_delete()` now returns `AIXOS_ERR_BUSY` instead of transferring ownership to a task while deleting the mutex object.
- Added host-test-only return paths for non-returning firmware paths so simulator tests can cover crash/start/exit paths without changing target firmware behavior.
- Added host-test-only waiter injection hooks for pipe, message queue, and event objects to cover wake/delete-busy paths safely in host simulation.
- Forced fast syscall host tests to use the dispatcher instead of executing native `svc`/`ecall` instructions.

## Gap Assessment

The requested function coverage target is met. Line coverage improved substantially but is not yet at 95%. The remaining gap is concentrated in:

- POSIX compatibility error branches and multi-party wait loops
- Microkernel synchronous IPC receiver/reply edge paths
- Branch-heavy test macros and inline list helpers that inflate region/branch totals
- Blocking paths that require either a real scheduler context switch or controlled host-only hooks

