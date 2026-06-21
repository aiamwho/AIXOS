#ifndef AIXOS_TRACE_H
#define AIXOS_TRACE_H
#include "aixos/types.h"
void aixos_trace_init(void);
void aixos_trace_record(aixos_trace_event_t event, uint32_t d0, uint32_t d1);

#if AIXOS_CFG_TRACE_ENABLE
#define AIXOS_TRACE(event, d0, d1)  aixos_trace_record(event, (uint32_t)(d0), (uint32_t)(d1))
#else
#define AIXOS_TRACE(event, d0, d1)  ((void)0)
#endif
void aixos_trace_dump(void (*output)(const char *));
uint32_t aixos_trace_snapshot(aixos_trace_entry_t *entries,
                              uint32_t capacity,
                              aixos_trace_info_t *info);
void aixos_trace_get_info(aixos_trace_info_t *info);
uint32_t aixos_cpu_usage_get(void);
uint32_t aixos_get_tick(void);
void aixos_sys_info(aixos_sys_info_t *info);
#endif
