#ifndef HEAP_H
#define HEAP_H

/**
 * @brief Heap allocator
 *
 * @note Compile opts:
 *   - NO_HEAP       — Exclude this header entirely, no heap allocator
 *   - CUSTOM_HEAP   — Only provide constants; you implement an allocator
 *   - HEAP_SZ     — Override default 32kb
 */

#include "types.h"
#include "syscall.h"

/** Fixed vaddr where the runtime maps the heap */
#define HEAP_START ((uint64_t)0x300000000)

#ifndef HEAP_SZ
#define HEAP_SZ  32768
#endif

#ifndef CUSTOM_HEAP

static uint64_t _heap_pos;

/**
 * tnit the bump allocator - this is called automatically by `ENTRYPOINT`;
 * call manually if using `LAZY_ENTRYPOINT`
 */
static inline void heap_init(void) {
    _heap_pos = HEAP_START;
}

/**
 * alloc `sz` bytes from the heap (8-byte aligned, zero-init)
 *
 * @param sz The number of bytes to allocate
 * @return The pointer to the allocated memory, or NULL if the heap is exhausted
 */
static inline void *alloc(uint64_t sz) {
    /* Align up to 8 bytes */
    sz = (sz + 7) & ~(uint64_t)7;
    if (sz == 0) sz = 8;

    uint64_t pos = _heap_pos;
    uint64_t end = pos + sz;

    if (end > HEAP_START + HEAP_SZ) return NULL;

    _heap_pos = end;
    sol_memset_((void *)pos, 0, sz);
    return (void *)pos;
}

/* noop for bump allocator */
static inline void free(void *ptr) {
    (void)ptr;
}

/**
 * reset the bump allocator, reclaiming all previous allocations
 * 
 * @note All pointers returned by prior alloc() calls become invalid
 */
static inline void heap_reset(void) {
    _heap_pos = HEAP_START;
}

#endif /* CUSTOM_HEAP */

#endif /* HEAP_H */
