# AIXOS v1.0 API 参考

本文汇总 `include/aixos/` 下声明的客户可见 API。规范性的兼容性、所有权和上下文规则见 `API_CONTRACT.zh-CN.md`。

## 通用规则

- 公共 API 返回 `AIXOS_OK` 或负的 `AIXOS_ERR_*`。
- 创建内核对象的 API 返回 `aixos_handle_t`。
- `AIXOS_HANDLE_INVALID` 表示创建失败。
- `timeout_ms == 0` 为非阻塞。
- `timeout_ms == UINT32_MAX` 为无限等待。
- 阻塞 API 只能在任务上下文调用。

包含完整公共 API：

```c
#include "aixos/aixos.h"
```

## 内核初始化

| API | 用途 |
|---|---|
| `aixos_object_init()` | 初始化对象池和 handle 状态 |
| `aixos_heap_init(start, size)` | 初始化内核堆 |
| `aixos_task_init()` | 初始化任务子系统和系统任务 |
| `aixos_trace_init()` | 初始化 trace ring |
| `aixos_timer_init()` | 初始化软件定时器子系统 |
| `aixos_namespace_init()` | 初始化 namespace registry，启用 namespace 时调用 |
| `aixos_timing_wheel_init()` | 初始化层级 timeout wheel，启用 time wheel 时调用 |
| `aixos_sched_init()` | 初始化调度器状态 |
| `aixos_start()` | 启动调度器，不返回 |

这些函数应在启动阶段调用一次，并在应用任务依赖内核服务前完成。

## 任务 API

| API | 用途 |
|---|---|
| `aixos_task_create(name, entry, arg, stack_size, priority)` | 用堆分配的 TCB 和栈创建内核任务 |
| `aixos_task_create_static(name, entry, arg, stack, stack_size, priority, tcb)` | 用调用者提供的 TCB 和栈创建内核任务 |
| `aixos_user_task_create(name, entry, arg, stack_size, priority)` | 用堆存储创建用户任务 |
| `aixos_user_task_create_static(...)` | 用调用者存储创建用户任务 |
| `aixos_task_delete(task)` | 在应用完成资源交接后删除任务 |
| `aixos_task_sleep(ms)` | 延迟当前任务 |
| `aixos_task_yield()` | 让出当前时间片 |
| `aixos_task_self()` | 返回当前任务 handle |
| `aixos_task_suspend(task)` | 挂起任务 |
| `aixos_task_resume(task)` | 恢复挂起任务 |
| `aixos_task_set_priority(task, priority)` | 修改基础优先级 |
| `aixos_task_get_info(task, info)` | 读取任务诊断信息 |
| `aixos_task_stack_check(task)` | 检查栈 guard 完整性 |
| `aixos_task_is_user(task)` | 判断任务是否为用户任务 |

优先级 `0` 保留给 idle。产品任务优先级应小于 `AIXOS_CFG_MAX_PRIORITY`，并避免饿死 timer service task。

## 调度器 API

| API | 用途 |
|---|---|
| `aixos_sched_lock()` | 短临界区内阻止任务调度 |
| `aixos_sched_unlock()` | 释放一层调度器锁 |
| `aixos_sched_lock_count()` | 返回嵌套计数 |
| `aixos_sched_is_locked()` | 返回调度器是否锁定 |

调度器锁定期间不要调用阻塞 API。

## ISR 诊断 API

| API | 用途 |
|---|---|
| `aixos_in_isr()` | 返回当前是否处在 ISR 上下文 |
| `aixos_isr_nesting_level()` | 当前 ISR 嵌套层级 |
| `aixos_isr_nesting_high_watermark()` | 观察到的最高嵌套层级 |
| `aixos_isr_nesting_overflow_count()` | 嵌套预算溢出次数 |
| `aixos_isr_stats_reset()` | 非 ISR 上下文重置 ISR 统计 |

`aixos_isr_enter()` 和 `aixos_isr_exit()` 是架构/IRQ wrapper hook，用于 nesting 统计。应用 ISR 不能把它们当作高优先级中断可调用 RTOS 服务的许可。

## IPC API

### 信号量

| API | 用途 |
|---|---|
| `aixos_sem_create(initial_count)` | 创建计数信号量 |
| `aixos_sem_wait(sem, timeout_ms)` | 获取或等待信号量 |
| `aixos_sem_post(sem)` | 任务上下文释放信号量 |
| `aixos_sem_post_from_isr(sem)` | ISR 上下文释放信号量 |
| `aixos_sem_get_count(sem)` | 读取当前计数 |
| `aixos_sem_delete(sem)` | 删除信号量 |

### 互斥量

| API | 用途 |
|---|---|
| `aixos_mutex_create()` | 创建互斥量 |
| `aixos_mutex_lock(mutex, timeout_ms)` | 加锁，可带 timeout |
| `aixos_mutex_unlock(mutex)` | owner 任务解锁 |
| `aixos_mutex_delete(mutex)` | 无 owner 和 waiter 时删除 |

互斥量包含优先级继承。尽量用于短临界区，多个锁必须固定顺序。

### 消息队列

| API | 用途 |
|---|---|
| `aixos_mq_create(max_msgs, msg_size)` | 创建堆存储队列 |
| `aixos_mq_create_static(max_msgs, msg_size, buffer, lengths)` | 使用调用者队列存储 |
| `aixos_mq_send(mq, msg, size, timeout_ms)` | 发送消息 |
| `aixos_mq_send_priority(mq, msg, size, priority, timeout_ms)` | 按消息优先级发送 |
| `aixos_mq_send_from_isr(mq, msg, size)` | ISR 内发送，受 ISR copy limit 限制 |
| `aixos_mq_recv(mq, buf, capacity, size, timeout_ms)` | 接收到有界 buffer |
| `aixos_mq_recv_priority(...)` | 接收消息和优先级 |
| `aixos_mq_get_info(mq, info)` | 读取容量和占用 |
| `aixos_mq_delete(mq)` | 删除队列 |

队列复制消息。接收容量不足时不会消费消息。

### 事件、管道、通知

| API | 用途 |
|---|---|
| `aixos_event_create()` / `aixos_event_wait()` / `aixos_event_set()` / `aixos_event_clear()` / `aixos_event_delete()` | 事件标志组，支持 AND/OR 和 clear-on-consume |
| `aixos_pipe_create(size)` / `aixos_pipe_create_static(buffer, size)` | 创建字节流管道 |
| `aixos_pipe_write()` / `aixos_pipe_read()` | 写/读字节，返回传输字节数或负错误 |
| `aixos_pipe_write_from_isr()` | ISR 内写入受限字节数 |
| `aixos_task_notify()` / `aixos_task_notify_from_isr()` | 向任务发通知 |
| `aixos_task_notify_wait()` / `aixos_task_notify_take()` | 等待通知 bit/value 或计数式通知 |

管道适合字节流；有边界消息优先使用消息队列。

## 定时器 API

| API | 用途 |
|---|---|
| `aixos_timer_create(name, type, callback, arg)` | 创建 one-shot 或 periodic 软件定时器 |
| `aixos_timer_start(timer, interval_ms)` | 启动或重启定时器 |
| `aixos_timer_stop(timer)` | 停止活动定时器 |
| `aixos_timer_delete(timer)` | 删除定时器 |

定时器回调在 timer service 任务上下文执行，不在 ISR 中执行。回调应保持有界，长工作交给任务。

## 内存 API

| API | 用途 |
|---|---|
| `aixos_malloc(size)` / `aixos_calloc(count, size)` / `aixos_realloc(ptr, new_size)` / `aixos_free(ptr)` | 堆分配、清零分配、调整和释放 |
| `aixos_mem_info(info)` | 读取堆统计 |
| `aixos_heap_check()` | 校验堆元数据 |
| `aixos_heap_lockdown()` / `aixos_heap_is_locked()` | 禁用并查询运行期直接堆分配 |
| `aixos_mempool_init()` / `aixos_mempool_alloc()` / `aixos_mempool_free()` / `aixos_mempool_owns()` | 固定块池初始化、分配、释放和归属检查 |

生产固件应在初始化期分配，运行期优先静态对象或固定块池。

## MPU 和用户内存 API

| API | 用途 |
|---|---|
| `aixos_task_mpu_region_add(task, base, size, attr)` | 给用户任务授权内存 region |
| `aixos_mpu_region_valid(base, size, attr)` | 校验 portable MPU/PMP region 约束 |
| `aixos_user_memory_check(address, size, write_access)` | 校验当前用户访问范围 |
| `aixos_copy_from_user(kernel_dst, user_src, size)` | 校验后从当前用户任务复制输入 |
| `aixos_copy_to_user(user_dst, kernel_src, size)` | 校验后复制到当前用户任务 |
| `aixos_zero_to_user(user_dst, size)` | 校验后清零用户 buffer |

属性包括 `AIXOS_MPU_READ`、`AIXOS_MPU_WRITE`、`AIXOS_MPU_EXEC`、`AIXOS_MPU_DEVICE`。Syscall handler 必须使用 user-copy helper，不能直接解引用用户指针。

## Trace、Crash、Microkernel 和 POSIX

| API/模块 | 用途 |
|---|---|
| `aixos_trace_record()`、`AIXOS_TRACE()`、`aixos_trace_snapshot()`、`aixos_trace_get_info()` | trace ring 记录、快照和状态 |
| `aixos_cpu_usage_get()`、`aixos_get_tick()`、`aixos_sys_info()` | CPU、tick 和系统摘要 |
| `aixos_crash_record_store*()`、`aixos_crash_record_get()`、`aixos_crash_record_validate()`、`aixos_crash_record_clear()` | crash record 保存、读取、校验和清除 |
| `aixos_namespace_*` | namespace 注册、解析、打开 capability |
| `aixos_user_*` wrappers | 用户态 syscall-facing API |
| `aixos_channel_create`、`aixos_connect`、`aixos_msg_send`、`aixos_msg_receive`、`aixos_msg_reply` | 同步消息 IPC |

POSIX 头文件位于 `posix/include/`。使用 POSIX 子集前运行：

```sh
make posix-api-check RISCV_PREFIX=riscv64-elf-
```
