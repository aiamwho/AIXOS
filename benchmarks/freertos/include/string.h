#ifndef FREERTOS_BENCH_STRING_H
#define FREERTOS_BENCH_STRING_H

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length);
void *memset(void *destination, int value, size_t length);

#endif
