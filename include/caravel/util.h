#ifndef CVL_UTIL_H
#define CVL_UTIL_H

#include <stdbool.h>

/**
 * @brief Readers for unsigned integer types -- 
 *        these all assume little-endian repr
 */

#define CVL_READ_U8(ptr)  (*(ptr)++)

#define CVL_READ_U16(ptr) \
    ((uint16_t)(ptr)[0] | ((uint16_t)(ptr)[1] << 8))

#define CVL_READ_U32(ptr) \
    ((uint32_t)(ptr)[0] | ((uint32_t)(ptr)[1] << 8) | ((uint32_t)(ptr)[2] << 16) | ((uint32_t)(ptr)[3] << 24))

#define CVL_READ_U64(ptr) ({ \
    uint64_t _v = *(uint64_t *)(ptr); \
    (ptr) += 8; \
    _v; \
})

/**
 * Stack-allocated growable array
 *
 * @code
 * CvlVec(uint64_t, 10) prices = CVL_VEC_INIT;
 * cvl_vec_push(&prices, 42);
 * cvl_vec_push(&prices, 100);
 * uint64_t last = cvl_vec_pop(&prices);
 * uint64_t first = prices.data[0];
 */
 #define CvlVec(T, cap) struct { uint32_t len; T data[cap]; }
 #define CVL_VEC_INIT { .len = 0 }

 #define cvl_vec_cap(v) \
     (sizeof((v)->data) / sizeof((v)->data[0]))
 
 #define cvl_vec_len(v)  ((v)->len)
 #define cvl_vec_full(v) ((v)->len >= cvl_vec_cap(v))
 #define cvl_vec_clear(v) ((v)->len = 0)

 #define cvl_vec_push(v, val) \
     ((v)->data[(v)->len++] = (val))
 
 #define cvl_vec_pop(v) \
     ((v)->data[--(v)->len])

/**
 * Compare two public keys for equality
 *
 * @param a The first public key
 * @param b The second public key
 * @return True if the public keys are equal, false otherwise
 */
static inline bool cvl_pubkey_eq(const CvlPubkey *a, const CvlPubkey *b) {
    const uint64_t *pa = (const uint64_t *)a->bytes;
    const uint64_t *pb = (const uint64_t *)b->bytes;
    return pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2] && pa[3] == pb[3];
}

/**
 * Copy a public key from one location to another
 *
 * @param dst The destination location
 * @param src The source location
 */
static inline void cvl_pubkey_cpy(void *dst, const void *src) {
    const uint64_t *s = (const uint64_t *)src;
    uint64_t *d = (uint64_t *)dst;
    d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
}

#endif /* CVL_UTIL_H */