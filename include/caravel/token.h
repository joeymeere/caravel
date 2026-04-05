/**
 * @brief Token program helpers
 *
 * @note Compile opts:
 *   - NO_TOKEN       — Exclude this header entirely, no token program helpers
 */

#ifndef TOKEN_H
#define TOKEN_H

#include "types.h"
#include "error.h"
#include "cpi.h"

#define TOKEN_IX_INITIALIZE_MINT     0
#define TOKEN_IX_INITIALIZE_ACCOUNT  1
#define TOKEN_IX_TRANSFER            3
#define TOKEN_IX_APPROVE             4
#define TOKEN_IX_REVOKE              5
#define TOKEN_IX_SET_AUTHORITY       6
#define TOKEN_IX_MINT_TO             7
#define TOKEN_IX_BURN                8
#define TOKEN_IX_CLOSE_ACCOUNT       9
#define TOKEN_IX_SYNC_NATIVE        17

typedef struct __attribute__((packed)) {
    Pubkey mint;            /* 0..32   */
    Pubkey owner;           /* 32..64  */
    uint64_t  amount;          /* 64..72  */
    uint32_t  delegate_option; /* 72..76  (0 = None, 1 = Some) */
    Pubkey delegate;        /* 76..108 */
    uint8_t   state;           /* 108     (0=uninitialized, 1=initialized, 2=frozen) */
    uint32_t  is_native_option;/* 109..113 */
    uint64_t  is_native;       /* 113..121 */
    uint64_t  delegated_amount;/* 121..129 */
    uint32_t  close_authority_option; /* 129..133 */
    Pubkey close_authority; /* 133..165 */
} TokenAccount;

typedef struct __attribute__((packed)) {
    uint32_t  mint_authority_option; /* 0..4 */
    Pubkey mint_authority;        /* 4..36 */
    uint64_t  supply;                /* 36..44 */
    uint8_t   decimals;              /* 44 */
    uint8_t   is_initialized;        /* 45 */
    uint32_t  freeze_authority_option; /* 46..50 */
    Pubkey freeze_authority;      /* 50..82 */
} MintAccount;

/**
 * Cast account data to a TokenAccount pointer.
 */
#define TOKEN_ACCOUNT(acc) ((TokenAccount *)((acc)->data))

/**
 * Cast account data to a MintAccount pointer.
 */
#define MINT_ACCOUNT(acc) ((MintAccount *)((acc)->data))

/**
 * Transfer tokens from one token account to another.
 *
 * @param source The source token account to transfer from
 * @param destination The destination token account to transfer to
 * @param authority The authority to transfer the tokens
 * @param amount The amount of tokens to transfer
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_TRANSFER(source, destination, authority, amount, accounts, accounts_len);
 */
static inline uint64_t token_transfer(
    AccountInfo *source,
    AccountInfo *destination,
    AccountInfo *authority,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len
) {
    /* Transfer: u8 instruction (3) + u64 amount */
    uint8_t ix_data[9];
    ix_data[0] = TOKEN_IX_TRANSFER;
    *(uint64_t *)(ix_data + 1) = amount;

    AccountMeta metas[3] = {
        meta_writable(source->key),
        meta_writable(destination->key),
        meta_readonly_signer(authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Transfer tokens with PDA authority.
 *
 * @param source The source token account to transfer from
 * @param destination The destination token account to transfer to
 * @param authority The authority to transfer the tokens
 * @param amount The amount of tokens to transfer
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_TRANSFER_SIGNED(source, destination, authority, amount, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_transfer_signed(
    AccountInfo *source,
    AccountInfo *destination,
    AccountInfo *authority,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[9];
    ix_data[0] = TOKEN_IX_TRANSFER;
    *(uint64_t *)(ix_data + 1) = amount;

    AccountMeta metas[3] = {
        meta_writable(source->key),
        meta_writable(destination->key),
        meta_readonly_signer(authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Mint tokens to a token account.
 *
 * @param mint The mint account to mint to
 * @param destination The destination account to mint to
 * @param mint_authority The mint authority to use
 * @param amount The amount of tokens to mint
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_MINT_TO(mint, destination, mint_authority, amount, accounts, accounts_len);
 */
static inline uint64_t token_mint_to(
    AccountInfo *mint,
    AccountInfo *destination,
    AccountInfo *mint_authority,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[9];
    ix_data[0] = TOKEN_IX_MINT_TO;
    *(uint64_t *)(ix_data + 1) = amount;

    AccountMeta metas[3] = {
        meta_writable(mint->key),
        meta_writable(destination->key),
        meta_readonly_signer(mint_authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Mint tokens with PDA mint authority.
 *
 * @param mint The mint account to mint to
 * @param destination The destination account to mint to
 * @param mint_authority The mint authority to use
 * @param amount The amount of tokens to mint
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_MINT_TO_SIGNED(mint, destination, mint_authority, amount, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_mint_to_signed(
    AccountInfo *mint,
    AccountInfo *destination,
    AccountInfo *mint_authority,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[9];
    ix_data[0] = TOKEN_IX_MINT_TO;
    *(uint64_t *)(ix_data + 1) = amount;

    AccountMeta metas[3] = {
        meta_writable(mint->key),
        meta_writable(destination->key),
        meta_readonly_signer(mint_authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Burn tokens from a token account.
 *
 * @param token_account The token account to burn
 * @param mint The mint account to burn from
 * @param authority The authority to burn the tokens
 * @param amount The amount of tokens to burn
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_BURN(token_account, mint, authority, amount, accounts, accounts_len);
 */
static inline uint64_t token_burn(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *authority,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[9];
    ix_data[0] = TOKEN_IX_BURN;
    *(uint64_t *)(ix_data + 1) = amount;

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_writable(mint->key),
        meta_readonly_signer(authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Close a token account, transferring remaining SOL to destination.
 *
 * @param token_account The token account to close
 * @param destination The destination account to transfer remaining SOL to
 * @param authority The authority to close the token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_CLOSE_ACCOUNT(token_account, destination, authority, accounts, accounts_len);
 */
static inline uint64_t token_close_account(
    AccountInfo *token_account,
    AccountInfo *destination,
    AccountInfo *authority,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_CLOSE_ACCOUNT };

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_writable(destination->key),
        meta_readonly_signer(authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Close a token account with PDA authority.
 *
 * @param token_account The token account to close
 * @param destination The destination account to transfer remaining SOL to
 * @param authority The authority to close the token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_CLOSE_ACCOUNT_SIGNED(token_account, destination, authority, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_close_account_signed(
    AccountInfo *token_account,
    AccountInfo *destination,
    AccountInfo *authority,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_CLOSE_ACCOUNT };

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_writable(destination->key),
        meta_readonly_signer(authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Approve a delegate to transfer tokens.
 *
 * @param token_account The token account to approve
 * @param delegate The delegate to approve
 * @param owner The owner of the token account
 * @param amount The amount of tokens to approve
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_APPROVE(token_account, delegate, owner, amount, accounts, accounts_len);
 */
static inline uint64_t token_approve(
    AccountInfo *token_account,
    AccountInfo *delegate,
    AccountInfo *owner,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[9];
    ix_data[0] = TOKEN_IX_APPROVE;
    *(uint64_t *)(ix_data + 1) = amount;

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_readonly(delegate->key),
        meta_readonly_signer(owner->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Sync a native SOL token account's balance with its lamports.
 *
 * @param token_account The token account to sync
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_SYNC_NATIVE(token_account, accounts, accounts_len);
 */
static inline uint64_t token_sync_native(
    AccountInfo *token_account,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_SYNC_NATIVE };

    AccountMeta metas[1] = {
        meta_writable(token_account->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

#endif /* TOKEN_H */
