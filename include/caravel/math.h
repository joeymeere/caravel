#ifndef CVL_MATH_H
#define CVL_MATH_H

#include "types.h"
#include "error.h"

#define CVL_U64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFF)

static inline uint64_t cvl_checked_add_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (a > CVL_U64_MAX - b) {
        return CVL_ERROR_INVALID_ARGUMENT;
    }
    *result = a + b;
    return CVL_SUCCESS;
}

static inline uint64_t cvl_checked_sub_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (a < b) {
        return CVL_ERROR_INSUFFICIENT_FUNDS;
    }
    *result = a - b;
    return CVL_SUCCESS;
}

static inline uint64_t cvl_checked_mul_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (b != 0 && a > CVL_U64_MAX / b) {
        return CVL_ERROR_INVALID_ARGUMENT;
    }
    *result = a * b;
    return CVL_SUCCESS;
}

static inline uint64_t cvl_saturating_add_u64(uint64_t a, uint64_t b) {
    uint64_t result = a + b;
    if (result < a) return CVL_U64_MAX;
    return result;
}

static inline uint64_t cvl_saturating_sub_u64(uint64_t a, uint64_t b) {
    if (a < b) return 0;
    return a - b;
}

static inline uint64_t cvl_min_u64(uint64_t a, uint64_t b) {
    return (a < b) ? a : b;
}

static inline uint64_t cvl_max_u64(uint64_t a, uint64_t b) {
    return (a > b) ? a : b;
}

#endif /* CVL_MATH_H */
