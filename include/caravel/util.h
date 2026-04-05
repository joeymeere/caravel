#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

/**
 * @brief Readers for unsigned integer types -- 
 *        these all assume little-endian repr
 */

#define READ_U8(ptr)  (*(ptr)++)

#define READ_U16(ptr) \
    ((uint16_t)(ptr)[0] | ((uint16_t)(ptr)[1] << 8))

#define READ_U32(ptr) \
    ((uint32_t)(ptr)[0] | ((uint32_t)(ptr)[1] << 8) | ((uint32_t)(ptr)[2] << 16) | ((uint32_t)(ptr)[3] << 24))

#define READ_U64(ptr) ({ \
    uint64_t _v = *(uint64_t *)(ptr); \
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
 #define Vec(T, cap) struct { uint32_t len; T data[cap]; }
 #define VEC_INIT { .len = 0 }

 #define vec_cap(v) \
     (sizeof((v)->data) / sizeof((v)->data[0]))
 
 #define vec_len(v)  ((v)->len)
 #define vec_full(v) ((v)->len >= vec_cap(v))
 #define vec_clear(v) ((v)->len = 0)

 #define vec_push(v, val) \
     ((v)->data[(v)->len++] = (val))
 
 #define vec_pop(v) \
     ((v)->data[--(v)->len])

/**
 * Compare two public keys for equality
 *
 * @param a The first public key
 * @param b The second public key
 * @return True if the public keys are equal, false otherwise
 */
static inline bool pubkey_eq(const Pubkey *a, const Pubkey *b) {
    const uint64_t *pa = (const uint64_t *)a->bytes;
    const uint64_t *pb = (const uint64_t *)b->bytes;                                                                       
    return ((pa[0] ^ pb[0]) | (pa[1] ^ pb[1]) | (pa[2] ^ pb[2]) | (pa[3] ^ pb[3])) == 0;  
}

/**
 * Copy a public key from one location to another
 *
 * @param dst The destination location
 * @param src The source location
 */
static inline void pubkey_cpy(void *dst, const void *src) {
    const uint64_t *s = (const uint64_t *)src;
    uint64_t *d = (uint64_t *)dst;
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
}

#endif /* UTIL_H */