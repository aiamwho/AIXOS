#include "aixos/mempool.h"
#include "aixos/arch/arch.h"

static uint32_t align_up(uint32_t value)
{
    uint32_t alignment = (uint32_t)sizeof(uintptr_t);
    return (value + alignment - 1U) & ~(alignment - 1U);
}

int aixos_mempool_init(aixos_mempool_t *pool, void *storage,
                       size_t storage_size, size_t block_size,
                       uint32_t block_count)
{
    uint32_t aligned_size;
    if (pool == NULL || storage == NULL || block_size == 0U ||
        block_size > UINT32_MAX || block_count == 0U ||
        block_count > AIXOS_MEMPOOL_MAX_BLOCKS ||
        ((uintptr_t)storage & (sizeof(uintptr_t) - 1U)) != 0U) {
        return AIXOS_ERR_INVAL;
    }
    aligned_size = align_up((uint32_t)block_size);
    if (aligned_size > UINT32_MAX / block_count ||
        storage_size < (size_t)aligned_size * block_count) {
        return AIXOS_ERR_INVAL;
    }
    pool->storage = (uint8_t *)storage;
    pool->storage_size = aligned_size * block_count;
    pool->block_size = aligned_size;
    pool->block_count = block_count;
    pool->free_bitmap = block_count == 32U ?
                        UINT32_MAX : (UINT32_C(1) << block_count) - 1U;
    pool->allocated = 0U;
    pool->peak_allocated = 0U;
    pool->alloc_failures = 0U;
    return AIXOS_OK;
}

void *aixos_mempool_alloc(aixos_mempool_t *pool)
{
    uint32_t available;
    uint32_t index;
    aixos_arch_flags_t flags;
    if (pool == NULL || aixos_in_isr()) {
        return NULL;
    }
    flags = aixos_arch_int_disable();
    available = pool->free_bitmap;
    if (available == 0U) {
        pool->alloc_failures++;
        aixos_arch_int_restore(flags);
        return NULL;
    }
    index = (uint32_t)__builtin_ctz(available);
    pool->free_bitmap &= ~(UINT32_C(1) << index);
    pool->allocated++;
    if (pool->allocated > pool->peak_allocated) {
        pool->peak_allocated = pool->allocated;
    }
    aixos_arch_int_restore(flags);
    return pool->storage + index * pool->block_size;
}

int aixos_mempool_owns(const aixos_mempool_t *pool, const void *block)
{
    uintptr_t offset;
    if (pool == NULL || pool->storage == NULL || block == NULL ||
        (const uint8_t *)block < pool->storage ||
        (const uint8_t *)block >= pool->storage + pool->storage_size) {
        return 0;
    }
    offset = (uintptr_t)((const uint8_t *)block - pool->storage);
    return (offset % pool->block_size) == 0U;
}

int aixos_mempool_free(aixos_mempool_t *pool, void *block)
{
    uint32_t index;
    uint32_t mask;
    aixos_arch_flags_t flags;
    if (aixos_in_isr()) {
        return AIXOS_ERR_CONTEXT;
    }
    if (!aixos_mempool_owns(pool, block)) {
        return AIXOS_ERR_INVAL;
    }
    index = (uint32_t)(((uint8_t *)block - pool->storage) / pool->block_size);
    mask = UINT32_C(1) << index;
    flags = aixos_arch_int_disable();
    if ((pool->free_bitmap & mask) != 0U) {
        aixos_arch_int_restore(flags);
        return AIXOS_ERR_INVAL;
    }
    pool->free_bitmap |= mask;
    pool->allocated--;
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}
