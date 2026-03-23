#ifndef CVL_SYSTEM_H
#define CVL_SYSTEM_H

#include "types.h"
#include "error.h"
#include "cpi.h"

#define CVL_SYSTEM_IX_CREATE_ACCOUNT  0
#define CVL_SYSTEM_IX_ASSIGN          1
#define CVL_SYSTEM_IX_TRANSFER        2
#define CVL_SYSTEM_IX_ALLOCATE        8

/**
 * Transfer lamports from one account to another via the System program.
 */
static inline uint64_t cvl_system_transfer(
    CvlAccountInfo *from,
    CvlAccountInfo *to,
    uint64_t lamports,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    /* System transfer instruction: u32 index (2) + u64 lamports */
    uint8_t ix_data[12];
    *(uint32_t *)ix_data = CVL_SYSTEM_IX_TRANSFER;
    *(uint64_t *)(ix_data + 4) = lamports;

    CvlAccountMeta metas[2] = {
        cvl_meta_writable_signer(from->key),
        cvl_meta_writable(to->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Transfer lamports from a PDA to another account.
 */
static inline uint64_t cvl_system_transfer_signed(
    CvlAccountInfo *from,
    CvlAccountInfo *to,
    uint64_t lamports,
    CvlAccountInfo *accounts,
    int accounts_len,
    const CvlSignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[12];
    *(uint32_t *)ix_data = CVL_SYSTEM_IX_TRANSFER;
    *(uint64_t *)(ix_data + 4) = lamports;

    CvlAccountMeta metas[2] = {
        cvl_meta_writable_signer(from->key),
        cvl_meta_writable(to->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Create a new account via the System program.
 */
static inline uint64_t cvl_system_create_account(
    CvlAccountInfo *payer,
    CvlAccountInfo *new_account,
    uint64_t lamports,
    uint64_t space,
    const CvlPubkey *owner,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    /* CreateAccount: u32 index (0) + u64 lamports + u64 space + [32] owner */
    uint8_t ix_data[52];
    *(uint32_t *)ix_data = CVL_SYSTEM_IX_CREATE_ACCOUNT;
    *(uint64_t *)(ix_data + 4) = lamports;
    *(uint64_t *)(ix_data + 12) = space;
    sol_memcpy_(ix_data + 20, owner->bytes, 32);

    CvlAccountMeta metas[2] = {
        cvl_meta_writable_signer(payer->key),
        cvl_meta_writable_signer(new_account->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Create a new PDA account via the System program (signed by PDA seeds).
 */
static inline uint64_t cvl_system_create_account_signed(
    CvlAccountInfo *payer,
    CvlAccountInfo *new_account,
    uint64_t lamports,
    uint64_t space,
    const CvlPubkey *owner,
    CvlAccountInfo *accounts,
    int accounts_len,
    const CvlSignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[52];
    *(uint32_t *)ix_data = CVL_SYSTEM_IX_CREATE_ACCOUNT;
    *(uint64_t *)(ix_data + 4) = lamports;
    *(uint64_t *)(ix_data + 12) = space;
    sol_memcpy_(ix_data + 20, owner->bytes, 32);

    CvlAccountMeta metas[2] = {
        cvl_meta_writable_signer(payer->key),
        cvl_meta_writable_signer(new_account->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Allocate space for an account.
 */
static inline uint64_t cvl_system_allocate(
    CvlAccountInfo *account,
    uint64_t space,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[12];
    *(uint32_t *)ix_data = CVL_SYSTEM_IX_ALLOCATE;
    *(uint64_t *)(ix_data + 4) = space;

    CvlAccountMeta metas[1] = {
        cvl_meta_writable_signer(account->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Assign an account to a new program owner.
 */
static inline uint64_t cvl_system_assign(
    CvlAccountInfo *account,
    const CvlPubkey *owner,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[36];
    *(uint32_t *)ix_data = CVL_SYSTEM_IX_ASSIGN;
    sol_memcpy_(ix_data + 4, owner->bytes, 32);

    CvlAccountMeta metas[1] = {
        cvl_meta_writable_signer(account->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

#endif /* CVL_SYSTEM_H */
