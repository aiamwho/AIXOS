#ifndef AIXOS_EVENT_H
#define AIXOS_EVENT_H
#include "aixos/types.h"
aixos_handle_t aixos_event_create(void);
int aixos_event_wait(aixos_handle_t ev, uint32_t mask, uint8_t mode,
                     uint32_t timeout_ms, uint32_t *matched);
int aixos_event_set(aixos_handle_t ev, uint32_t flags);
int aixos_event_clear(aixos_handle_t ev, uint32_t flags);
int aixos_event_delete(aixos_handle_t ev);
#ifdef AIXOS_HOST_TEST
int aixos_test_event_add_waiter(aixos_handle_t ev, aixos_handle_t task,
                                uint32_t mask, uint8_t mode);
#endif
#endif
