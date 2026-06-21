#ifndef AIXOS_ABI_H
#define AIXOS_ABI_H

#include "aixos/types.h"
#include "aixos/version.h"

#define AIXOS_STATIC_ASSERT(name, expression) \
    typedef char aixos_static_assert_##name[(expression) ? 1 : -1]

AIXOS_STATIC_ASSERT(handle_is_32_bit, sizeof(aixos_handle_t) == 4U);
AIXOS_STATIC_ASSERT(crash_record_is_60_bytes,
                    sizeof(aixos_crash_record_t) == 60U);
AIXOS_STATIC_ASSERT(trace_entry_is_20_bytes,
                    sizeof(aixos_trace_entry_t) == 20U);

#endif
