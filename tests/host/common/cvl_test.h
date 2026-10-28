#ifndef CVL_TEST_H
#define CVL_TEST_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cvl_t_checks;
static int cvl_t_fails;
static const char *cvl_t_current;

#define CHECK(cond) \
    do { \
        cvl_t_checks++; \
        if (!(cond)) { \
            cvl_t_fails++; \
            fprintf( \
                stderr, "  FAIL %s (%s:%d): %s\n", cvl_t_current, __FILE__, \
                __LINE__, #cond \
            ); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do { \
        uint64_t _a = (uint64_t)(a), _b = (uint64_t)(b); \
        cvl_t_checks++; \
        if (_a != _b) { \
            cvl_t_fails++; \
            fprintf( \
                stderr, \
                "  FAIL %s (%s:%d): %s == %s\n" \
                "       got 0x%llx, want 0x%llx\n", \
                cvl_t_current, __FILE__, __LINE__, #a, #b, \
                (unsigned long long)_a, (unsigned long long)_b \
            ); \
        } \
    } while (0)

#define CHECK_MEM(a, b, n) \
    do { \
        cvl_t_checks++; \
        if (memcmp((a), (b), (n)) != 0) { \
            cvl_t_fails++; \
            fprintf( \
                stderr, "  FAIL %s (%s:%d): memcmp(%s, %s, %s)\n", \
                cvl_t_current, __FILE__, __LINE__, #a, #b, #n \
            ); \
        } \
    } while (0)

#define TEST(name) static void name(void)

#define RUN(name) \
    do { \
        cvl_t_current = #name; \
        name(); \
    } while (0)

#define TEST_REPORT() \
    do { \
        fprintf( \
            stderr, "%-28s %4d checks, %d failed\n", __FILE__, cvl_t_checks, \
            cvl_t_fails \
        ); \
        return cvl_t_fails ? 1 : 0; \
    } while (0)

/* splitmix64 prng */
static uint64_t cvl_t_rng = 0x9E3779B97F4A7C15ULL;
static inline uint64_t cvl_t_rand(void) {
    uint64_t z = (cvl_t_rng += 0x9E3779B97F4A7C15ULL);
    z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z          = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

#endif /* CVL_TEST_H */
