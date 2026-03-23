#ifndef CVL_SYSVAR_H
#define CVL_SYSVAR_H

#include "types.h"
#include "syscall.h"

typedef struct {
    uint64_t slot;
    int64_t  epoch_start_timestamp;
    uint64_t epoch;
    uint64_t leader_schedule_epoch;
    int64_t  unix_timestamp;
} CvlClock;

/**
 * Read the Clock sysvar into the provided struct.
 * Returns CVL_SUCCESS on success.
 */
static inline uint64_t cvl_get_clock(CvlClock *clock) {
    return sol_get_clock_sysvar(clock);
}

typedef struct {
    uint64_t lamports_per_byte_year;
    uint8_t  exemption_threshold[8]; /* f64 stored as raw */
    uint8_t  burn_percent;
} CvlRent;

/**
 * Read the Rent sysvar into the provided struct.
 * Returns CVL_SUCCESS on success.
 */
static inline uint64_t cvl_get_rent(CvlRent *rent) {
    return sol_get_rent_sysvar(rent);
}

/**
 * Calculate minimum balance for rent exemption.
 * data_len is the size of the account data in bytes.
 *
 * The exemption threshold is always 2.0 years on Solana
 */
static inline uint64_t cvl_minimum_balance(const CvlRent *rent, uint64_t data_len) {
    /* 128 = account metadata overhead */
    uint64_t total_size = 128 + data_len;
    uint64_t cost_per_year = total_size * rent->lamports_per_byte_year;
    /* exemption_threshold is always 2.0 */
    return cost_per_year * 2;
}

#endif /* CVL_SYSVAR_H */
