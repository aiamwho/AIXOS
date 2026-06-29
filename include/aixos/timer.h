#ifndef AIXOS_TIMER_H
#define AIXOS_TIMER_H
#include "aixos/types.h"
typedef void (*aixos_timer_callback_t)(void *arg);
aixos_handle_t aixos_timer_create(const char *name, aixos_timer_type_t type, aixos_timer_callback_t callback, void *arg);
int aixos_timer_start(aixos_handle_t timer, uint32_t interval_ms);
int aixos_timer_stop(aixos_handle_t timer);
int aixos_timer_delete(aixos_handle_t timer);
void aixos_timer_init(void);
void aixos_timer_tick(uint32_t now);
int aixos_timer_service_start(void);
unsigned int aixos_timer_dispatch(void);
#ifdef AIXOS_HOST_TEST
void aixos_test_timer_service_entry(void);
#endif
#endif
