#ifndef CVL_PDA_H
#define CVL_PDA_H

#include "types.h"
#include "error.h"
#include "syscall.h"
#include "log.h"

/**
 * Create a seed from a string literal.
 *
 * @param s The string to use as a seed
 * @return A CvlSignerSeed struct
 *
 * @code
 * CvlSignerSeed seed = CVL_SEED_STR("vault");  
 */
#define CVL_SEED_STR(s) \
    ((CvlSignerSeed){ .addr = (const uint8_t *)(s), .len = sizeof(s) - 1 })

/**
 * Create a seed from a pubkey.
 *
 * @param pubkey_ptr The public key to use as a seed
 * @return A CvlSignerSeed struct
 *
 * @code
 * CvlSignerSeed seed = CVL_SEED_PUBKEY(authority_key);
 */
#define CVL_SEED_PUBKEY(pubkey_ptr) \
    ((CvlSignerSeed){ .addr = (pubkey_ptr)->bytes, .len = 32 })

/**
 * Create a seed from a single byte (typically the bump seed).
 * Requires a backing variable for the byte.
 *
 * @code
 * uint8_t bump = 254;
 * CvlSignerSeed seed = CVL_SEED_U8(&bump);
 */
#define CVL_SEED_U8(byte_ptr) \
    ((CvlSignerSeed){ .addr = (const uint8_t *)(byte_ptr), .len = 1 })

/**
 * Create a seed from a raw byte buffer.
 *
 * @param buf The byte buffer to use as a seed
 * @param length The length of the byte buffer
 * @return A CvlSignerSeed struct
 *
 * @code
 * CvlSignerSeed seed = CVL_SEED_BYTES(buf, length);
 */
#define CVL_SEED_BYTES(buf, length) \
    ((CvlSignerSeed){ .addr = (const uint8_t *)(buf), .len = (length) })

/**
 * Find a program address (PDA) and its bump seed.
 *
 * @param seeds The seeds to use to derive the PDA
 * @param seeds_len The length of the seeds array
 * @param program_id The program id to use to derive the PDA
 * @param address The address to fill with the derived PDA
 * @param bump The bump seed to fill with the derived PDA
 * @return CVL_SUCCESS on success, CVL_ERROR_INVALID_PDA on failure
 *
 * @code
 * CvlSignerSeed seeds[] = { CVL_SEED_STR("vault"), CVL_SEED_PUBKEY(auth_key) };
 * CvlPubkey address;
 * uint8_t bump;
 * CVL_ASSERT_PDA(vault_acc, seeds, 2, program_id, &bump);
 */
static inline uint64_t cvl_find_program_address(
    const CvlSignerSeed *seeds,
    int seeds_len,
    const CvlPubkey *program_id,
    CvlPubkey *address,
    uint8_t *bump
) {
    static const uint8_t pda_marker[] = "ProgramDerivedAddress";

    CvlSignerSeed parts[19]; /* max 16 seeds + bump + program_id + marker */
    int i;
    for (i = 0; i < seeds_len; i++)
        parts[i] = seeds[i];

    uint8_t bump_byte = 255;
    int bump_idx   = i++;
    int pgm_idx    = i++;
    int marker_idx = i++;

    parts[bump_idx]   = (CvlSignerSeed){ .addr = &bump_byte,          .len = 1  };
    parts[pgm_idx]    = (CvlSignerSeed){ .addr = program_id->bytes,   .len = 32 };
    parts[marker_idx] = (CvlSignerSeed){ .addr = pda_marker,          .len = 21 };

    for (;;) {
        sol_sha256(parts, (uint64_t)i, address->bytes);

        /* curve_id 0 = Ed25519; returns 0 if on curve */
        if (sol_curve_validate_point(0, address->bytes, 0) != 0) {
            *bump = bump_byte;
            return CVL_SUCCESS;
        }

        if (bump_byte == 0)
            break;
        bump_byte--;
    }

    return CVL_ERROR_INVALID_PDA;
}

/**
 * Create a program address from seeds (including bump).
 *
 * @param seeds The seeds to use to derive the PDA
 * @param seeds_len The length of the seeds array
 * @param program_id The program id to use to derive the PDA
 * @param address The address to fill with the derived PDA
 * @return CVL_SUCCESS on success, error if seeds produce a point on the curve
 *
 * @code
 * CvlSignerSeed seeds[] = { CVL_SEED_STR("vault"), CVL_SEED_PUBKEY(auth_key) };
 * CvlPubkey address;
 * CVL_CREATE_PDA(vault_acc, seeds, 2, program_id, &address);
 * if (error) {
 *     cvl_log_literal("Error: PDA creation failed");
 *     return error;
 * }
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
 * Derive a PDA via a single pass of sha256 with known valid inputs.
 * 
 * @note User must ensure that the seeds + bump result in a valid address
 *
 * @param seeds The seeds (including bump) to hash
 * @param seeds_len The number of seeds (max 16)
 * @param program_id The program id
 * @param address the derived address
 */
static inline void cvl_derive_address(
    const CvlSignerSeed *seeds,
    int seeds_len,
    const CvlPubkey *program_id,
    CvlPubkey *address
) {
    CvlSignerSeed parts[18];

    int i;
    for (i = 0; i < seeds_len; i++) {
        parts[i] = seeds[i];
    }

    static const uint8_t pda_marker[] = "ProgramDerivedAddress";

    parts[i++] = (CvlSignerSeed){ .addr = program_id->bytes, .len = 32 };
    parts[i++] = (CvlSignerSeed){ .addr = pda_marker,        .len = 21 };

    sol_sha256(parts, (uint64_t)i, address->bytes);
}

/**
 * Assert that an account's key matches the expected PDA.
 * Derives the PDA from seeds + program_id and compares.
 *
 * @param acc The account to check
 * @param seeds The seeds to use to derive the PDA
 * @param seeds_len The length of the seeds array
 * @param program_id The program id to use to derive the PDA
 * @param bump_out The bump seed to use to derive the PDA
 * @return CVL_SUCCESS on success, CVL_ERROR_INVALID_PDA on failure
 *
 * @code
 *   CvlSignerSeed seeds[] = { CVL_SEED_STR("vault"), CVL_SEED_PUBKEY(auth_key) };
 *   CVL_ASSERT_PDA(vault_acc, seeds, 2, program_id, &bump);
 */
#define CVL_ASSERT_PDA(acc, seeds, seeds_len, program_id, bump_out) \
    do { \
        CvlPubkey _cvl_expected; \
        uint64_t _cvl_pda_err = cvl_find_program_address( \
            (seeds), (seeds_len), (program_id), &_cvl_expected, (bump_out)); \
        if (_cvl_pda_err) { \
            cvl_log_literal("err: PDA derivation failed"); \
            return CVL_ERROR_INVALID_PDA; \
        } \
        if (!cvl_pubkey_eq((acc)->key, &_cvl_expected)) { \
            cvl_log_literal("err: account key does not match PDA"); \
            return CVL_ERROR_INVALID_PDA; \
        } \
    } while (0)

#endif /* CVL_PDA_H */
