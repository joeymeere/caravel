#ifndef CVL_SYSVAR_H
#define CVL_SYSVAR_H

#include "types.h"
#include "error.h"
#include "log.h"
#include "syscall.h"
#include "util.h"

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
 * @code CVL_GET_CLOCK(clock);
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
 * @code CVL_GET_RENT(rent);
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
 * @code CVL_MINIMUM_BALANCE(rent, data_len);
 */
static inline uint64_t cvl_minimum_balance(const CvlRent *rent, uint64_t data_len) {
    /* 128 = account metadata overhead */
    uint64_t total_size = 128 + data_len;
    uint64_t cost_per_year = total_size * rent->lamports_per_byte_year;
    /* exemption_threshold is always 2.0 */
    return cost_per_year * 2;
}

typedef struct {
    const uint8_t *data;
    uint64_t       data_len;
} CvlInstructionsSysvar;

/**
 * View of a single instruction
 */
typedef struct {
    const CvlPubkey *program_id;
    const uint8_t   *accounts_raw;  /* ptr to 1st account_meta */
    uint16_t         accounts_len;
    const uint8_t   *data;
    uint16_t         data_len;
} CvlLoadedInstruction;

/* is_signer + is_writable + pubkey */
#define _CVL_IX_ACCOUNT_META_SIZE 34

#define CVL_ASSERT_INSTRUCTIONS_SYSVAR(acc) \
    do { \
        if (!cvl_pubkey_eq((acc)->key, &CVL_INSTRUCTIONS_SYSVAR_ID)) { \
            cvl_log_literal("err: not instructions sysvar"); \
            return CVL_ERROR_INVALID_ARGUMENT; \
        } \
    } while (0)

/**
 * Initialize the instructions sysvar wrapper from an account,
 * validating that the account key matches the Instructions sysvar
 *
 * @param sysvar  Pointer to a CvlInstructionsSysvar to fill
 * @param account The account to read from (must be the Instructions sysvar)
 * @return CVL_SUCCESS
 */
static inline uint64_t cvl_instructions_sysvar_init(
    CvlInstructionsSysvar *sysvar,
    const CvlAccountInfo *account
) {
    if (!cvl_pubkey_eq(account->key, &CVL_INSTRUCTIONS_SYSVAR_ID)) {
        return CVL_ERROR_INVALID_ARGUMENT;
    }
    sysvar->data     = account->data;
    sysvar->data_len = account->data_len;
    return CVL_SUCCESS;
}

static inline uint16_t cvl_instructions_count(
    const CvlInstructionsSysvar *sysvar
) {
    return CVL_READ_U16(sysvar->data);
}

/**
 * Get the index of the currently executing instruction
 */
static inline uint16_t cvl_instructions_current_index(
    const CvlInstructionsSysvar *sysvar
) {
    return CVL_READ_U16(sysvar->data + sysvar->data_len - 2);
}

/**
 * Load an instruction by index
 *
 * @param sysvar The instructions sysvar wrapper
 * @param index  Instruction index (0-based)
 * @param ix     Output: populated with pointers into the sysvar data
 * @return CVL_SUCCESS on success, CVL_ERROR_INVALID_ARGUMENT if out of range
 */
static inline uint64_t cvl_instructions_get(
    const CvlInstructionsSysvar *sysvar,
    uint16_t index,
    CvlLoadedInstruction *ix
) {
    uint16_t count = CVL_READ_U16(sysvar->data);
    if (index >= count) {
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    /* Read the offset for this instruction from the offset table */
    uint16_t offset = CVL_READ_U16(sysvar->data + 2 + index * 2);
    const uint8_t *ptr = sysvar->data + offset;

    /* num_accounts */
    ix->accounts_len = CVL_READ_U16(ptr);
    ptr += 2;

    /* accounts array (34 bytes each: is_signer + is_writable + 32-byte pubkey) */
    ix->accounts_raw = ptr;
    ptr += (uint32_t)ix->accounts_len * _CVL_IX_ACCOUNT_META_SIZE;

    /* program_id (32 bytes) */
    ix->program_id = (const CvlPubkey *)ptr;
    ptr += 32;

    /* data */
    ix->data_len = CVL_READ_U16(ptr);
    ptr += 2;
    ix->data = ptr;

    return CVL_SUCCESS;
}

/**
 * Read an account meta from a loaded instruction
 *
 * @param ix       A previously loaded instruction
 * @param index    Account index within the instruction
 * @param pubkey   Output: pointer to the 32-byte pubkey (into sysvar data)
 * @param is_signer   Output: whether the account is a signer
 * @param is_writable Output: whether the account is writable
 */
static inline void cvl_loaded_ix_account(
    const CvlLoadedInstruction *ix,
    uint16_t index,
    const CvlPubkey **pubkey,
    bool *is_signer,
    bool *is_writable
) {
    const uint8_t *meta = ix->accounts_raw + (uint32_t)index * _CVL_IX_ACCOUNT_META_SIZE;
    *is_signer   = meta[0];
    *is_writable = meta[1];
    *pubkey      = (const CvlPubkey *)(meta + 2);
}

/**
 * Check that no other instruction in the transaction invokes the given
 * program
 *
 * @param sysvar     The instructions sysvar
 * @param program_id The program ID to guard against
 * @return CVL_SUCCESS, CVL_ERROR_INVALID_ARGUMENT
 */
static inline uint64_t cvl_assert_no_reentrancy(
    const CvlInstructionsSysvar *sysvar,
    const CvlPubkey *program_id
) {
    uint16_t count   = cvl_instructions_count(sysvar);
    uint16_t current = cvl_instructions_current_index(sysvar);

    for (uint16_t i = 0; i < count; i++) {
        if (i == current) continue;

        CvlLoadedInstruction ix;
        uint64_t err = cvl_instructions_get(sysvar, i, &ix);
        if (err) return err;

        if (cvl_pubkey_eq(ix.program_id, program_id)) {
            cvl_log_literal("err: reentrancy detected");
            return CVL_ERROR_INVALID_ARGUMENT;
        }
    }
    return CVL_SUCCESS;
}

#endif /* CVL_SYSVAR_H */
