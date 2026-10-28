#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

#include "types.h"

/**
 * Unaligned little-endian loads/stores. Dereferencing a cast pointer at an
 * unaligned address is undefined behavior in C; __builtin_memcpy is not, and
 * on SBF it still compiles to a single ldx/stx (verified for v0 and v3).
 */
static inline uint64_t cvl_load_u64(const void *p) {
    uint64_t v;
    __builtin_memcpy(&v, p, sizeof(v));
    return v;
}

static inline void cvl_store_u64(void *p, uint64_t v) {
    __builtin_memcpy(p, &v, sizeof(v));
}

static inline void cvl_store_u32(void *p, uint32_t v) {
    __builtin_memcpy(p, &v, sizeof(v));
}

/**
 * @brief Readers for unsigned integer types -- 
 *        these all assume little-endian repr
 */

#define READ_U8(ptr) (*(ptr)++)

#define READ_U16(ptr) ((uint16_t)(ptr)[0] | ((uint16_t)(ptr)[1] << 8))

#define READ_U32(ptr) \
    ((uint32_t)(ptr)[0] | ((uint32_t)(ptr)[1] << 8) | \
     ((uint32_t)(ptr)[2] << 16) | ((uint32_t)(ptr)[3] << 24))

#define READ_U64(ptr) \
    ({ \
        uint64_t _v = cvl_load_u64(ptr); \
        (ptr) += 8; \
        _v; \
    })

/**
 * Stack-allocated growable array
 *
 * @code
 * Vec(uint64_t, 10) prices = VEC_INIT;
 * vec_push(&prices, 42);
 * vec_push(&prices, 100);
 * uint64_t last = vec_pop(&prices);
 * uint64_t first = prices.data[0];
 */
#define Vec(T, cap) \
    struct { \
        uint32_t len; \
        T data[cap]; \
    }
#define VEC_INIT {.len = 0}

#define vec_cap(v) (sizeof((v)->data) / sizeof((v)->data[0]))

#define vec_len(v)   ((v)->len)
#define vec_full(v)  ((v)->len >= vec_cap(v))
#define vec_clear(v) ((v)->len = 0)

#define vec_push(v, val) ((v)->data[(v)->len++] = (val))

#define vec_pop(v) ((v)->data[--(v)->len])

/**
 * Compare two public keys for equality
 *
 * @param a The first public key
 * @param b The second public key
 * @return True if the public keys are equal, false otherwise
 */
static inline bool pubkey_eq(const Pubkey *a, const Pubkey *b) {
    const uint8_t *pa = a->bytes, *pb = b->bytes;
    return ((cvl_load_u64(pa) ^ cvl_load_u64(pb)) |
            (cvl_load_u64(pa + 8) ^ cvl_load_u64(pb + 8)) |
            (cvl_load_u64(pa + 16) ^ cvl_load_u64(pb + 16)) |
            (cvl_load_u64(pa + 24) ^ cvl_load_u64(pb + 24))) == 0;
}

/**
 * Copy a public key from one location to another
 *
 * @param dst The destination location
 * @param src The source location
 */
static inline void pubkey_cpy(void *dst, const void *src) {
    __builtin_memcpy(dst, src, 32);
}

#endif /* UTIL_H */
