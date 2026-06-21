#ifndef AIXOS_CRASH_H
#define AIXOS_CRASH_H

#include "aixos/types.h"

void aixos_crash_record_store(uint32_t architecture, uint32_t reason,
                              uint32_t program_counter,
                              uint32_t fault_address,
                              uint32_t stack_pointer);
void aixos_crash_record_store_extended(uint32_t architecture, uint32_t reason,
                                       uint32_t program_counter,
                                       uint32_t fault_address,
                                       uint32_t stack_pointer,
                                       uint32_t fault_status,
                                       uint32_t fault_status2,
                                       uint32_t auxiliary);
const aixos_crash_record_t *aixos_crash_record_get(void);
int aixos_crash_record_validate(const aixos_crash_record_t *record);
void aixos_crash_record_clear(void);
void aixos_arm_fault_handler(uint32_t *frame, uint32_t reason);

#endif
