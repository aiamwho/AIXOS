#ifndef AIXOS_ARCH_H
#define AIXOS_ARCH_H

#include <stdint.h>

struct aixos_tcb;

/*
 * AIXOS 架构移植层 HAL 接口
 *
 * 每个目标架构必须实现以下 6 个原语。
 * 这些原语是 AIXOS 内核与硬件之间的唯一耦合点。
 *
 * 需要实现的接口:
 *   aixos_arch_int_disable     — 关中断并返回状态字
 *   aixos_arch_int_restore     — 恢复中断状态字
 *   aixos_arch_stack_init      — 初始化任务栈帧, 返回栈顶指针
 *   aixos_arch_context_switch  — 请求调度 (触发上下文切换)
 *   aixos_arch_start_first_task— 启动第一个任务
 *   aixos_arch_tick_handler    — 系统时钟中断处理函数
 */

/* 中断状态保存类型 */
typedef uint32_t aixos_arch_flags_t;

/* ── 临界区保护 ──────────────────────────── */
aixos_arch_flags_t aixos_arch_int_disable(void);
void               aixos_arch_int_restore(aixos_arch_flags_t flags);

/* ── 栈帧初始化 ──────────────────────────── */
/* entry: 任务入口函数; stack_top: 栈顶地址; arg: 入口参数 */
/* 返回: 初始化后的栈指针 (架构相关) */
void *aixos_arch_stack_init(void (*entry)(void*), void *stack_top, void *arg,
                            int user_mode);

/* ── 上下文切换 ──────────────────────────── */
void aixos_arch_context_switch(void);

/* ── 启动第一个任务 (在 arch port 中实现) ── */
#ifdef AIXOS_HOST_TEST
void aixos_arch_start_first_task(void);
#else
void aixos_arch_start_first_task(void) __attribute__((noreturn));
#endif

/* ── 时钟中断入口 ──────────────────────────── */
void aixos_arch_tick_handler(void);

/* ── 系统初始化 (架构相关, 如设置 SysTick) ── */
void aixos_arch_system_init(void);
void aixos_arch_systick_enable(void);
void aixos_arch_mpu_configure_task(const struct aixos_tcb *task);

/* Architecture exception entry/exit calls these kernel hooks. */
void aixos_isr_enter(void);
void aixos_isr_exit(void);
int aixos_in_isr(void);
uint32_t aixos_isr_nesting_level(void);
uint32_t aixos_isr_nesting_high_watermark(void);
uint32_t aixos_isr_nesting_overflow_count(void);
void aixos_isr_stats_reset(void);
void aixos_reschedule_request(void);

#if defined(__riscv)
void aixos_arch_timer_ack(void);
void aixos_arch_software_ack(void);
void aixos_arch_trigger_switch(void);
#endif

#endif /* AIXOS_ARCH_H */
