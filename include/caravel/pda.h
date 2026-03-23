#ifndef CVL_PDA_H
#define CVL_PDA_H

#include "types.h"
#include "error.h"
#include "syscall.h"
#include "log.h"

/**
 * Create a seed from a string literal.
 * Usage: CvlSignerSeed seed = CVL_SEED_STR("vault");
 */
#define CVL_SEED_STR(s) \
    ((CvlSignerSeed){ .addr = (const uint8_t *)(s), .len = sizeof(s) - 1 })

/**
 * Create a seed from a pubkey.
 * Usage: CvlSignerSeed seed = CVL_SEED_PUBKEY(authority_key);
 */
#define CVL_SEED_PUBKEY(pubkey_ptr) \
    ((CvlSignerSeed){ .addr = (pubkey_ptr)->bytes, .len = 32 })

/**
 * Create a seed from a single byte (typically the bump seed).
 * Requires a backing variable for the byte.
 * Usage:
 *   uint8_t bump = 254;
 *   CvlSignerSeed seed = CVL_SEED_U8(&bump);
 */
#define CVL_SEED_U8(byte_ptr) \
    ((CvlSignerSeed){ .addr = (const uint8_t *)(byte_ptr), .len = 1 })

/**
 * Create a seed from a raw byte buffer.
 */
#define CVL_SEED_BYTES(buf, length) \
    ((CvlSignerSeed){ .addr = (const uint8_t *)(buf), .len = (length) })

/**
 * Find a program address (PDA) and its bump seed.
 * Returns CVL_SUCCESS on success.
 */
static inline uint64_t cvl_find_program_address(
    const CvlSignerSeed *seeds,
    int seeds_len,
    const CvlPubkey *program_id,
    CvlPubkey *address,
    uint8_t *bump
) {
    return sol_try_find_program_address(seeds, seeds_len, program_id, address, bump);
}

/**
 * Create a program address from seeds (including bump).
 * Returns CVL_SUCCESS on success, error if seeds produce a point on the curve.
 */
static inline uint64_t cvl_create_program_address(
    const CvlSignerSeed *seeds,
    int seeds_len,
    const CvlPubkey *program_id,
    CvlPubkey *address
) {
    return sol_create_program_address(seeds, seeds_len, program_id, address);
}

/**
 * Assert that an account's key matches the expected PDA.
 * Derives the PDA from seeds + program_id and compares.
 *
 * Usage:
 *   CvlSignerSeed seeds[] = { CVL_SEED_STR("vault"), CVL_SEED_PUBKEY(auth_key) };
 *   CVL_ASSERT_PDA(vault_acc, seeds, 2, program_id, &bump);
 */
#define CVL_ASSERT_PDA(acc, seeds, seeds_len, program_id, bump_out) \
    do { \
        CvlPubkey _cvl_expected; \
        uint64_t _cvl_pda_err = cvl_find_program_address( \
            (seeds), (seeds_len), (program_id), &_cvl_expected, (bump_out)); \
        if (_cvl_pda_err) { \
            cvl_log_literal("Error: PDA derivation failed"); \
            return CVL_ERROR_INVALID_PDA; \
        } \
        if (!cvl_pubkey_equals((acc)->key, &_cvl_expected)) { \
            cvl_log_literal("Error: account key does not match PDA"); \
            return CVL_ERROR_INVALID_PDA; \
        } \
    } while (0)

#endif /* CVL_PDA_H */
