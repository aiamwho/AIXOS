#ifndef AIXOS_TYPES_H
#define AIXOS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── 句柄类型 ──────────────────────────────── */
typedef int32_t aixos_handle_t;

#define AIXOS_HANDLE_INVALID    ((aixos_handle_t)-1)
#define AIXOS_HANDLE_MAKE(idx, gen)  \
    ((aixos_handle_t)((((uint32_t)(gen) & 0x00FFFFFFU) << 8) | \
                      ((uint32_t)(idx) & 0xFFU)))
#define AIXOS_HANDLE_IDX(h)          ((uint32_t)(h) & 0xFFU)
#define AIXOS_HANDLE_GEN(h)          (((uint32_t)(h) >> 8) & 0x00FFFFFFU)

/* ── 错误码 ────────────────────────────────── */
#define AIXOS_OK                0
#define AIXOS_ERR              -1
#define AIXOS_ERR_TIMEOUT      -2
#define AIXOS_ERR_BUSY         -3
#define AIXOS_ERR_INVAL        -4
#define AIXOS_ERR_NOMEM        -5
#define AIXOS_ERR_AGAIN        -6
#define AIXOS_ERR_CONTEXT      -7
#define AIXOS_ERR_OVERFLOW     -8
#define AIXOS_ERR_LOCKED       -9
#define AIXOS_ERR_CORRUPT      -10
#define AIXOS_ERR_INTR         -11
#define AIXOS_ERR_PERM         -12
#define AIXOS_ERR_FAULT        -13
#define AIXOS_ERR_NOSYS        -14
#define AIXOS_ERR_RESET        -15
#define AIXOS_ERR_NOT_FOUND    -16
#define AIXOS_ERR_CANCELED     -17
#define AIXOS_ERR_DEADLOCK     -18

/* ── 内核对象类型 ──────────────────────────── */
typedef enum {
    AIXOS_OBJ_UNUSED    = 0,
    AIXOS_OBJ_TASK      = 1,
    AIXOS_OBJ_SEM       = 2,
    AIXOS_OBJ_MUTEX     = 3,
    AIXOS_OBJ_MQ        = 4,
    AIXOS_OBJ_EVENT     = 5,
    AIXOS_OBJ_PIPE      = 6,
    AIXOS_OBJ_TIMER     = 7,
    AIXOS_OBJ_CHANNEL   = 8,
    AIXOS_OBJ_CONNECTION= 9,
    AIXOS_OBJ_SIGNAL    = 10,
    AIXOS_OBJ_NAMESPACE = 11,
} aixos_obj_type_t;

/* ── 任务状态 ──────────────────────────────── */
typedef enum {
    AIXOS_TASK_READY    = 0,
    AIXOS_TASK_RUNNING  = 1,
    AIXOS_TASK_BLOCKED  = 2,
    AIXOS_TASK_DELAYED  = 3,
    AIXOS_TASK_SUSPENDED= 4,
    AIXOS_TASK_STOP     = 5,
} aixos_task_state_t;

typedef enum {
    AIXOS_SCHED_FIFO        = 0,
    AIXOS_SCHED_RR          = 1,
} aixos_sched_policy_t;

typedef enum {
    AIXOS_DOMAIN_KERNEL = 0,
    AIXOS_DOMAIN_USER   = 1,
} aixos_domain_t;

/* ── 事件标志等待模式 ──────────────────────── */
#define AIXOS_EVENT_AND     0x01
#define AIXOS_EVENT_OR      0x02
#define AIXOS_EVENT_CLEAR   0x04

/* ── 定时器类型 ────────────────────────────── */
typedef enum {
    AIXOS_TIMER_ONESHOT     = 0,
    AIXOS_TIMER_PERIODIC    = 1,
} aixos_timer_type_t;

/* ── 跟踪事件类型 ──────────────────────────── */
typedef enum {
    AIXOS_TRACE_NONE        = 0,
    AIXOS_TRACE_TASK_SWITCH = 1,
    AIXOS_TRACE_TASK_CREATE = 2,
    AIXOS_TRACE_TASK_DELETE = 3,
    AIXOS_TRACE_IPC_WAIT    = 4,
    AIXOS_TRACE_IPC_POST    = 5,
    AIXOS_TRACE_TIMER       = 6,
    AIXOS_TRACE_MEM_ALLOC   = 7,
    AIXOS_TRACE_MEM_FREE    = 8,
    AIXOS_TRACE_ISR_ENTER   = 9,
    AIXOS_TRACE_ISR_EXIT    = 10,
    AIXOS_TRACE_STACK_GUARD = 11,
} aixos_trace_event_t;

/* ── 系统信息 ──────────────────────────────── */
typedef struct {
    uint64_t    total_ticks;
    uint64_t    idle_ticks;
    uint64_t    switch_count;
    uint32_t    task_count;
    uint32_t    heap_used;
    uint32_t    heap_total;
    uint32_t    cpu_usage;   /* 百分比 * 100 整数 */
} aixos_sys_info_t;

/* ── 任务信息 ──────────────────────────────── */
typedef struct {
    char        name[16];
    aixos_handle_t handle;
    int         priority;
    aixos_task_state_t state;
    uint32_t    stack_size;
    uint32_t    stack_used;
    uint64_t    runtime_ticks;
    uint64_t    switch_count;
    uint32_t    stack_free;
    uint8_t     stack_guard_ok;
} aixos_task_info_t;

/* ── 内存信息 ──────────────────────────────── */
typedef struct {
    uint32_t    total_bytes;
    uint32_t    free_bytes;
    uint32_t    used_bytes;
    uint32_t    max_free_block;
    uint32_t    alloc_count;
    uint32_t    free_count;
    uint32_t    alloc_failures;
    uint32_t    corruption_count;
    uint32_t    peak_used_bytes;
    uint32_t    free_block_count;
    uint32_t    fragmentation_per_mille;
    uint8_t     runtime_locked;
} aixos_mem_info_t;

typedef struct {
    uint64_t total_ticks;
    uint64_t idle_ticks;
    uint64_t switch_count;
} aixos_sched_stats_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint32_t build_id;
    uint32_t architecture;
    uint32_t reason;
    uint32_t program_counter;
    uint32_t fault_address;
    uint32_t stack_pointer;
    uint32_t task_handle;
    uint32_t tick;
    uint32_t fault_status;
    uint32_t fault_status2;
    uint32_t auxiliary;
    uint32_t crc32;
} aixos_crash_record_t;

typedef struct {
    uint32_t sequence;
    uint32_t timestamp;
    uint16_t event;
    uint16_t reserved;
    uint32_t arg0;
    uint32_t arg1;
} aixos_trace_entry_t;

typedef struct {
    uint32_t available;
    uint32_t capacity;
    uint32_t overwritten;
} aixos_trace_info_t;

typedef struct {
    uint32_t reset_count;
    uint32_t last_crash_reason;
    uint32_t uptime_ticks;
    uint32_t boot_count;
    uint8_t safe_mode;
} aixos_system_status_t;

typedef struct {
    uint32_t sigaction_count;
    uint32_t sigpending_count;
    uint32_t signo;
    uint32_t sigvalue;
} aixos_signal_info_t;

#endif /* AIXOS_TYPES_H */
