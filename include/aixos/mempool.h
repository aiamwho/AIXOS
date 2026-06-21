#ifndef AIXOS_MEMPOOL_H
#define AIXOS_MEMPOOL_H

#include "aixos/types.h"

#define AIXOS_MEMPOOL_MAX_BLOCKS 32U

typedef struct {
    uint8_t *storage;
    uint32_t storage_size;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t free_bitmap;
    uint32_t allocated;
    uint32_t peak_allocated;
    uint32_t alloc_failures;
} aixos_mempool_t;

int aixos_mempool_init(aixos_mempool_t *pool, void *storage,
                       size_t storage_size, size_t block_size,
                       uint32_t block_count);
void *aixos_mempool_alloc(aixos_mempool_t *pool);
int aixos_mempool_free(aixos_mempool_t *pool, void *block);
int aixos_mempool_owns(const aixos_mempool_t *pool, const void *block);

#endif
