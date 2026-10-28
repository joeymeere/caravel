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
 * @code get_clock(&clock);
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
 * @code get_rent(&rent);
 */
static inline uint64_t get_rent(Rent *rent) {
    return sol_get_rent_sysvar(rent);
}

/** per-account metadata overhead the runtime charges rent for */
#define ACCOUNT_STORAGE_OVERHEAD 128

/*
 * Round the 128-bit integer hi:lo to 53 significant bits (round half to
 * even), as an f64 conversion would. Returns the significand q and writes
 * the binary exponent so that value ~= q * 2^(*e).
 */
static inline uint64_t _cvl_round53(uint64_t hi, uint64_t lo, int64_t *e) {
    int64_t bits = 0;
    if (hi) { uint64_t t = hi; bits = 64; while (t) { t >>= 1; bits++; } }
    else    { uint64_t t = lo;            while (t) { t >>= 1; bits++; } }

    if (bits <= 53) { *e = 0; return lo; }

    int64_t drop = bits - 53;
    uint64_t q, rem_hi_bit, rem_rest;
    if (drop < 64) {
        q = (lo >> drop) | (hi << (64 - drop));
        rem_hi_bit = (lo >> (drop - 1)) & 1;
        rem_rest   = (drop > 1) ? (lo & ((1ULL << (drop - 1)) - 1)) : 0;
    } else {
        q = hi >> (drop - 64);
        rem_hi_bit = (drop == 64) ? (lo >> 63) : ((hi >> (drop - 65)) & 1);
        rem_rest   = (drop == 64)
            ? (lo & 0x7FFFFFFFFFFFFFFFULL)
            : (lo | (hi & ((1ULL << (drop - 65)) - 1)));
    }
    if (rem_hi_bit && (rem_rest || (q & 1))) q++;
    *e = drop;
    return q;
}

/* q * 2^e as u64, saturating (equiv. f64 as u64) */
static inline uint64_t _cvl_ldexp_u64(uint64_t q, int64_t e) {
    if (e >= 0) {
        if (e >= 64 || q > (UINT64_MAX >> e)) return UINT64_MAX;
        return q << e;
    }
    return (-e >= 64) ? 0 : q >> -e;
}

/**
 * Calculate minimum balance for rent exemption.
 *
 * @param rent Pointer to a Rent struct filled by get_rent()
 * @param data_len The size of the account data in bytes
 * @return The minimum balance for rent exemption
 *
 * @code
 *   Rent rent;
 *   get_rent(&rent);
 *   uint64_t lamports = minimum_balance(&rent, sizeof(MyState));
 */
static inline uint64_t minimum_balance(const Rent *rent, uint64_t data_len) {
    uint64_t cost = (ACCOUNT_STORAGE_OVERHEAD + data_len)
                  * rent->lamports_per_byte_year;

    /* IEEE-754 f64 threshold ~ value = mant * 2^(exp - 1075) */
    uint64_t bits;
    __builtin_memcpy(&bits, rent->exemption_threshold, sizeof(bits));

    int64_t  exp  = (int64_t)((bits >> 52) & 0x7FF);
    uint64_t frac = bits & 0xFFFFFFFFFFFFFULL;

    if ((bits >> 63) || exp == 0) return 0;          /* <= 0 or subnormal */
    if (exp == 0x7FF) return UINT64_MAX;            /* inf / nan */

    /* cost as f64 */
    int64_t ce = 0;
    uint64_t cq = cost;
    if (UNLIKELY(cost >> 53))
        cq = _cvl_round53(0, cost, &ce);

    /* threshold is a power of two (1.0, 2.0, ...) */
    if (frac == 0)
        return _cvl_ldexp_u64(cq, ce + exp - 1023);

    /* round(cq * mant) * 2^(ce + exp - 1075) */
    uint64_t mant = frac | (1ULL << 52);
    uint64_t a_lo = cq & 0xFFFFFFFF, a_hi = cq >> 32;
    uint64_t b_lo = mant & 0xFFFFFFFF, b_hi = mant >> 32;
    uint64_t ll = a_lo * b_lo, lh = a_lo * b_hi;
    uint64_t hl = a_hi * b_lo, hh = a_hi * b_hi;
    uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFF) + (hl & 0xFFFFFFFF);
    uint64_t lo  = (ll & 0xFFFFFFFF) | (mid << 32);
    uint64_t hi  = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);

    int64_t pe;
    uint64_t pq = _cvl_round53(hi, lo, &pe);
    return _cvl_ldexp_u64(pq, pe + ce + exp - 1075);
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
