#ifndef AMM_MATH_H
#define AMM_MATH_H

#include <caravel.h>

static inline void mul_wide(uint64_t a, uint64_t b,
                            uint64_t *hi, uint64_t *lo) {
    uint64_t a_lo = a & 0xFFFFFFFF;
    uint64_t a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFF;
    uint64_t b_hi = b >> 32;

    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_hi * b_lo;
    uint64_t p2 = a_lo * b_hi;
    uint64_t p3 = a_hi * b_hi;

    uint64_t mid = (p0 >> 32) + (p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF);
    *lo = (p0 & 0xFFFFFFFF) | (mid << 32);
    *hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
}

extern uint64_t div_wide(uint64_t hi, uint64_t lo, uint64_t d);
extern uint64_t mul_div_u64(uint64_t a, uint64_t b, uint64_t c);

static inline uint64_t isqrt_wide(uint64_t hi, uint64_t lo) {
    if (hi == 0 && lo == 0) return 0;

    if (hi == 0) {
        uint64_t b = 0;
        uint64_t t = lo;
        while (t > 0) { t >>= 1; b++; }
        uint64_t x = (uint64_t)1 << ((b + 1) >> 1);

        for (;;) {
            uint64_t y = (x + lo / x) >> 1;
            if (y >= x) break;
            x = y;
        }
        return x;
    }

    uint64_t bits = 64;
    uint64_t t = hi;
    while (t > 0) { t >>= 1; bits++; }

    uint64_t x = (uint64_t)1 << (((bits - 1) >> 1) + 1);

    for (uint64_t i = 0; i < 128; i++) {
        uint64_t q = div_wide(hi, lo, x);
        uint64_t nx = (x >> 1) + (q >> 1) + (x & q & 1);
        if (nx >= x) break;
        x = nx;
    }
    return x;
}

/**
 * Compute isqrt(a * b)
 */
static inline uint64_t isqrt_mul(uint64_t a, uint64_t b) {
    uint64_t hi, lo;
    mul_wide(a, b, &hi, &lo);
    return isqrt_wide(hi, lo);
}

#endif /* AMM_MATH_H */
