#ifndef AIXOS_HEAP_INTERNAL_H
#define AIXOS_HEAP_INTERNAL_H

#include <stddef.h>

/*
 * Trusted kernel metadata allocation. This bypasses public heap lockdown but
 * retains the same heap integrity checks and accounting.
 */
void *aixos_kernel_malloc(size_t size);

#endif
