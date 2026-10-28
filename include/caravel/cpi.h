#ifndef CPI_H
#define CPI_H

#include "types.h"
#include "error.h"
#include "syscall.h"

/**
 * Invoke a cross-program instruction (no signer seeds).
 */
static inline uint64_t
invoke(const Instruction *ix, const AccountInfo *accounts, int accounts_len) {
    return sol_invoke_signed_c(ix, accounts, accounts_len, NULL, 0);
}

/**
 * Invoke a cross-program instruction with PDA signer seeds.
 */
static inline uint64_t invoke_signed(
    const Instruction *ix,
    const AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    return sol_invoke_signed_c(
        ix, accounts, accounts_len, signer_seeds, signer_seeds_len
    );
}

/**
 * Create an account meta for a writable signer.
 */
static inline AccountMeta meta_writable_signer(Pubkey *pubkey) {
    return (AccountMeta){
        .pubkey = pubkey, .is_writable = true, .is_signer = true
    };
}

/**
 * Create an account meta for a writable non-signer.
 */
static inline AccountMeta meta_writable(Pubkey *pubkey) {
    return (AccountMeta){
        .pubkey = pubkey, .is_writable = true, .is_signer = false
    };
}

/**
 * Create an account meta for a readonly signer.
 */
static inline AccountMeta meta_readonly_signer(Pubkey *pubkey) {
    return (AccountMeta){
        .pubkey = pubkey, .is_writable = false, .is_signer = true
    };
}

/**
 * Create an account meta for a readonly non-signer.
 */
static inline AccountMeta meta_readonly(Pubkey *pubkey) {
    return (AccountMeta){
        .pubkey = pubkey, .is_writable = false, .is_signer = false
    };
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
 * @code:
 *   AccountMeta metas[] = {
 *       meta_writable_signer(from_key),
 *       meta_writable(to_key),
 *   };
 *   uint8_t data[] = { ... };
 *   CPI_INVOKE(
 *       &SYSTEM_PROGRAM_ID, metas, 2, data, sizeof(data),
 *       params->accounts, params->accounts_len
 *   );
 */
#define CPI_INVOKE( \
    _program_id, _metas, _metas_len, _ix_data, _ix_data_len, _accounts, \
    _accounts_len \
) \
    do { \
        Instruction _ix = { \
            .program_id   = (Pubkey *)(_program_id), \
            .accounts     = (_metas), \
            .accounts_len = (_metas_len), \
            .data         = (_ix_data), \
            .data_len     = (_ix_data_len), \
        }; \
        uint64_t _cpi_err = invoke(&_ix, (_accounts), (_accounts_len)); \
        if (_cpi_err) return _cpi_err; \
    } while (0)

/**
 * Build and invoke a signed CPI instruction on the stack.
 */
#define CPI_INVOKE_SIGNED( \
    _program_id, _metas, _metas_len, _ix_data, _ix_data_len, _accounts, \
    _accounts_len, _signer_seeds, _signer_seeds_len \
) \
    do { \
        Instruction _ix = { \
            .program_id   = (Pubkey *)(_program_id), \
            .accounts     = (_metas), \
            .accounts_len = (_metas_len), \
            .data         = (_ix_data), \
            .data_len     = (_ix_data_len), \
        }; \
        uint64_t _cpi_err = invoke_signed( \
            &_ix, (_accounts), (_accounts_len), (_signer_seeds), \
            (_signer_seeds_len) \
        ); \
        if (_cpi_err) return _cpi_err; \
    } while (0)

#endif /* CPI_H */
