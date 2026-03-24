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
 *
 * @param clock Pointer to a CvlClock struct to fill
 * @return CVL_SUCCESS on success
 *
 * @usage: CVL_GET_CLOCK(clock);
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
 *
 * @param rent Pointer to a CvlRent struct to fill
 * @return CVL_SUCCESS on success
 *
 * @usage: CVL_GET_RENT(rent);
 */
static inline uint64_t cvl_get_rent(CvlRent *rent) {
    return sol_get_rent_sysvar(rent);
}

/**
 * Calculate minimum balance for rent exemption.
 *
 * @param rent Pointer to a CvlRent struct to use
 * @param data_len The size of the account data in bytes
 * @return The minimum balance for rent exemption
 *
 * @usage: CVL_MINIMUM_BALANCE(rent, data_len);
 */
static inline uint64_t cvl_minimum_balance(const CvlRent *rent, uint64_t data_len) {
    /* 128 = account metadata overhead */
    uint64_t total_size = 128 + data_len;
    uint64_t cost_per_year = total_size * rent->lamports_per_byte_year;
    /* exemption_threshold is always 2.0 */
    return cost_per_year * 2;
}

#endif /* CVL_SYSVAR_H */
