# AIXOS v1.0 API Reference

This reference summarizes the customer-facing APIs declared under
`include/aixos/`. The normative compatibility and ownership rules are in
`API_CONTRACT.md`.

## Common Rules

Return convention:

- Public APIs return `AIXOS_OK` or a negative `AIXOS_ERR_*` value.
- APIs that create kernel objects return an `aixos_handle_t`.
- `AIXOS_HANDLE_INVALID` means creation failed.
- `timeout_ms == 0` is non-blocking.
- `timeout_ms == UINT32_MAX` waits indefinitely.
- Blocking APIs are task-context only.

Include the complete public API with:

```c
#include "aixos/aixos.h"
```

## Kernel Initialization

| API | Use |
|---|---|
| `aixos_object_init()` | Initialize object pools and handle state |
| `aixos_sched_init()` | Initialize scheduler state |
| `aixos_heap_init(start, size)` | Initialize the kernel heap |
| `aixos_task_init()` | Initialize task subsystem and system tasks |
| `aixos_timer_init()` | Initialize software timer subsystem |
| `aixos_start()` | Start scheduling and does not return |

Call these once during startup before application tasks depend on kernel
services.

## Task APIs

| API | Use |
|---|---|
| `aixos_task_create(name, entry, arg, stack_size, priority)` | Create a kernel task with heap-owned TCB and stack |
| `aixos_task_create_static(name, entry, arg, stack, stack_size, priority, tcb)` | Create a kernel task with caller-owned TCB and stack |
| `aixos_user_task_create(name, entry, arg, stack_size, priority)` | Create a user task with heap-owned storage |
| `aixos_user_task_create_static(...)` | Create a user task with caller-owned storage |
| `aixos_task_delete(task)` | Delete a task after application-level resource handoff |
| `aixos_task_sleep(ms)` | Delay the current task |
| `aixos_task_yield()` | Yield the current time slice |
| `aixos_task_self()` | Return the current task handle |
| `aixos_task_suspend(task)` | Suspend a task |
| `aixos_task_resume(task)` | Resume a suspended task |
| `aixos_task_set_priority(task, priority)` | Change base priority |
| `aixos_task_get_info(task, info)` | Read task diagnostic information |
| `aixos_task_stack_check(task)` | Check stack guard integrity |
| `aixos_task_is_user(task)` | Return whether the task is a user task |

Priority `0` is reserved for idle. Product tasks should use priorities below
`AIXOS_CFG_MAX_PRIORITY` and avoid starving the timer service task.

Static task storage example:

```c
static aixos_tcb_t tcb;
static uint8_t stack[512] __attribute__((aligned(8)));

aixos_handle_t task = aixos_task_create_static("worker", worker, NULL,
                                               stack, sizeof(stack),
                                               10, &tcb);
```

## Scheduler APIs

| API | Use |
|---|---|
| `aixos_sched_lock()` | Prevent task scheduling in short critical sections |
| `aixos_sched_unlock()` | Release one scheduler lock nesting level |
| `aixos_sched_lock_count()` | Return nesting count |
| `aixos_sched_is_locked()` | Return whether scheduling is locked |

Do not call blocking APIs while the scheduler is locked.

## ISR Diagnostics

| API | Use |
|---|---|
| `aixos_in_isr()` | Return whether the kernel is currently inside ISR context |
| `aixos_isr_nesting_level()` | Return current ISR nesting level |
| `aixos_isr_nesting_high_watermark()` | Return highest observed ISR nesting level |
| `aixos_isr_nesting_overflow_count()` | Return nesting limit overflow count |
| `aixos_isr_stats_reset()` | Reset ISR high-water and overflow counters when not inside ISR |

`aixos_isr_enter()` and `aixos_isr_exit()` are architecture/IRQ wrapper hooks.
Board integration wrappers should call them for nesting accounting. Application
ISR code must not treat them as permission to call RTOS service APIs from
priorities above the kernel `BASEPRI` threshold.

## Semaphore APIs

| API | Use |
|---|---|
| `aixos_sem_create(initial_count)` | Create a counting semaphore |
| `aixos_sem_wait(sem, timeout_ms)` | Take or wait for a semaphore |
| `aixos_sem_post(sem)` | Release a semaphore from task context |
| `aixos_sem_post_from_isr(sem)` | Release a semaphore from ISR context |
| `aixos_sem_get_count(sem)` | Read current count |
| `aixos_sem_delete(sem)` | Delete semaphore when no task owns the protocol |

Use `timeout_ms == 0` for non-blocking polling.

## Mutex APIs

| API | Use |
|---|---|
| `aixos_mutex_create()` | Create a mutex |
| `aixos_mutex_lock(mutex, timeout_ms)` | Lock with optional timeout |
| `aixos_mutex_unlock(mutex)` | Unlock from owning task |
| `aixos_mutex_delete(mutex)` | Delete when no task owns or waits for it |

Mutexes include priority inheritance. Use them for short critical sections and
avoid blocking while holding multiple locks unless the lock order is fixed.

## Message Queue APIs

| API | Use |
|---|---|
| `aixos_mq_create(max_msgs, msg_size)` | Create heap-backed queue storage |
| `aixos_mq_create_static(max_msgs, msg_size, buffer, lengths)` | Use caller-owned queue storage |
| `aixos_mq_send(mq, msg, size, timeout_ms)` | Send a message |
| `aixos_mq_send_priority(mq, msg, size, priority, timeout_ms)` | Send with message priority |
| `aixos_mq_send_from_isr(mq, msg, size)` | Send from ISR with ISR copy limit |
| `aixos_mq_recv(mq, buf, capacity, size, timeout_ms)` | Receive into bounded buffer |
| `aixos_mq_recv_priority(mq, buf, capacity, size, priority, timeout_ms)` | Receive message and priority |
| `aixos_mq_get_info(mq, info)` | Read queue capacity and fill level |
| `aixos_mq_delete(mq)` | Delete the queue |

The queue copies messages. `capacity` must be large enough on receive; an
undersized receive does not consume the message.

## Event APIs

| API | Use |
|---|---|
| `aixos_event_create()` | Create an event flag group |
| `aixos_event_wait(ev, mask, mode, timeout_ms, matched)` | Wait for AND/OR flags |
| `aixos_event_set(ev, flags)` | Set event flags |
| `aixos_event_clear(ev, flags)` | Clear event flags |
| `aixos_event_delete(ev)` | Delete event group |

`mode` combines `AIXOS_EVENT_AND`, `AIXOS_EVENT_OR`, and optional
`AIXOS_EVENT_CLEAR`.

## Pipe APIs

| API | Use |
|---|---|
| `aixos_pipe_create(size)` | Create heap-backed byte pipe |
| `aixos_pipe_create_static(buffer, size)` | Use caller-owned byte storage |
| `aixos_pipe_write(pipe, data, len, timeout_ms)` | Write bytes, returns transferred count or error |
| `aixos_pipe_read(pipe, buf, len, timeout_ms)` | Read bytes, returns transferred count or error |
| `aixos_pipe_write_from_isr(pipe, data, len)` | Write bytes from ISR within ISR copy limit |
| `aixos_pipe_delete(pipe)` | Delete pipe |

Pipes are suitable for byte streams. Message queues are better for framed
messages.

## Timer APIs

| API | Use |
|---|---|
| `aixos_timer_create(name, type, callback, arg)` | Create one-shot or periodic software timer |
| `aixos_timer_start(timer, interval_ms)` | Start or restart timer |
| `aixos_timer_stop(timer)` | Stop active timer |
| `aixos_timer_delete(timer)` | Delete timer |

Timer callbacks run in timer service context, not ISR context. Keep callbacks
bounded and hand off long work to tasks.

## Heap APIs

| API | Use |
|---|---|
| `aixos_malloc(size)` | Allocate aligned heap block |
| `aixos_calloc(count, size)` | Allocate and zero block |
| `aixos_realloc(ptr, new_size)` | Resize block |
| `aixos_free(ptr)` | Free block |
| `aixos_mem_info(info)` | Read heap statistics |
| `aixos_heap_check()` | Validate heap metadata |
| `aixos_heap_lockdown()` | Disable direct runtime heap allocation |
| `aixos_heap_is_locked()` | Return heap lock state |

Production firmware should allocate during initialization and use static
objects or fixed-block pools at runtime.

## Fixed-Block Pool APIs

| API | Use |
|---|---|
| `aixos_mempool_init(pool, storage, storage_size, block_size, block_count)` | Initialize caller-owned fixed-block pool |
| `aixos_mempool_alloc(pool)` | Allocate one block |
| `aixos_mempool_free(pool, block)` | Free exact block returned by alloc |
| `aixos_mempool_owns(pool, block)` | Check whether a pointer belongs to pool |

Use fixed-block pools for deterministic runtime allocation.

## MPU APIs

| API | Use |
|---|---|
| `aixos_task_mpu_region_add(task, base, size, attr)` | Grant a user task access to a memory region |
| `aixos_mpu_region_valid(base, size, attr)` | Validate portable MPU/PMP region constraints |
| `aixos_user_memory_check(address, size, write_access)` | Check current user access before kernel copy |
| `aixos_copy_from_user(kernel_dst, user_src, size)` | Copy from the current user task after MPU/PMP read validation |
| `aixos_copy_to_user(user_dst, kernel_src, size)` | Copy to the current user task after MPU/PMP write validation |
| `aixos_zero_to_user(user_dst, size)` | Clear a current-user buffer after MPU/PMP write validation |

Attributes are `AIXOS_MPU_READ`, `AIXOS_MPU_WRITE`, `AIXOS_MPU_EXEC`, and
`AIXOS_MPU_DEVICE`.

Kernel syscall handlers must use the user copy helpers for user pointers.
`aixos_user_memory_check()` is for range validation only; it does not perform
the transfer.

## Notification APIs

| API | Use |
|---|---|
| `aixos_task_notify(task, value, action)` | Signal a task from task context |
| `aixos_task_notify_from_isr(task, value, action)` | Signal a task from ISR context |
| `aixos_task_notify_wait(clear_on_entry, clear_on_exit, value, timeout_ms)` | Wait for notification bits/value |
| `aixos_task_notify_take(clear_count, timeout_ms, value)` | Counting-style notification wait |

Notification actions are set bits, increment, overwrite, and no-overwrite.

## Trace and System Information APIs

| API | Use |
|---|---|
| `aixos_trace_init()` | Initialize trace ring |
| `aixos_trace_record(event, d0, d1)` | Record one trace event |
| `AIXOS_TRACE(event, d0, d1)` | Compile-time controlled trace macro |
| `aixos_trace_snapshot(entries, capacity, info)` | Copy trace entries |
| `aixos_trace_get_info(info)` | Read trace ring status |
| `aixos_cpu_usage_get()` | Return CPU usage scaled by 100 |
| `aixos_get_tick()` | Return current kernel tick |
| `aixos_sys_info(info)` | Read scheduler and memory summary |

## Crash APIs

| API | Use |
|---|---|
| `aixos_crash_record_store(...)` | Store a basic crash record |
| `aixos_crash_record_store_extended(...)` | Store crash record with fault details |
| `aixos_crash_record_get()` | Return current crash record pointer |
| `aixos_crash_record_validate(record)` | Validate magic, size, version, and CRC |
| `aixos_crash_record_clear()` | Clear stored crash record |

Export valid crash records before clearing them.

## Microkernel and Namespace APIs

| API | Use |
|---|---|
| `aixos_namespace_init()` | Initialize namespace registry |
| `aixos_namespace_register(name, obj, type, rights)` | Register object under a name |
| `aixos_namespace_unregister(name)` | Remove namespace entry |
| `aixos_namespace_resolve(name, obj, type, rights)` | Resolve object metadata |
| `aixos_namespace_open(name, required_rights, cap)` | Open capability to object |
| `aixos_user_*` wrappers | User-mode syscall-facing APIs |
| `aixos_channel_create`, `aixos_connect`, `aixos_msg_send`, `aixos_msg_receive`, `aixos_msg_reply` | Synchronous message IPC |

Use capabilities to avoid exposing raw kernel handles to user tasks.

## POSIX Compatibility

POSIX headers are under `posix/include/`. The compatibility layer supports the
subset exercised by `tests/posix_api_compile.c` and host regression tests.
Include POSIX headers from `posix/include` before relying on a POSIX API in
customer code, and run:

```sh
make posix-api-check RISCV_PREFIX=riscv64-elf-
```
