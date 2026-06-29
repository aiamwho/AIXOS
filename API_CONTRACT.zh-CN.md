# AIXOS v1.0 公共 API 合同

本文定义 `AIXOS_API_VERSION == 0x00010000` 与 `AIXOS_ABI_VERSION == 0x00010000` 对应的公共 API/ABI 合同。适用范围包括 `include/aixos/` 下的头文件，以及 `posix/include/` 下的 POSIX 兼容头文件。

## 版本策略

- `AIXOS_VERSION_MAJOR`、`AIXOS_VERSION_MINOR`、`AIXOS_VERSION_PATCH` 标识源码包版本。
- 公共 API 语义或声明变化时，必须更新 `AIXOS_API_VERSION`。
- ABI 可见结构布局、handle 编码、二进制记录格式或调用约定变化时，必须更新 `AIXOS_ABI_VERSION`。
- 每个已验证固件镜像都应归档版本头、map、ELF、编译器信息、配置和源码 manifest。

## 通用 API 规则

- `timeout_ms == 0` 表示非阻塞。
- `timeout_ms == UINT32_MAX` 表示无限等待。
- 其他 timeout 会向上折算为 tick，并限制在 32 位 tick 半范围内，保证 wrap-safe 比较有效。
- 返回 `AIXOS_ERR_CONTEXT` 表示当前执行上下文禁止调用该 API。
- 返回 `AIXOS_ERR_LOCKED` 表示调度器锁定时该操作可能阻塞或移除当前任务，因此被拒绝。
- Handle 带 generation 检查；已删除、过期或类型不匹配的 handle 无效。
- 除 static-create API 明确保留用户传入存储外，用户 buffer 仍由调用者拥有。
- 除非头文件明确说明，API 不接受重叠的源/目的 buffer。
- 普通任务 API 内部会和中断串行化，但调用者仍负责传入 buffer 的生命周期和所有权。

## 错误码

公共 API 成功返回 `AIXOS_OK`，失败返回负的 `AIXOS_ERR_*`。常见错误码定义在 `include/aixos/types.h`。

| 错误 | 含义 |
|---|---|
| `AIXOS_ERR_TIMEOUT` | 条件满足前等待超时 |
| `AIXOS_ERR_BUSY` | 对象当前不可用 |
| `AIXOS_ERR_INVAL` | 参数、handle、大小或状态无效 |
| `AIXOS_ERR_NOMEM` | 内存或对象槽位不足 |
| `AIXOS_ERR_AGAIN` | 非阻塞操作本来会阻塞 |
| `AIXOS_ERR_CONTEXT` | 当前上下文不支持该 API |
| `AIXOS_ERR_OVERFLOW` | 目的区、计数器或容量会溢出 |
| `AIXOS_ERR_LOCKED` | 调度器或堆锁定时不允许该操作 |
| `AIXOS_ERR_CORRUPT` | 完整性检查失败 |
| `AIXOS_ERR_PERM` | capability 或权限检查失败 |
| `AIXOS_ERR_FAULT` | 用户 buffer 或 fault-sensitive 访问失败 |
| `AIXOS_ERR_NOT_FOUND` | namespace 或对象查找失败 |
| `AIXOS_ERR_DEADLOCK` | 操作会造成已知死锁 |

## 上下文矩阵

| API 类别 | 任务上下文 | ISR 上下文 | 是否可能阻塞 |
|---|---|---|---|
| 任务创建/删除/睡眠/挂起 | 是 | 否 | sleep 和移除当前任务会阻塞 |
| 调度器锁/解锁 | 是 | 否 | 否 |
| 信号量 wait/post | 是 | 否 | wait 可能阻塞 |
| `sem_post_from_isr` | 否 | 是 | 否 |
| 互斥量 lock/unlock | 是 | 否 | lock 可能阻塞 |
| 消息队列 send/receive | 是 | 否 | 非零 timeout 可能阻塞 |
| `mq_send_from_isr` | 否 | 是 | 否 |
| 事件 wait/set/clear | 是 | 否 | wait 可能阻塞 |
| 管道 read/write | 是 | 否 | 非零 timeout 可能阻塞 |
| `pipe_write_from_isr` | 否 | 是 | 否 |
| 定时器 create/start/stop/delete | 是 | 否 | 否 |
| 定时器回调 | timer service task | 否 | 必须有界 |
| 堆 alloc/realloc/free | 是 | 否 | 不阻塞，但分配是 O(N) |
| 固定块池 alloc/free | 是 | 否 | 否，O(1) |
| MPU region add | 是 | 否 | 否 |
| trace record | 是 | 是 | 否 |
| crash record 读/清除 | 启动或诊断任务 | 否 | 否 |

## 任务创建 API

| API | 存储所有权 | MPU 行为 |
|---|---|---|
| `aixos_task_create()` | 从内核堆分配 TCB 和栈，`stack_size` 不小于 `AIXOS_CFG_MIN_TASK_STACK_SIZE` | 内核任务 |
| `aixos_user_task_create()` | 从内核堆分配 TCB 和栈 | 用户任务，栈注册为可读写用户 MPU region |
| `aixos_task_create_static()` | 使用调用者拥有的 TCB 和栈，直到任务删除前必须有效 | 内核任务 |
| `aixos_user_task_create_static()` | 使用调用者拥有的 TCB 和栈 | 用户任务，栈注册为可读写用户 MPU region |

## Buffer 和所有权规则

- 消息队列发送最多复制队列配置的消息大小。
- 消息队列接收必须传入目的容量；目的过小时不消费消息。
- 管道操作返回已传输字节数或负错误码。
- 静态任务栈、TCB、静态队列、管道和固定块池存储必须在对象生命周期内保持有效。
- `aixos_mempool_free()` 只接受 `aixos_mempool_alloc()` 返回的精确地址，并拒绝 double free。
- 定时器回调不能假设自己处在 ISR 中，且必须保持有界。
- 用户 MPU region 必须是自然对齐的 2 的幂范围；可写 region 必须可读。
- syscall handler 必须使用 `aixos_copy_from_user()`、`aixos_copy_to_user()` 或 `aixos_zero_to_user()` 传输用户 buffer。`aixos_user_memory_check()` 只是校验原语，不能替代 copy helper。

## 生产配置原则

- 尽量在系统初始化阶段创建动态对象。
- 生产固件建议启用 `AIXOS_CFG_HEAP_LOCK_ON_START=1`。
- 动态任务创建是公共堆锁定后的受控内核例外；TCB、栈和任务槽元数据仍受完整性检查和统计。
- 运行期数据优先使用静态对象和固定块池。
- `aixos_heap_check() != AIXOS_OK`、栈 guard 失败、crash CRC 无效和异常 trace 覆盖增长都应视为安全监控事件。
- 任务删除需要应用层取消和资源所有权协议；任意强制删除不是恢复机制。

## ABI 规则

- ABI 可见记录使用固定宽度整数。
- Crash record 和 trace record 在 `include/aixos/abi.h` 中有编译期大小断言。
- Handle 是 32 位值，低 8 位为槽位索引，高 24 位为 generation。
- 不兼容的公共结构或语义变化必须更新 `AIXOS_ABI_VERSION`。
