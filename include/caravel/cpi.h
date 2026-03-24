#ifndef CVL_CPI_H
#define CVL_CPI_H

#include "types.h"
#include "error.h"
#include "syscall.h"

/**
 * Invoke a cross-program instruction (no signer seeds).
 */
static inline uint64_t cvl_invoke(
    const CvlInstruction *ix,
    const CvlAccountInfo *accounts,
    int accounts_len
) {
    return sol_invoke_signed_c(ix, accounts, accounts_len, NULL, 0);
}

/**
 * Invoke a cross-program instruction with PDA signer seeds.
 */
static inline uint64_t cvl_invoke_signed(
    const CvlInstruction *ix,
    const CvlAccountInfo *accounts,
    int accounts_len,
    const CvlSignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    return sol_invoke_signed_c(ix, accounts, accounts_len,
                                signer_seeds, signer_seeds_len);
}

/**
 * Create an account meta for a writable signer.
 */
static inline CvlAccountMeta cvl_meta_writable_signer(CvlPubkey *pubkey) {
    return (CvlAccountMeta){ .pubkey = pubkey, .is_writable = true, .is_signer = true };
}

/**
 * Create an account meta for a writable non-signer.
 */
static inline CvlAccountMeta cvl_meta_writable(CvlPubkey *pubkey) {
    return (CvlAccountMeta){ .pubkey = pubkey, .is_writable = true, .is_signer = false };
}

/**
 * Create an account meta for a readonly signer.
 */
static inline CvlAccountMeta cvl_meta_readonly_signer(CvlPubkey *pubkey) {
    return (CvlAccountMeta){ .pubkey = pubkey, .is_writable = false, .is_signer = true };
}

/**
 * Create an account meta for a readonly non-signer.
 */
static inline CvlAccountMeta cvl_meta_readonly(CvlPubkey *pubkey) {
    return (CvlAccountMeta){ .pubkey = pubkey, .is_writable = false, .is_signer = false };
}

/**
 * Build and invoke a CPI instruction on the stack.
 *
 * @param program_id The program id to invoke
 * @param metas The account metas to use
 * @param metas_len The length of the account metas array
 * @param ix_data The instruction data to use
 * @param ix_data_len The length of the instruction data
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 *
 * @usage:
 *   CvlAccountMeta metas[] = {
 *       cvl_meta_writable_signer(from_key),
 *       cvl_meta_writable(to_key),
 *   };
 *   uint8_t data[] = { ... };
 *   CVL_CPI_INVOKE(
 *       &system_program_id, metas, 2, data, sizeof(data),
 *       params->accounts, params->accounts_len
 *   );
 */
#define CVL_CPI_INVOKE(program_id, metas, metas_len, ix_data, ix_data_len, \
                        accounts, accounts_len) \
    do { \
        CvlInstruction _cvl_ix = { \
            .program_id    = (program_id), \
            .accounts      = (metas), \
            .accounts_len  = (metas_len), \
            .data          = (ix_data), \
            .data_len      = (ix_data_len), \
        }; \
        uint64_t _cvl_cpi_err = cvl_invoke(&_cvl_ix, (accounts), (accounts_len)); \
        if (_cvl_cpi_err) return _cvl_cpi_err; \
    } while (0)

/**
 * Build and invoke a signed CPI instruction on the stack.
 */
#define CVL_CPI_INVOKE_SIGNED(program_id, metas, metas_len, ix_data, ix_data_len, \
                               accounts, accounts_len, signer_seeds, signer_seeds_len) \
    do { \
        CvlInstruction _cvl_ix = { \
            .program_id    = (program_id), \
            .accounts      = (metas), \
            .accounts_len  = (metas_len), \
            .data          = (ix_data), \
            .data_len      = (ix_data_len), \
        }; \
        uint64_t _cvl_cpi_err = cvl_invoke_signed( \
            &_cvl_ix, (accounts), (accounts_len), \
            (signer_seeds), (signer_seeds_len)); \
        if (_cvl_cpi_err) return _cvl_cpi_err; \
    } while (0)

#endif /* CVL_CPI_H */
