#include "aixos/types.h"
#include "aixos/heap.h"
#include "aixos/trace.h"
#include "arch/include/aixos/arch/arch.h"
#include "config/aixos_cfg.h"
#include "config/string.h"
#include "kernel/heap_internal.h"

#define HEAP_MAGIC_FREE    0xF1EE
#define HEAP_MAGIC_USED    0x1ADD
#define HEAP_ALIGN         8
#define HEAP_CANARY        UINT32_C(0xBEEFC0DE)
#define HEAP_CANARY_SIZE   ((uint32_t)sizeof(uint32_t))
#define HEAP_HEADER_SIZE   (sizeof(heap_hdr_t))

typedef struct heap_hdr {
    uint16_t       magic;
    uint16_t       reserved;
    uint32_t       size;
    uint32_t       requested_size;
    uint32_t       header_guard;
    struct heap_hdr *next;
    struct heap_hdr *prev;
} heap_hdr_t;

static char         heap_pool[AIXOS_CFG_HEAP_SIZE] __attribute__((aligned(HEAP_ALIGN)));
static heap_hdr_t  *heap_first = NULL;
static uintptr_t    heap_begin = 0U;
static uintptr_t    heap_end = 0U;
static uint32_t     heap_total = 0U;
static int          heap_initialized = 0;
static uint32_t     alloc_count = 0;
static uint32_t     free_count = 0;
static uint32_t     alloc_failures = 0;
static uint32_t     corruption_count = 0;
static uint32_t     current_used = 0;
static uint32_t     peak_used = 0;
static uint8_t      runtime_locked = 0U;

static uint32_t heap_header_guard(const heap_hdr_t *hdr)
{
    return (uint32_t)((uintptr_t)hdr ^ hdr->size ^ hdr->requested_size ^
                      AIXOS_CFG_HEAP_MAGIC);
}

static int heap_header_valid(const heap_hdr_t *hdr)
{
    uintptr_t address = (uintptr_t)hdr;
    if (address < heap_begin || address + HEAP_HEADER_SIZE > heap_end ||
        (hdr->magic != HEAP_MAGIC_FREE && hdr->magic != HEAP_MAGIC_USED) ||
        hdr->size > heap_end - address - HEAP_HEADER_SIZE ||
        hdr->header_guard != heap_header_guard(hdr)) {
        return 0;
    }
    if (hdr->magic == HEAP_MAGIC_USED &&
        (hdr->requested_size > hdr->size ||
         hdr->requested_size + HEAP_CANARY_SIZE > hdr->size)) {
        return 0;
    }
    return 1;
}

static void heap_write_canary(heap_hdr_t *hdr)
{
    uint32_t canary = HEAP_CANARY ^ (uint32_t)(uintptr_t)hdr;
    memcpy((char *)hdr + HEAP_HEADER_SIZE + hdr->requested_size,
           &canary, sizeof(canary));
}

static int heap_canary_valid(const heap_hdr_t *hdr)
{
    uint32_t actual;
    uint32_t expected = HEAP_CANARY ^ (uint32_t)(uintptr_t)hdr;
    memcpy(&actual, (const char *)hdr + HEAP_HEADER_SIZE +
                    hdr->requested_size, sizeof(actual));
    return actual == expected;
}

void aixos_heap_init(void *start, size_t size)
{
    heap_hdr_t *hdr;
    if (start != NULL && size >= HEAP_HEADER_SIZE + HEAP_ALIGN) {
        heap_first = (heap_hdr_t *)start;
        size &= ~(size_t)(HEAP_ALIGN - 1U);
    } else {
        heap_first = (heap_hdr_t *)heap_pool;
        size = AIXOS_CFG_HEAP_SIZE;
    }
    hdr = heap_first;
    hdr->magic = HEAP_MAGIC_FREE;
    hdr->reserved = 0;
    hdr->size  = (uint32_t)(size - HEAP_HEADER_SIZE);
    hdr->requested_size = 0U;
    hdr->header_guard = 0U;
    hdr->next  = NULL;
    hdr->prev  = NULL;
    heap_begin = (uintptr_t)start;
    if (start == NULL || size == AIXOS_CFG_HEAP_SIZE) {
        heap_begin = (uintptr_t)heap_first;
    }
    heap_end = heap_begin + size;
    heap_total = (uint32_t)size;
    heap_initialized = 1;
    alloc_count = 0;
    free_count = 0;
    alloc_failures = 0;
    corruption_count = 0;
    current_used = 0;
    peak_used = 0;
    runtime_locked = 0U;
    hdr->header_guard = heap_header_guard(hdr);
}

static void *heap_allocate(size_t size, int kernel_allocation)
{
    heap_hdr_t *hdr, *new_hdr;
    aixos_arch_flags_t flags;
    void *result;
    size_t requested;
    if (aixos_in_isr()) return NULL;
    if (!heap_initialized) return NULL;
    if (size == 0) size = 1;

    /* ISR allocation limit and user-space large-allocation limit. */
    if (size > AIXOS_CFG_MAX_IPC_COPY_BYTES && !kernel_allocation) {
        return NULL;
    }
    requested = size;
    if (size > UINT32_MAX - HEAP_CANARY_SIZE - (HEAP_ALIGN - 1U)) return NULL;
    size = (size + HEAP_CANARY_SIZE + HEAP_ALIGN - 1U) &
           ~(size_t)(HEAP_ALIGN - 1U);
    flags = aixos_arch_int_disable();
    if (runtime_locked != 0U && kernel_allocation == 0) {
        alloc_failures++;
        aixos_arch_int_restore(flags);
        return NULL;
    }

    /* Best-fit 搜索 */
    heap_hdr_t *best = NULL;
    size_t best_remain = 0xFFFFFFFF;
    for (hdr = heap_first; hdr; hdr = hdr->next) {
        if (!heap_header_valid(hdr)) {
            corruption_count++;
            aixos_arch_int_restore(flags);
            return NULL;
        }
        if (hdr->magic != HEAP_MAGIC_FREE) continue;
        if ((size_t)hdr->size >= size) {
            size_t remain = hdr->size - size;
            if (remain < best_remain) {
                best = hdr;
                best_remain = remain;
            }
        }
    }
    if (!best) {
        alloc_failures++;
        aixos_arch_int_restore(flags);
        return NULL;
    }

    /* 如果剩余空间足够大, 拆分 */
    if (best_remain > HEAP_HEADER_SIZE + HEAP_ALIGN) {
        new_hdr = (heap_hdr_t *)((char *)best + HEAP_HEADER_SIZE + size);
        new_hdr->magic = HEAP_MAGIC_FREE;
        new_hdr->reserved = 0;
        new_hdr->size  = (uint32_t)(best_remain - HEAP_HEADER_SIZE);
        new_hdr->requested_size = 0U;
        new_hdr->header_guard = 0U;
        new_hdr->next  = best->next;
        new_hdr->prev  = best;
        if (best->next) {
            best->next->prev = new_hdr;
            best->next->header_guard = heap_header_guard(best->next);
        }
        best->next = new_hdr;
        best->size = (uint32_t)size;
        new_hdr->header_guard = heap_header_guard(new_hdr);
    }

    best->magic = HEAP_MAGIC_USED;
    best->requested_size = (uint32_t)requested;
    best->header_guard = heap_header_guard(best);
    heap_write_canary(best);
    alloc_count++;
    current_used += best->size + HEAP_HEADER_SIZE;
    if (current_used > peak_used) {
        peak_used = current_used;
    }
    result = (void *)((char *)best + HEAP_HEADER_SIZE);
    aixos_arch_int_restore(flags);
    memset(result, 0xCC, best->requested_size);
    heap_write_canary(best);
    AIXOS_TRACE(AIXOS_TRACE_MEM_ALLOC, (uint32_t)(uintptr_t)result,
                best->requested_size);
    return result;
}

void *aixos_malloc(size_t size)
{
    return heap_allocate(size, 0);
}

void *aixos_kernel_malloc(size_t size)
{
    return heap_allocate(size, 1);
}

void *aixos_calloc(size_t count, size_t size)
{
    if (size != 0U && count > (size_t)-1 / size) return NULL;
    void *p = aixos_malloc(count * size);
    if (p) memset(p, 0, count * size);
    return p;
}

void *aixos_realloc(void *ptr, size_t new_size)
{
    size_t old_size;
    aixos_arch_flags_t flags;
    if (!ptr) return aixos_malloc(new_size);
    if (new_size == 0) { aixos_free(ptr); return NULL; }
    if (aixos_in_isr()) return NULL;
    if ((uintptr_t)ptr < heap_begin + HEAP_HEADER_SIZE ||
        (uintptr_t)ptr >= heap_end) return NULL;
    heap_hdr_t *hdr = (heap_hdr_t *)((char *)ptr - HEAP_HEADER_SIZE);
    flags = aixos_arch_int_disable();
    if (runtime_locked != 0U) {
        alloc_failures++;
        aixos_arch_int_restore(flags);
        return NULL;
    }
    if (!heap_header_valid(hdr) || hdr->magic != HEAP_MAGIC_USED ||
        !heap_canary_valid(hdr)) {
        corruption_count++;
        aixos_arch_int_restore(flags);
        return NULL;
    }
    old_size = hdr->requested_size;
    aixos_arch_int_restore(flags);
    void *newp = aixos_malloc(new_size);
    if (newp) {
        size_t copy = old_size < new_size ? old_size : new_size;
        memcpy(newp, ptr, copy);
        aixos_free(ptr);
    }
    return newp;
}

void aixos_free(void *ptr)
{
    aixos_arch_flags_t flags;
    if (!ptr) return;
    if (aixos_in_isr()) return;
    if ((uintptr_t)ptr < heap_begin + HEAP_HEADER_SIZE ||
        (uintptr_t)ptr >= heap_end ||
        ((uintptr_t)ptr & (HEAP_ALIGN - 1U)) != 0U) return;
    heap_hdr_t *hdr = (heap_hdr_t *)((char *)ptr - HEAP_HEADER_SIZE);
    flags = aixos_arch_int_disable();
    if ((uintptr_t)hdr < heap_begin ||
        (uintptr_t)hdr + HEAP_HEADER_SIZE > heap_end ||
        !heap_header_valid(hdr) || hdr->magic != HEAP_MAGIC_USED ||
        !heap_canary_valid(hdr)) {
        corruption_count++;
        aixos_arch_int_restore(flags);
        return;
    }
    current_used -= hdr->size + HEAP_HEADER_SIZE;
    hdr->magic = HEAP_MAGIC_FREE;
    hdr->requested_size = 0U;
    free_count++;

    /* Verify adjacent block address continuity before coalescing. */
    if (hdr->next) {
        uintptr_t expected_next = (uintptr_t)hdr + HEAP_HEADER_SIZE + hdr->size;
        if ((uintptr_t)hdr->next != expected_next) {
            corruption_count++;
            aixos_arch_int_restore(flags);
            return;
        }
    }

    /* 合并相邻空闲块 */
    if (hdr->next && hdr->next->magic == HEAP_MAGIC_FREE) {
        hdr->size += HEAP_HEADER_SIZE + hdr->next->size;
        hdr->next = hdr->next->next;
        if (hdr->next) {
            hdr->next->prev = hdr;
            hdr->next->header_guard = heap_header_guard(hdr->next);
        }
    }
    if (hdr->prev && hdr->prev->magic == HEAP_MAGIC_FREE) {
        hdr->prev->size += HEAP_HEADER_SIZE + hdr->size;
        hdr->prev->next = hdr->next;
        if (hdr->next) {
            hdr->next->prev = hdr->prev;
            hdr->next->header_guard = heap_header_guard(hdr->next);
        }
        hdr->prev->header_guard = heap_header_guard(hdr->prev);
    } else {
        hdr->header_guard = heap_header_guard(hdr);
    }
    aixos_arch_int_restore(flags);
    AIXOS_TRACE(AIXOS_TRACE_MEM_FREE, (uint32_t)(uintptr_t)ptr, 0U);
}

void aixos_mem_info(aixos_mem_info_t *info)
{
    heap_hdr_t *hdr;
    uint32_t free_bytes = 0, max_free = 0, free_blocks = 0;
    aixos_arch_flags_t flags;
    if (!info) return;
    flags = aixos_arch_int_disable();
    for (hdr = heap_first; hdr; hdr = hdr->next) {
        if (!heap_header_valid(hdr)) {
            corruption_count++;
            break;
        }
        if (hdr->magic == HEAP_MAGIC_FREE) {
            free_bytes += hdr->size;
            free_blocks++;
            if (hdr->size > max_free) max_free = hdr->size;
        }
    }
    info->total_bytes = heap_total;
    info->free_bytes  = free_bytes;
    info->used_bytes  = heap_total - free_bytes;
    info->max_free_block = max_free;
    info->alloc_count = alloc_count;
    info->free_count  = free_count;
    info->alloc_failures = alloc_failures;
    info->corruption_count = corruption_count;
    info->peak_used_bytes = peak_used;
    info->free_block_count = free_blocks;
    info->fragmentation_per_mille =
        free_bytes == 0U ? 0U :
        (uint32_t)(((uint64_t)(free_bytes - max_free) * 1000U) / free_bytes);
    info->runtime_locked = runtime_locked;
    aixos_arch_int_restore(flags);
}

int aixos_heap_check(void)
{
    heap_hdr_t *hdr;
    heap_hdr_t *previous = NULL;
    aixos_arch_flags_t flags;
    if (!heap_initialized) {
        return AIXOS_ERR_INVAL;
    }
    flags = aixos_arch_int_disable();
    for (hdr = heap_first; hdr != NULL; hdr = hdr->next) {
        if (!heap_header_valid(hdr) || hdr->prev != previous ||
            (hdr->magic == HEAP_MAGIC_USED && !heap_canary_valid(hdr))) {
            corruption_count++;
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_CORRUPT;
        }
        if (hdr->next != NULL &&
            (uintptr_t)hdr + HEAP_HEADER_SIZE + hdr->size !=
            (uintptr_t)hdr->next) {
            corruption_count++;
            aixos_arch_int_restore(flags);
            return AIXOS_ERR_CORRUPT;
        }
        previous = hdr;
    }
    aixos_arch_int_restore(flags);
    return AIXOS_OK;
}

void aixos_heap_lockdown(void)
{
    /* Do not lock the heap from ISR context. */
    if (aixos_in_isr()) return;

    aixos_arch_flags_t flags = aixos_arch_int_disable();
    runtime_locked = 1U;
    aixos_arch_int_restore(flags);
}

int aixos_heap_is_locked(void)
{
    return runtime_locked != 0U;
}
