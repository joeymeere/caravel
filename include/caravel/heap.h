#ifndef CVL_HEAP_H
#define CVL_HEAP_H

/**
 * @brief Heap allocator
 *
 * @note Compile opts:
 *   - CVL_NO_HEAP       — Exclude this header entirely, no heap allocator
 *   - CVL_CUSTOM_HEAP   — Only provide constants; you implement an allocator
 *   - CVL_HEAP_SZ     — Override default 32kb
 */

#include "types.h"
#include "syscall.h"

/** Fixed vaddr where the runtime maps the heap */
#define CVL_HEAP_START ((uint64_t)0x300000000)

#ifndef CVL_HEAP_SZ
#define CVL_HEAP_SZ  32768
#endif

#ifndef CVL_CUSTOM_HEAP

static uint64_t _cvl_heap_pos;

/**
 * tnit the bump allocator - this is called automatically by `CVL_ENTRYPOINT`;
 * call manually if using `CVL_LAZY_ENTRYPOINT`
 */
static inline void cvl_heap_init(void) {
    _cvl_heap_pos = CVL_HEAP_START;
}

/**
 * alloc `sz` bytes from the heap (8-byte aligned, zero-init)
 *
 * @param sz The number of bytes to allocate
 * @return The pointer to the allocated memory, or NULL if the heap is exhausted
 */
static inline void *cvl_alloc(uint64_t sz) {
    /* Align up to 8 bytes */
    sz = (sz + 7) & ~(uint64_t)7;
    if (sz == 0) sz = 8;

    uint64_t pos = _cvl_heap_pos;
    uint64_t end = pos + sz;

    if (end > CVL_HEAP_START + CVL_HEAP_SZ) return NULL;

    _cvl_heap_pos = end;
    sol_memset_((void *)pos, 0, sz);
    return (void *)pos;
}

/* noop for bump allocator */
static inline void cvl_free(void *ptr) {
    (void)ptr;
}

/**
 * reset the bump allocator, reclaiming all previous allocations
 * 
 * @note All pointers returned by prior cvl_alloc() calls become invalid
 */
static inline void cvl_heap_reset(void) {
    _cvl_heap_pos = CVL_HEAP_START;
}

#endif /* CVL_CUSTOM_HEAP */

#endif /* CVL_HEAP_H */
