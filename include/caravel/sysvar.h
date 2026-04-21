#ifndef SYSVAR_H
#define SYSVAR_H

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
} Clock;

/**
 * Read the Clock sysvar into the provided struct.
 *
 * @param clock Pointer to a Clock struct to fill
 * @return SUCCESS on success
 *
 * @code GET_CLOCK(clock);
 */
static inline uint64_t get_clock(Clock *clock) {
    return sol_get_clock_sysvar(clock);
}

typedef struct {
    uint64_t lamports_per_byte_year;
    uint8_t  exemption_threshold[8]; /* f64 stored as raw */
    uint8_t  burn_percent;
} Rent;

/**
 * Read the Rent sysvar into the provided struct.
 *
 * @param rent Pointer to a Rent struct to fill
 * @return SUCCESS on success
 *
 * @code GET_RENT(rent);
 */
static inline uint64_t get_rent(Rent *rent) {
    return sol_get_rent_sysvar(rent);
}

/**
 * Calculate minimum balance for rent exemption.
 *
 * @param rent Pointer to a Rent struct to use
 * @param data_len The size of the account data in bytes
 * @return The minimum balance for rent exemption
 *
 * @code MINIMUM_BALANCE(rent, data_len);
 */
static inline uint64_t minimum_balance(const Rent *rent, uint64_t data_len) {
    /* 128 = account metadata overhead */
    uint64_t total_size = 128 + data_len;
    uint64_t cost_per_year = total_size * rent->lamports_per_byte_year;
    /* exemption_threshold is always 2.0 */
    return cost_per_year * 2;
}

typedef struct {
    const uint8_t *data;
    uint64_t       data_len;
} InstructionsSysvar;

/**
 * View of a single instruction
 */
typedef struct {
    const Pubkey *program_id;
    const uint8_t   *accounts_raw;  /* ptr to 1st account_meta */
    uint16_t         accounts_len;
    const uint8_t   *data;
    uint16_t         data_len;
} LoadedInstruction;

/* flag_byte + pubkey */
#define _IX_ACCOUNT_META_SIZE 33

#define ASSERT_INSTRUCTIONS_SYSVAR(acc) \
    do { \
        if (!pubkey_eq((acc)->key, &INSTRUCTIONS_SYSVAR_ID)) { \
            log_literal("err: not instructions sysvar"); \
            return ERROR_INVALID_ARGUMENT; \
        } \
    } while (0)

/**
 * Initialize the instructions sysvar wrapper from an account,
 * validating that the account key matches the Instructions sysvar
 *
 * @param sysvar  Pointer to a InstructionsSysvar to fill
 * @param account The account to read from (must be the Instructions sysvar)
 * @return SUCCESS
 */
static inline uint64_t instructions_sysvar_init(
    InstructionsSysvar *sysvar,
    const AccountInfo *account
) {
    if (!pubkey_eq(account->key, &INSTRUCTIONS_SYSVAR_ID)) {
        return ERROR_INVALID_ARGUMENT;
    }
    sysvar->data     = account->data;
    sysvar->data_len = account->data_len;
    return SUCCESS;
}

static inline uint16_t instructions_count(
    const InstructionsSysvar *sysvar
) {
    return READ_U16(sysvar->data);
}

/**
 * Get the index of the currently executing instruction
 */
static inline uint16_t instructions_current_index(
    const InstructionsSysvar *sysvar
) {
    return READ_U16(sysvar->data + sysvar->data_len - 2);
}

/**
 * Load an instruction by index
 *
 * @param sysvar The instructions sysvar wrapper
 * @param index  Instruction index (0-based)
 * @param ix     Output: populated with pointers into the sysvar data
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT if out of range
 */
static inline uint64_t instructions_get(
    const InstructionsSysvar *sysvar,
    uint16_t index,
    LoadedInstruction *ix
) {
    uint16_t count = READ_U16(sysvar->data);
    if (index >= count) {
        return ERROR_INVALID_ARGUMENT;
    }

    /* Read the offset for this instruction from the offset table */
    uint16_t offset = READ_U16(sysvar->data + 2 + index * 2);
    const uint8_t *ptr = sysvar->data + offset;

    /* num_accounts */
    ix->accounts_len = READ_U16(ptr);
    ptr += 2;

    /* accounts array (33 bytes each: flag_byte + 32-byte pubkey) */
    ix->accounts_raw = ptr;
    ptr += (uint32_t)ix->accounts_len * _IX_ACCOUNT_META_SIZE;

    /* program_id (32 bytes) */
    ix->program_id = (const Pubkey *)ptr;
    ptr += 32;

    /* data */
    ix->data_len = READ_U16(ptr);
    ptr += 2;
    ix->data = ptr;

    return SUCCESS;
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
static inline void loaded_ix_account(
    const LoadedInstruction *ix,
    uint16_t index,
    const Pubkey **pubkey,
    bool *is_signer,
    bool *is_writable
) {
    const uint8_t *meta = ix->accounts_raw + (uint32_t)index * _IX_ACCOUNT_META_SIZE;
    *is_signer   = meta[0];
    *is_writable = meta[1];
    *pubkey      = (const Pubkey *)(meta + 2);
}

/**
 * Check that no other instruction in the transaction invokes the given
 * program
 *
 * @param sysvar     The instructions sysvar
 * @param program_id The program ID to guard against
 * @return SUCCESS, ERROR_INVALID_ARGUMENT
 */
static inline uint64_t assert_no_reentrancy(
    const InstructionsSysvar *sysvar,
    const Pubkey *program_id
) {
    uint16_t count   = instructions_count(sysvar);
    uint16_t current = instructions_current_index(sysvar);

    for (uint16_t i = 0; i < count; i++) {
        if (i == current) continue;

        LoadedInstruction ix;
        uint64_t err = instructions_get(sysvar, i, &ix);
        if (err) return err;

        if (pubkey_eq(ix.program_id, program_id)) {
            log_literal("err: reentrancy detected");
            return ERROR_INVALID_ARGUMENT;
        }
    }
    return SUCCESS;
}

#endif /* SYSVAR_H */
