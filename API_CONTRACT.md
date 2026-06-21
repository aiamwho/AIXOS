# AIXOS v1.0 Public API Contract

This document defines the public API and ABI contract identified by
`AIXOS_API_VERSION == 0x00010000` and
`AIXOS_ABI_VERSION == 0x00010000`.

The contract applies to headers under `include/aixos/` and the POSIX
compatibility headers under `posix/include/`.

## Version Policy

- `AIXOS_VERSION_MAJOR`, `AIXOS_VERSION_MINOR`, and `AIXOS_VERSION_PATCH`
  identify the source package version.
- `AIXOS_API_VERSION` changes when public API semantics or declarations change.
- `AIXOS_ABI_VERSION` changes when ABI-visible structure layout, handle
  encoding, binary record format, or calling convention changes.
- Customer firmware should archive the version header, map file, ELF, compiler
  identity, configuration, and source manifest together for each qualified
  image.

## Common API Rules

- `timeout_ms == 0` is non-blocking.
- `timeout_ms == UINT32_MAX` waits indefinitely.
- Other timeouts are rounded up to ticks and capped below half of the 32-bit
  tick range so wrap-safe comparisons remain valid.
- APIs returning `AIXOS_ERR_CONTEXT` are forbidden in the current execution
  context.
- APIs returning `AIXOS_ERR_LOCKED` would block or remove the current task
  while the scheduler is locked.
- Handles are generation-checked. A deleted, stale, or wrong-type handle is
  invalid.
- User buffers remain owned by the caller unless a static-create API explicitly
  retains them for the lifetime of the object.
- APIs do not accept overlapping source and destination buffers unless the
  header explicitly states otherwise.
- Ordinary task APIs are serialized internally against interrupts, but the
  application remains responsible for passed buffer lifetime and ownership.

## Error Codes

Public APIs return `AIXOS_OK` on success or a negative `AIXOS_ERR_*` value on
failure. The common error values are defined in `include/aixos/types.h`.

| Error | Meaning |
|---|---|
| `AIXOS_ERR_TIMEOUT` | Wait expired before the condition was satisfied |
| `AIXOS_ERR_BUSY` | Object is currently unavailable |
| `AIXOS_ERR_INVAL` | Invalid argument, handle, size, or state |
| `AIXOS_ERR_NOMEM` | Required memory or object slot is unavailable |
| `AIXOS_ERR_AGAIN` | Non-blocking operation would block |
| `AIXOS_ERR_CONTEXT` | API called from an unsupported context |
| `AIXOS_ERR_OVERFLOW` | Destination, counter, or object capacity would overflow |
| `AIXOS_ERR_LOCKED` | Operation is not allowed while scheduler or heap is locked |
| `AIXOS_ERR_CORRUPT` | Integrity check failed |
| `AIXOS_ERR_PERM` | Capability or privilege check failed |
| `AIXOS_ERR_FAULT` | User buffer or fault-sensitive access failed |
| `AIXOS_ERR_NOT_FOUND` | Namespace or object lookup did not find an entry |
| `AIXOS_ERR_DEADLOCK` | Operation would create a known deadlock condition |

## Context Matrix

| API family | Task context | ISR context | May block |
|---|---|---|---|
| Task create/delete/sleep/suspend | Yes | No | Sleep and current-task removal |
| Scheduler lock/unlock | Yes | No | No |
| Semaphore wait/post | Yes | No | Wait |
| Semaphore post_from_isr | No | Yes | No |
| Mutex lock/unlock | Yes | No | Lock |
| Message queue send/receive | Yes | No | Yes when timeout is non-zero |
| Message queue send_from_isr | No | Yes | No |
| Event wait/set/clear | Yes | No | Wait |
| Pipe read/write | Yes | No | Yes when timeout is non-zero |
| Pipe write_from_isr | No | Yes | No |
| Timer create/start/stop/delete | Yes | No | No |
| Timer callback | Timer service task | No | Must remain bounded |
| Heap allocate/reallocate/free | Yes | No | No, but allocation is O(N) |
| Fixed-block pool allocate/free | Yes | No | No, O(1) |
| MPU region add | Yes | No | No |
| Trace record | Yes | Yes | No |
| Crash record read/clear | Startup or diagnostic task | No | No |

## Task Creation APIs

| API | Storage ownership | MPU behavior |
|---|---|---|
| `aixos_task_create()` | Allocates TCB and stack from the kernel heap. `stack_size` must be at least `AIXOS_CFG_MIN_TASK_STACK_SIZE`. | Kernel task. |
| `aixos_user_task_create()` | Allocates TCB and stack from the kernel heap. `stack_size` must be at least `AIXOS_CFG_MIN_TASK_STACK_SIZE`. | User task; stack is registered as a read/write user MPU region. |
| `aixos_task_create_static()` | Uses caller-owned stack and TCB storage for the lifetime of the task. `stack` and `tcb` must remain valid until task deletion. | Kernel task. |
| `aixos_user_task_create_static()` | Uses caller-owned stack and TCB storage for the lifetime of the task. `stack` and `tcb` must remain valid until task deletion. | User task; stack is registered as a read/write user MPU region. |

## Buffer and Ownership Rules

- Message queue send copies at most the queue configured message size.
- Message queue receive requires an explicit destination capacity and reports
  the required size on overflow.
- Pipe operations return the transferred byte count or a negative error.
- Static task stack and TCB storage must remain valid until task deletion.
- Static message queue, pipe, and fixed-block-pool storage must remain valid
  until the owning object is no longer used.
- `aixos_mempool_free()` accepts only the exact address returned by
  `aixos_mempool_alloc()`. Double free is rejected.
- Timer callback functions must not assume ISR context and must stay bounded.
- User MPU regions must be naturally aligned power-of-two ranges. Writable
  regions must also be readable.
- Syscall handlers must transfer user buffers through `aixos_copy_from_user()`,
  `aixos_copy_to_user()`, or `aixos_zero_to_user()`. `aixos_user_memory_check()`
  is a validation primitive, not a substitute for the copy helpers.

## Production Profile

- Create dynamic objects during system initialization whenever possible.
- Use `AIXOS_CFG_HEAP_LOCK_ON_START=1` for production firmware.
- Dynamic task creation is a controlled kernel exception to public heap
  lockdown. TCB, stack, and task-slot metadata remain integrity-checked and
  accounted, while direct application `aixos_malloc()` stays disabled.
- Prefer static object creation and fixed-block pools for runtime data.
- Treat `aixos_heap_check() != AIXOS_OK`, stack guard failure, invalid crash
  CRC, and unexpected trace overwrite growth as safety-monitor events.
- Task deletion requires an application-level cancellation and resource
  ownership protocol. Arbitrary forced deletion is not a recovery mechanism.

## ABI Rules

- ABI-visible records use fixed-width integer fields.
- Crash records and trace records have compile-time size assertions in
  `include/aixos/abi.h`.
- The handle format is a 32-bit value with an 8-bit slot index and 24-bit
  generation.
- Incompatible public structure or semantic changes must increment
  `AIXOS_ABI_VERSION`.
