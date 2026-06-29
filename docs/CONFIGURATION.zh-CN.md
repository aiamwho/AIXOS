# AIXOS v1.0 配置指南

大多数产品级裁剪通过 `make config` 完成，它会写入 `config/aixos_user_cfg.h`。基础默认值和校验逻辑仍位于 `config/aixos_cfg.h`。

非交互或 CI 构建可通过 `CONFIG_CFLAGS` 覆盖宏：

```sh
make arm CONFIG_CFLAGS=-DAIXOS_CFG_SCHEDULER=AIXOS_CFG_SCHED_SIMPLE
```

使用 `make oldconfig` 打印当前菜单可配置值，不修改文件。

## 优先级和任务限制

| 宏 | 用途 |
|---|---|
| `AIXOS_CFG_MAX_PRIORITY` | 优先级级数 |
| `AIXOS_CFG_IDLE_PRIORITY` | idle 任务优先级，目前必须为 `0` |
| `AIXOS_CFG_TIMER_TASK_PRIORITY` | timer service 任务优先级 |
| `AIXOS_CFG_TASK_HANDLE_LIMIT` | 任务 handle 槽位容量 |
| `AIXOS_CFG_TASK_SLOT_PAGE_SIZE` | 动态任务槽位分配页大小 |
| `AIXOS_CFG_CAPS_PER_TASK` | 每任务 capability 表容量 |

当前 handle 格式要求 `AIXOS_CFG_TASK_HANDLE_LIMIT == 256`。

## 对象池容量

通过以下宏配置内核对象数量上限：

- `AIXOS_CFG_MAX_SEM`
- `AIXOS_CFG_MAX_MUTEX`
- `AIXOS_CFG_MAX_MQ`
- `AIXOS_CFG_MAX_EVENT`
- `AIXOS_CFG_MAX_PIPE`
- `AIXOS_CFG_MAX_TIMER`

生产构建应基于产品资源模型设置容量，并为诊断和恢复路径留余量。

## 时间和中断

| 宏 | 用途 |
|---|---|
| `AIXOS_CFG_TIME_SLICE_TICKS` | round-robin 时间片 |
| `AIXOS_CFG_SYSTICK_HZ` | 内核 tick 频率 |
| `AIXOS_CFG_CPU_CLOCK_HZ` | Cortex-M SysTick 使用的 CPU 时钟 |
| `AIXOS_CFG_KERNEL_IRQ_PRIORITY` | Cortex-M 内核临界区使用的 `BASEPRI` 阈值 |
| `AIXOS_CFG_SYSTICK_IRQ_PRIORITY` | SysTick 优先级，必须能被 kernel threshold 屏蔽 |
| `AIXOS_CFG_PENDSV_IRQ_PRIORITY` | PendSV 优先级，不能比 SysTick 更紧急 |
| `AIXOS_CFG_ISR_NESTING_MAX` | ISR nesting 诊断预算 |
| `AIXOS_CFG_ISR_NESTING_PANIC` | nesting overflow 时是否升级为 `aixos_panic()` |

默认 Cortex-M3 threshold 为 `0x40`。数值更低的高响应中断可抢占内核临界区，但不能调用 AIXOS API；它们应只做硬件确认和最小缓冲，把 RTOS 交互延后到低优先级中断或任务。

## 调度器后端

| 宏 | 值 | 用途 |
|---|---|---|
| `AIXOS_CFG_SCHEDULER` | `AIXOS_CFG_SCHED_BITMAP` | 默认 bitmap multi-queue 调度器 |
| `AIXOS_CFG_SCHEDULER` | `AIXOS_CFG_SCHED_SIMPLE` | 单个排序 ready-list 调度器 |

bitmap 后端用更多 RAM 换取固定优先级范围内的常数时间选择。simple 后端减少调度器数据和代码大小，但插入和重排与 runnable task 数量线性相关。

## 栈、堆和内存池

| 宏 | 用途 |
|---|---|
| `AIXOS_CFG_IDLE_STACK_SIZE` | idle 栈，单位 `uint32_t` |
| `AIXOS_CFG_TIMER_STACK_SIZE` | timer service 栈 |
| `AIXOS_CFG_DEFAULT_STACK_SIZE` | 默认任务栈，单位 byte |
| `AIXOS_CFG_MIN_TASK_STACK_SIZE` | 架构初始上下文最小栈 |
| `AIXOS_CFG_STACK_GUARD_BYTES` | 栈 guard 区 |
| `AIXOS_CFG_HEAP_SIZE` | 内部堆大小 |
| `AIXOS_CFG_HEAP_MAGIC` | 堆完整性 magic |
| `AIXOS_CFG_HEAP_LOCK_ON_START` | 启动后锁定运行期堆分配 |
| `AIXOS_CFG_ALIGNMENT` | 堆和 guard 对齐 |

客户固件应在最坏中断、timer 和 IPC 负载下测量栈高水位后再定栈大小。运行期确定性分配优先使用固定块池。

## MPU/PMP 保护

| 宏 | 用途 |
|---|---|
| `AIXOS_CFG_ENABLE_MPU` | 启用 per-task memory protection |
| `AIXOS_CFG_MPU_REGIONS_PER_TASK` | 每用户任务 portable region 数量 |
| `AIXOS_CFG_MPU_MIN_REGION_SIZE` | 最小 region size |
| `AIXOS_CFG_FLASH_BASE` / `AIXOS_CFG_FLASH_SIZE` | Cortex-M 可执行代码 region |
| `AIXOS_CFG_RAM_BASE` / `AIXOS_CFG_RAM_SIZE` | Cortex-M RAM 范围 |
| `AIXOS_CFG_RISCV_RAM_BASE` / `AIXOS_CFG_RISCV_RAM_SIZE` | RISC-V 可执行 RAM 范围 |

当前 portable profile 暴露 3 个 per-task region。用户任务默认用一个 region 保护栈，剩余 region 可给用户 buffer 或共享内存。

## Trace、IPC、POSIX 和可选子系统

| 宏 | 用途 |
|---|---|
| `AIXOS_CFG_TRACE_ENABLE` / `AIXOS_CFG_TRACE_BUFFER_SIZE` | trace 使能和 ring 大小 |
| `AIXOS_CFG_CPU_USAGE_ENABLE` | CPU usage 统计 |
| `AIXOS_CFG_CRASH_MAGIC` | crash record magic |
| `AIXOS_CFG_MAX_IPC_COPY_BYTES` | 普通 IPC 最大 copy 大小 |
| `AIXOS_CFG_ISR_COPY_MAX_BYTES` | ISR API 最大 copy 大小 |
| `AIXOS_CFG_MQ_PRIORITY_MAX` | 消息队列最大消息优先级 |
| `AIXOS_CFG_POSIX_KEYS` / `AIXOS_CFG_POSIX_TIMERS` 等 | POSIX 子集容量 |
| `AIXOS_CFG_ENABLE_SIGNALS` | 任务 signal API |
| `AIXOS_CFG_ENABLE_NAMESPACE` | resource namespace API |
| `AIXOS_CFG_ENABLE_TIME_WHEEL` | 层级 timeout wheel |
| `AIXOS_CFG_ENABLE_PANIC_RESET` | 受控 panic reset |

只有在编译并测试产品配置后，才禁用未使用的子系统。

## Minimal Profile

编译时设置 `AIXOS_CFG_PROFILE_MINIMAL=1` 可降低多个对象和诊断容量。启用后必须重新验证所有产品 workload，因为容量限制会变化。
