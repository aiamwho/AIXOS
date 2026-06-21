#ifndef AIXOS_HEAP_H
#define AIXOS_HEAP_H
#include "aixos/types.h"
void aixos_heap_init(void *start, size_t size);
void *aixos_malloc(size_t size);
void *aixos_calloc(size_t count, size_t size);
void *aixos_realloc(void *ptr, size_t new_size);
void  aixos_free(void *ptr);
void  aixos_mem_info(aixos_mem_info_t *info);
int   aixos_heap_check(void);
void  aixos_heap_lockdown(void);
int   aixos_heap_is_locked(void);
#endif
