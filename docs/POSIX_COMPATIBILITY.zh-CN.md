# AIXOS v1.0 POSIX 兼容矩阵

AIXOS 提供有界 POSIX 兼容子集，用于嵌入式固件源码可移植性。它不是完整 POSIX 进程、文件系统、socket 或 shell 环境。

状态含义：

- `Supported`：已在 AIXOS 兼容层实现，并由编译或 host tests 覆盖。
- `Partial`：可用，但存在文档化的嵌入式限制。
- `Not supported`：v1.0 有意不提供。

## 头文件

| Header | 状态 | 说明 |
|---|---|---|
| `pthread.h` | Partial | 线程、mutex、condition variable、rwlock、barrier、key、once 子集 |
| `semaphore.h` | Supported | POSIX semaphore wrapper，底层为 AIXOS semaphore |
| `mqueue.h` | Partial | open/send/receive/getattr/setattr 子集，无文件系统 namespace |
| `time.h` | Partial | clock、nanosleep 和 POSIX timer 子集 |
| `unistd.h` | Partial | 测试使用的 pipe-backed descriptor 子集 |
| `errno.h` | Supported | AIXOS errno 常量和 task-local errno 位置 |
| `sched.h` | Partial | priority query 和 yield 子集 |
| `fcntl.h` | Partial | 支持 queue/descriptor 子集所需常量 |

## 线程和同步

| POSIX 领域 | AIXOS API | 状态 | 限制 |
|---|---|---|---|
| Thread create | `aixos_pthread_create` | Partial | 映射到 AIXOS task，无进程地址空间模型 |
| Join/detach | `aixos_pthread_join`, `aixos_pthread_detach` | Partial | 嵌入式任务生命周期语义，强制删除仍不安全 |
| self/equal | `aixos_pthread_self`, `aixos_pthread_equal` | Supported | handle-based identity |
| Thread exit | `aixos_pthread_exit` | Partial | 无进程级 cleanup 语义 |
| Scheduling priority | `aixos_pthread_getschedprio`, `aixos_pthread_setschedprio` | Partial | 固定优先级 RTOS 模型 |
| `pthread_once` | `aixos_pthread_once` | Supported | 固件进程内范围 |
| Thread-specific data | `aixos_pthread_key_*`, `aixos_pthread_getspecific`, `aixos_pthread_setspecific` | Partial | 受 `AIXOS_CFG_POSIX_KEYS` 容量限制 |
| Mutex | `aixos_pthread_mutex_*` | Partial | 底层为 AIXOS mutex，无 process-shared 对象 |
| Condition variable | `aixos_pthread_cond_*` | Partial | 基于内核原语，timed wait 使用 AIXOS 时钟转换 |
| Read-write lock | `aixos_pthread_rwlock_*` | Partial | reader ownership table 受 `AIXOS_CFG_POSIX_RWLOCK_READERS` 限制 |
| Barrier | `aixos_pthread_barrier_*` | Supported | 固件本地 |
| Semaphore | `aixos_sem_posix_*` | Supported | 无 named semaphore 文件系统 namespace |

## 时间和消息队列

| POSIX 领域 | AIXOS API | 状态 | 限制 |
|---|---|---|---|
| Clock read | `aixos_clock_gettime` | Partial | 支持 `AIXOS_CLOCK_REALTIME` 和 `AIXOS_CLOCK_MONOTONIC` |
| Clock resolution | `aixos_clock_getres` | Partial | 分辨率跟随 `AIXOS_CFG_SYSTICK_HZ` |
| Sleep | `aixos_nanosleep`, `aixos_clock_nanosleep` | Partial | tick 粒度，sub-tick 向 tick 取整 |
| POSIX timers | `aixos_timer_posix_*` | Partial | 池大小受 `AIXOS_CFG_POSIX_TIMERS` 限制 |
| Queue open/close | `aixos_mq_posix_open`, `aixos_mq_posix_close` | Partial | 创建 handle-backed queue，无路径 namespace 或权限模型 |
| Queue send/receive | `aixos_mq_posix_send`, `aixos_mq_posix_receive` | Supported | 消息大小和数量受队列配置限制 |
| Timed send/receive | `aixos_mq_posix_timedsend`, `aixos_mq_posix_timedreceive` | Partial | 绝对 timeout 转成 AIXOS tick |
| Attributes | `aixos_mq_posix_getattr`, `aixos_mq_posix_setattr` | Partial | 只处理支持的属性 |
| Notify | `aixos_mq_posix_notify` | Partial | 嵌入式 notification 语义，无进程 signal delivery 模型 |

## 文件描述符和 I/O

| POSIX 领域 | 状态 | 限制 |
|---|---|---|
| `pipe`/`read`/`write` 风格 I/O | Partial | pipe-backed descriptor 子集，无 VFS |
| 文件和目录 | Not supported | v1.0 无 filesystem、path lookup、`open`、`close`、`stat` 或目录 API |
| Sockets | Not supported | v1.0 无网络栈 |
| `select`/`poll` | Not supported | 使用原生 AIXOS IPC wait 或 event |
| Processes | Not supported | 无 `fork`、`exec`、`waitpid`、process group 或 shell 环境 |

## 验证

当前 POSIX 兼容检查：

```sh
make posix-api-check RISCV_PREFIX=riscv64-elf-
make test
```

该兼容层适合嵌入式源码可移植，不用于未经修改地运行通用 POSIX 应用。
