#ifndef CVL_LOG_H
#define CVL_LOG_H

#include "syscall.h"

static inline uint64_t cvl_strlen(const char *s) {
    uint64_t len = 0;
    while (s[len]) len++;
    return len;
}

/**
 * Log a string message. Computes length at runtime.
 *
 * @param msg The string to log
 */
static inline void cvl_log(const char *msg) {
    sol_log_(msg, cvl_strlen(msg));
}

/**
 * Log a string literal. Length computed at compile time.
 *
 * @param msg The string to log
 *
 * @code cvl_log_literal("Hello from Caravel!");
 */
#define cvl_log_literal(msg) sol_log_((msg), sizeof(msg) - 1)

/**
 * Log up to 5 uint64 values (useful for debugging numbers).
 *
 * @param a The first uint64 value to log
 * @param b The second uint64 value to log
 * @param c The third uint64 value to log
 * @param d The fourth uint64 value to log
 * @param e The fifth uint64 value to log
 */
static inline void cvl_log_64(uint64_t a, uint64_t b, uint64_t c,
                               uint64_t d, uint64_t e) {
    sol_log_64_(a, b, c, d, e);
}

/**
 * Log a single u64 value.
 *
 * @param val The uint64 value to log
 */
static inline void cvl_log_u64(uint64_t val) {
    sol_log_64_(0, 0, 0, 0, val);
}

/**
 * Log a public key (base58 in validator logs).
 *
 * @param key The public key to log
 */
static inline void cvl_log_pubkey(const CvlPubkey *key) {
    sol_log_pubkey(key);
}

/**
 * Log remaining compute units.
 */
static inline void cvl_log_compute_units(void) {
    sol_log_compute_units_();
}

#ifdef CVL_DEBUG
    #define cvl_debug(msg)          cvl_log_literal(msg)
    #define cvl_debug_u64(val)      cvl_log_u64(val)
    #define cvl_debug_pubkey(key)   cvl_log_pubkey(key)
#else
    #define cvl_debug(msg)          ((void)0)
    #define cvl_debug_u64(val)      ((void)0)
    #define cvl_debug_pubkey(key)   ((void)0)
#endif

#endif /* CVL_LOG_H */
