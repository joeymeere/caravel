#ifndef MATH_H
#define MATH_H

#include "types.h"
#include "error.h"

#define U64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFF)

static inline uint64_t checked_add_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (a > U64_MAX - b) {
        return ERROR_INVALID_ARGUMENT;
    }
    *result = a + b;
    return SUCCESS;
}

static inline uint64_t checked_sub_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (a < b) {
        return ERROR_INSUFFICIENT_FUNDS;
    }
    *result = a - b;
    return SUCCESS;
}

static inline uint64_t checked_mul_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (b != 0 && a > U64_MAX / b) {
        return ERROR_INVALID_ARGUMENT;
    }
    *result = a * b;
    return SUCCESS;
}

static inline uint64_t saturating_add_u64(uint64_t a, uint64_t b) {
    uint64_t result = a + b;
    if (result < a) return U64_MAX;
    return result;
}

static inline uint64_t saturating_sub_u64(uint64_t a, uint64_t b) {
    if (a < b) return 0;
    return a - b;
}

static inline uint64_t min_u64(uint64_t a, uint64_t b) {
    return (a < b) ? a : b;
}

static inline uint64_t max_u64(uint64_t a, uint64_t b) {
    return (a > b) ? a : b;
}

#endif /* MATH_H */
