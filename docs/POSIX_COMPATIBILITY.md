# AIXOS v1.0 POSIX Compatibility Matrix

AIXOS provides a bounded POSIX compatibility subset for embedded firmware
portability. This is not a full POSIX process, file-system, socket, or shell
environment.

Status values:

- `Supported`: implemented in the AIXOS compatibility layer and covered by
  compile or host tests.
- `Partial`: available with documented embedded limitations.
- `Not supported`: intentionally absent in v1.0.

## Headers

| Header | Status | Notes |
|---|---|---|
| `pthread.h` | Partial | Thread, mutex, condition variable, rwlock, barrier, key, once subset. |
| `semaphore.h` | Supported | POSIX semaphore wrappers over AIXOS semaphores. |
| `mqueue.h` | Partial | Queue open/send/receive/getattr/setattr subset; no filesystem namespace. |
| `time.h` | Partial | Clock, nanosleep, and POSIX timer subset. |
| `unistd.h` | Partial | Pipe-backed descriptor subset used by tests. |
| `errno.h` | Supported | AIXOS errno constants and task-local errno location. |
| `sched.h` | Partial | Priority query and yield subset. |
| `fcntl.h` | Partial | Constants required by supported queue/file descriptor subset. |

## Threading

| POSIX area | AIXOS APIs | Status | Limitations |
|---|---|---|---|
| Thread create | `aixos_pthread_create` | Partial | Maps to AIXOS tasks; no process address-space model. |
| Join/detach | `aixos_pthread_join`, `aixos_pthread_detach` | Partial | Embedded task lifecycle semantics; forced deletion remains unsafe. |
| Thread self/equal | `aixos_pthread_self`, `aixos_pthread_equal` | Supported | Handle-based identity. |
| Thread exit | `aixos_pthread_exit` | Partial | No process-level cleanup semantics. |
| Scheduling priority | `aixos_pthread_getschedprio`, `aixos_pthread_setschedprio` | Partial | Fixed-priority RTOS model; no full POSIX scheduler policy set. |
| `pthread_once` | `aixos_pthread_once` | Supported | In-process firmware scope only. |
| Thread-specific data | `aixos_pthread_key_*`, `aixos_pthread_getspecific`, `aixos_pthread_setspecific` | Partial | Capacity limited by `AIXOS_CFG_POSIX_KEYS`; destructor behavior is constrained. |

## Synchronization

| POSIX area | AIXOS APIs | Status | Limitations |
|---|---|---|---|
| Mutex attributes | `aixos_pthread_mutexattr_*` | Partial | Normal, recursive, errorcheck, and priority protocol metadata subset. |
| Mutex operations | `aixos_pthread_mutex_*` | Partial | Backed by AIXOS mutexes; no process-shared objects. |
| Condition variables | `aixos_pthread_cond_*` | Partial | Backed by kernel primitives; timed waits use AIXOS clock conversion. |
| Read-write locks | `aixos_pthread_rwlock_*` | Partial | Reader ownership table limited by `AIXOS_CFG_POSIX_RWLOCK_READERS`. |
| Barriers | `aixos_pthread_barrier_*` | Supported | Firmware-local only. |
| Semaphores | `aixos_sem_posix_*` | Supported | Backed by AIXOS semaphores; no named semaphore filesystem namespace. |

## Time

| POSIX area | AIXOS APIs | Status | Limitations |
|---|---|---|---|
| Clock read | `aixos_clock_gettime` | Partial | Supported clock IDs are `AIXOS_CLOCK_REALTIME` and `AIXOS_CLOCK_MONOTONIC`. |
| Clock resolution | `aixos_clock_getres` | Partial | Resolution follows `AIXOS_CFG_SYSTICK_HZ`. |
| Sleep | `aixos_nanosleep`, `aixos_clock_nanosleep` | Partial | Tick-granularity; sub-tick sleeps round to scheduler ticks. |
| POSIX timers | `aixos_timer_posix_*` | Partial | Pool limited by `AIXOS_CFG_POSIX_TIMERS`; callbacks follow AIXOS timer service behavior. |

## Message Queues

| POSIX area | AIXOS APIs | Status | Limitations |
|---|---|---|---|
| Open/close | `aixos_mq_posix_open`, `aixos_mq_posix_close` | Partial | Creates handle-backed queues; no path namespace or permissions model. |
| Send/receive | `aixos_mq_posix_send`, `aixos_mq_posix_receive` | Supported | Message size and count bounded by queue configuration. |
| Timed send/receive | `aixos_mq_posix_timedsend`, `aixos_mq_posix_timedreceive` | Partial | Absolute timeout converted to AIXOS ticks. |
| Attributes | `aixos_mq_posix_getattr`, `aixos_mq_posix_setattr` | Partial | Only supported attributes are honored. |
| Notify | `aixos_mq_posix_notify` | Partial | Embedded notification semantics; no process signal delivery model. |

## File Descriptors and I/O

| POSIX area | Status | Limitations |
|---|---|---|
| `pipe`/`read`/`write` style I/O | Partial | Pipe-backed descriptor subset; no VFS. |
| Files and directories | Not supported | No filesystem, path lookup, `open`, `close`, `stat`, or directory APIs in v1.0. |
| Sockets | Not supported | No network stack in v1.0. |
| `select`/`poll` | Not supported | Use native AIXOS IPC waits or events. |
| Processes | Not supported | No `fork`, `exec`, `waitpid`, process groups, or shell environment. |

## Verification

Current POSIX compatibility checks:

```sh
make posix-api-check RISCV_PREFIX=riscv64-elf-
make test
```

The compatibility layer is suitable for embedded source portability, not for
running general POSIX applications unmodified.

