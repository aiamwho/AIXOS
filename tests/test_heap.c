#include <stdint.h>
#include "test.h"
#include "aixos/heap.h"
#include "aixos/mempool.h"

void test_heap_allocator(void)
{
    aixos_mem_info_t before;
    aixos_mem_info_t after;
    void *a;
    void *b;
    void *c;

    aixos_heap_init(NULL, 0);
    aixos_mem_info(&before);
    a = aixos_malloc(1);
    b = aixos_calloc(8, 4);
    CHECK(a != NULL);
    CHECK(b != NULL);
    CHECK(((uintptr_t)a & 7U) == 0U);
    CHECK(((uintptr_t)b & 7U) == 0U);
    if (b != NULL) {
        CHECK(((unsigned char *)b)[0] == 0U);
    }
    c = aixos_realloc(a, 64);
    CHECK(c != NULL);
    aixos_free(c);
    aixos_free(b);
    aixos_mem_info(&after);
    CHECK(after.free_bytes == before.free_bytes);
    CHECK(aixos_calloc((size_t)-1, 2) == NULL);
    CHECK(aixos_heap_check() == AIXOS_OK);
    CHECK(after.peak_used_bytes > 0U);
    CHECK(after.free_block_count == 1U);
    CHECK(after.fragmentation_per_mille == 0U);

    a = aixos_malloc(8U);
    CHECK(a != NULL);
    ((uint8_t *)a)[8] ^= 1U;
    CHECK(aixos_heap_check() == AIXOS_ERR_CORRUPT);
    aixos_mem_info(&after);
    CHECK(after.corruption_count > 0U);
    aixos_heap_init(NULL, 0U);
    CHECK(aixos_heap_check() == AIXOS_OK);
}

void test_fixed_block_pool(void)
{
    static uintptr_t storage[8];
    aixos_mempool_t pool;
    void *blocks[5];
    uint32_t i;

    CHECK(aixos_mempool_init(&pool, storage, sizeof(storage), 12U, 4U) ==
          AIXOS_OK);
    CHECK(pool.block_size >= 12U);
    for (i = 0U; i < 4U; i++) {
        blocks[i] = aixos_mempool_alloc(&pool);
        CHECK(blocks[i] != NULL);
        CHECK(aixos_mempool_owns(&pool, blocks[i]));
    }
    blocks[4] = aixos_mempool_alloc(&pool);
    CHECK(blocks[4] == NULL);
    CHECK(pool.alloc_failures == 1U);
    CHECK(pool.peak_allocated == 4U);
    CHECK(aixos_mempool_free(&pool, blocks[1]) == AIXOS_OK);
    CHECK(aixos_mempool_free(&pool, blocks[1]) == AIXOS_ERR_INVAL);
    CHECK(aixos_mempool_alloc(&pool) == blocks[1]);
    CHECK(aixos_mempool_free(&pool, (uint8_t *)blocks[0] + 1U) ==
          AIXOS_ERR_INVAL);
}
