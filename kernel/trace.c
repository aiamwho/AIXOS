#include "aixos/types.h"
#include "aixos/trace.h"
#include "aixos/task.h"
#include "aixos/heap.h"
#include "kernel/sched.h"
#include "config/aixos_cfg.h"
#include "config/string.h"
#include "aixos/arch/arch.h"

#if AIXOS_CFG_TRACE_ENABLE

static aixos_trace_entry_t trace_buf[AIXOS_CFG_TRACE_BUFFER_SIZE];
static uint32_t trace_next_sequence;
static uint32_t trace_count;
static uint32_t trace_overwritten;

void aixos_trace_init(void)
{
    memset(trace_buf, 0, sizeof(trace_buf));
    trace_next_sequence = 0U;
    trace_count = 0U;
    trace_overwritten = 0U;
}

void aixos_trace_record(aixos_trace_event_t event, uint32_t d0, uint32_t d1)
{
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    uint32_t idx = trace_next_sequence % AIXOS_CFG_TRACE_BUFFER_SIZE;
    if (trace_count < AIXOS_CFG_TRACE_BUFFER_SIZE) {
        trace_count++;
    } else {
        trace_overwritten++;
    }
    trace_buf[idx].sequence = trace_next_sequence;
    trace_buf[idx].timestamp = aixos_get_tick();
    trace_buf[idx].event = (uint16_t)event;
    trace_buf[idx].reserved = 0U;
    trace_buf[idx].arg0 = d0;
    trace_buf[idx].arg1 = d1;
    trace_next_sequence++;
    aixos_arch_int_restore(flags);
}

uint32_t aixos_trace_snapshot(aixos_trace_entry_t *entries,
                              uint32_t capacity,
                              aixos_trace_info_t *info)
{
    uint32_t copy_count;
    uint32_t start;
    uint32_t i;
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    if (info != NULL) {
        info->available = trace_count;
        info->capacity = AIXOS_CFG_TRACE_BUFFER_SIZE;
        info->overwritten = trace_overwritten;
    }
    copy_count = capacity < trace_count ? capacity : trace_count;
    start = trace_next_sequence - copy_count;
    if (entries != NULL) {
        for (i = 0U; i < copy_count; i++) {
            entries[i] = trace_buf[(start + i) %
                                   AIXOS_CFG_TRACE_BUFFER_SIZE];
        }
    } else {
        copy_count = 0U;
    }
    aixos_arch_int_restore(flags);
    return copy_count;
}

void aixos_trace_get_info(aixos_trace_info_t *info)
{
    (void)aixos_trace_snapshot(NULL, 0U, info);
}

void aixos_trace_dump(void (*output)(const char *))
{
    uint32_t sequence;
    uint32_t end;
    char line[64];
    aixos_arch_flags_t flags = aixos_arch_int_disable();
    sequence = trace_next_sequence - trace_count;
    end = trace_next_sequence;
    aixos_arch_int_restore(flags);
    for (; sequence < end; sequence++) {
        aixos_trace_entry_t entry;
        flags = aixos_arch_int_disable();
        entry = trace_buf[sequence % AIXOS_CFG_TRACE_BUFFER_SIZE];
        aixos_arch_int_restore(flags);
        if (entry.sequence != sequence) {
            if (output != NULL) {
                output("[trace entry overwritten]");
            }
            continue;
        }
        aixos_snprintf(line, sizeof(line), "[%u] ev=%d d0=0x%X d1=0x%X",
                       entry.timestamp, entry.event, entry.arg0, entry.arg1);
        if (output) output(line);
    }
}

#else
void aixos_trace_init(void) {}
void aixos_trace_record(aixos_trace_event_t event, uint32_t d0, uint32_t d1)
{ (void)event; (void)d0; (void)d1; }
void aixos_trace_dump(void (*output)(const char *)) { (void)output; }
uint32_t aixos_trace_snapshot(aixos_trace_entry_t *entries, uint32_t capacity,
                              aixos_trace_info_t *info)
{
    (void)entries;
    (void)capacity;
    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
    return 0U;
}
void aixos_trace_get_info(aixos_trace_info_t *info)
{
    (void)aixos_trace_snapshot(NULL, 0U, info);
}
#endif

void aixos_sys_info(aixos_sys_info_t *info)
{
    aixos_mem_info_t memory;
    aixos_sched_stats_t stats;
    if (!info) return;
    memset(info, 0, sizeof(*info));
    aixos_sched_stats_snapshot(&stats);
    info->total_ticks = stats.total_ticks;
    info->idle_ticks = stats.idle_ticks;
    info->switch_count = stats.switch_count;
    info->task_count = aixos_task_count();
    aixos_mem_info(&memory);
    info->heap_used = memory.used_bytes;
    info->heap_total = memory.total_bytes;
#if AIXOS_CFG_CPU_USAGE_ENABLE
    info->cpu_usage = aixos_cpu_usage_get();
#endif
}
