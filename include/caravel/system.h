/**
 * @brief System program helpers
 *
 * @note Compile opts:
 *   - NO_SYSTEM       — Exclude this header entirely, no system program helpers
 */

#ifndef SYSTEM_H
#define SYSTEM_H

#include "types.h"
#include "error.h"
#include "cpi.h"
#include "util.h"

#define SYSTEM_IX_CREATE_ACCOUNT  0
#define SYSTEM_IX_ASSIGN          1
#define SYSTEM_IX_TRANSFER        2
#define SYSTEM_IX_ALLOCATE        8

/**
 * Transfer lamports from one account to another via the System program.
 *
 * @param from The account to transfer from
 * @param to The account to transfer to
 * @param lamports The number of lamports to transfer
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code SYSTEM_TRANSFER(from, to, lamports, accounts, accounts_len);
 */
static inline uint64_t system_transfer(
    AccountInfo *from,
    AccountInfo *to,
    uint64_t lamports,
    AccountInfo *accounts,
    int accounts_len
) {
    /* System transfer instruction: u32 index (2) + u64 lamports */
    uint8_t ix_data[12];
    *(uint32_t *)ix_data = SYSTEM_IX_TRANSFER;
    *(uint64_t *)(ix_data + 4) = lamports;

    AccountMeta metas[2] = {
        meta_writable_signer(from->key),
        meta_writable(to->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Transfer lamports from a PDA to another account.
 *
 * @param from The account to transfer from
 * @param to The account to transfer to
 * @param lamports The number of lamports to transfer
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code SYSTEM_TRANSFER_SIGNED(from, to, lamports, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t system_transfer_signed(
    AccountInfo *from,
    AccountInfo *to,
    uint64_t lamports,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[12];
    *(uint32_t *)ix_data = SYSTEM_IX_TRANSFER;
    *(uint64_t *)(ix_data + 4) = lamports;

    AccountMeta metas[2] = {
        meta_writable_signer(from->key),
        meta_writable(to->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Create a new account via the System program.
 *
 * @param payer The account to pay for the new account
 * @param new_account The new account to create
 * @param lamports The number of lamports to transfer
 * @param space The space to allocate for the new account
 * @param owner The owner of the new account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code SYSTEM_CREATE_ACCOUNT(payer, new_account, lamports, space, owner, accounts, accounts_len);
 */
static inline uint64_t system_create_account(
    AccountInfo *payer,
    AccountInfo *new_account,
    uint64_t lamports,
    uint64_t space,
    const Pubkey *owner,
    AccountInfo *accounts,
    int accounts_len
) {
    /* CreateAccount: u32 index (0) + u64 lamports + u64 space + [32] owner */
    uint8_t ix_data[52];
    *(uint32_t *)ix_data = SYSTEM_IX_CREATE_ACCOUNT;
    *(uint64_t *)(ix_data + 4) = lamports;
    *(uint64_t *)(ix_data + 12) = space;
    pubkey_cpy(ix_data + 20, owner->bytes);

    AccountMeta metas[2] = {
        meta_writable_signer(payer->key),
        meta_writable_signer(new_account->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Create a new PDA account via the System program (signed by PDA seeds).
 *
 * @param payer The account to pay for the new account
 * @param new_account The new account to create
 * @param lamports The number of lamports to transfer
 * @param space The space to allocate for the new account
 * @param owner The owner of the new account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code SYSTEM_CREATE_ACCOUNT_SIGNED(payer, new_account, lamports, space, owner, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t system_create_account_signed(
    AccountInfo *payer,
    AccountInfo *new_account,
    uint64_t lamports,
    uint64_t space,
    const Pubkey *owner,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[52];
    *(uint32_t *)ix_data = SYSTEM_IX_CREATE_ACCOUNT;
    *(uint64_t *)(ix_data + 4) = lamports;
    *(uint64_t *)(ix_data + 12) = space;
    pubkey_cpy(ix_data + 20, owner->bytes);

    AccountMeta metas[2] = {
        meta_writable_signer(payer->key),
        meta_writable_signer(new_account->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Allocate space for an account.
 *
 * @param account The account to allocate space for
 * @param space The space to allocate for the account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code SYSTEM_ALLOCATE(account, space, accounts, accounts_len);
 */
static inline uint64_t system_allocate(
    AccountInfo *account,
    uint64_t space,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[12];
    *(uint32_t *)ix_data = SYSTEM_IX_ALLOCATE;
    *(uint64_t *)(ix_data + 4) = space;

    AccountMeta metas[1] = {
        meta_writable_signer(account->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Assign an account to a new program owner.
 *
 * @param account The account to assign
 * @param owner The new owner of the account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code SYSTEM_ASSIGN(account, owner, accounts, accounts_len);
 */
static inline uint64_t system_assign(
    AccountInfo *account,
    const Pubkey *owner,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[36];
    *(uint32_t *)ix_data = SYSTEM_IX_ASSIGN;
    pubkey_cpy(ix_data + 4, owner->bytes);

    AccountMeta metas[1] = {
        meta_writable_signer(account->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Assign a PDA account to a new program owner (signed by PDA seeds).
 */
static inline uint64_t system_assign_signed(
    AccountInfo *account,
    const Pubkey *owner,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[36];
    *(uint32_t *)ix_data = SYSTEM_IX_ASSIGN;
    pubkey_cpy(ix_data + 4, owner->bytes);

    AccountMeta metas[1] = {
        meta_writable_signer(account->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

#endif /* SYSTEM_H */
