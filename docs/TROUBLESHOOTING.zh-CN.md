# AIXOS v1.0 排障指南

本文列出客户集成中常见的构建、运行、MPU 和 Renode 问题。

## 构建失败

### 找不到 RISC-V 编译器

如果提示缺少 `riscv-none-elf-gcc`，先运行：

```sh
make toolcheck
```

若本机安装的是 `riscv64-elf-gcc`：

```sh
make riscv RISCV_PREFIX=riscv64-elf-
make renode-riscv RISCV_PREFIX=riscv64-elf-
```

如果工具链不在 `PATH`：

```sh
make riscv RISCV_TOOLCHAIN_DIR=/path/to/toolchain
```

### 链接溢出

检查 linker script 的 FLASH/RAM 区域、`AIXOS_CFG_HEAP_SIZE`、对象池上限、trace buffer、任务栈数量和栈大小。使用：

```sh
make ram-report RISCV_PREFIX=riscv64-elf-
```

## 启动和调度问题

### 固件启动但任务不运行

检查：

- 是否调用 `aixos_task_init()`、`aixos_timer_init()`、可选 `aixos_namespace_init()`、可选 `aixos_timing_wheel_init()`、`aixos_sched_init()`。
- 是否至少创建了一个 runnable 应用任务。
- 是否在 `aixos_start()` 前调用了 `aixos_arch_system_init()`。
- tick 中断是否启动，PendSV/trap 路径是否正确。

### tick 计数保持为零

检查 `AIXOS_CFG_CPU_CLOCK_HZ`、`AIXOS_CFG_SYSTICK_HZ`、启动时钟、SysTick 或 RISC-V timer 中断配置，以及 Renode 平台中的时钟/中断描述。

## 任务和栈问题

### `aixos_task_create()` 返回 `AIXOS_HANDLE_INVALID`

常见原因：

- 栈小于 `AIXOS_CFG_MIN_TASK_STACK_SIZE`。
- 动态任务槽位耗尽。
- 堆空间不足或已 lockdown。
- 优先级超出 `AIXOS_CFG_MAX_PRIORITY`。

### 栈 guard 失败

增大任务栈，检查递归、过大的栈上对象、中断嵌套和编译器保存寄存器。用 `aixos_task_stack_check()` 或 trace/crash 记录定位。

## IPC 问题

### 非阻塞 wait 立即返回

`timeout_ms == 0` 明确定义为非阻塞。需要等待时传入非零 timeout，或使用 `UINT32_MAX` 无限等待。

### 阻塞 API 返回 `AIXOS_ERR_LOCKED`

调度器锁定时不能调用可能阻塞或移除当前任务的 API。缩短 critical section，把阻塞操作移出 `aixos_sched_lock()`/`unlock()` 区间。

### ISR API 拒绝大消息

ISR copy 大小受 `AIXOS_CFG_ISR_COPY_MAX_BYTES` 限制。中断里只传递短事件或指针/索引，把大数据复制放到任务上下文。

## MPU 和用户模式问题

### `aixos_task_mpu_region_add()` 返回 `AIXOS_ERR_INVAL`

检查：

- region size 是 2 的幂。
- region size 不小于 `AIXOS_CFG_MPU_MIN_REGION_SIZE`。
- base 地址按 region size 对齐。
- 可写 region 同时包含 `AIXOS_MPU_READ`。
- task handle 有效且对应 user task。

### 用户 buffer 访问返回 `AIXOS_ERR_FAULT`

检查是否注册了精确 buffer 范围、访问大小是否在范围内、输出 buffer 是否授予写权限。不要把内核栈、TCB、heap metadata 或对象池地址传给用户任务。

## Renode 问题

### Cortex-M Renode heartbeat 或 tick 断言失败

检查：

- 重新运行 `make arm`。
- `examples/smoke/main.c` 或客户 `APP_SRCS` 是否在 `aixos_start()` 前创建 runnable task。
- Robot 脚本中的 ELF 路径是否指向当前 build 输出。
- 平台文件是否匹配 linker memory map。

### RISC-V Renode runner 在测试开始前超时

如果错误是：

```text
Couldn't access port file for Renode instance
```

说明 Robot 还没进入固件测试体。先检查本地 Renode 安装、robot-server 启动、临时目录权限和 port file 处理，再判断是否为固件回归。

### Instruction benchmark 日志出现空地址访问

如果在 `aixos_timing_wheel_tick()` 附近反复看到 `ReadDoubleWord from non existing peripheral at 0x0`，通常说明应用启用了 `AIXOS_CFG_ENABLE_TIME_WHEEL`，但启动调度器前没有调用 `aixos_timing_wheel_init()`。

修复初始化顺序：

```c
aixos_timer_init();
#if AIXOS_CFG_ENABLE_NAMESPACE
aixos_namespace_init();
#endif
#if AIXOS_CFG_ENABLE_TIME_WHEEL
aixos_timing_wheel_init();
#endif
aixos_sched_init();
```

## 诊断收集

客户支持时请收集：

- 精确命令和完整终端输出。
- `make toolcheck` 的编译器版本。
- `config/aixos_cfg.h`。
- Linker script 和 map 文件。
- 失败目标的 ELF。
- Renode Robot 输出或硬件日志。
- 可用的 crash record 和 trace snapshot。
