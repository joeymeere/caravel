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
#define TOKEN_IX_FREEZE_ACCOUNT     10
#define TOKEN_IX_THAW_ACCOUNT       11
#define TOKEN_IX_SYNC_NATIVE        17

/* SetAuthority authority types */
#define TOKEN_AUTHORITY_MINT_TOKENS     0
#define TOKEN_AUTHORITY_FREEZE_ACCOUNT  1
#define TOKEN_AUTHORITY_ACCOUNT_OWNER   2
#define TOKEN_AUTHORITY_CLOSE_ACCOUNT   3

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
 * Initialize a mint account.
 *
 * The account must already be allocated and owned by the token program
 * (e.g. via a system program create_account CPI) before calling this.
 *
 * @param mint The mint account to initialize
 * @param rent_sysvar The Rent sysvar account
 * @param mint_authority The authority allowed to mint new tokens
 * @param freeze_authority The authority allowed to freeze token accounts, or NULL for none
 * @param decimals Number of base 10 digits to the right of the decimal place
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_INITIALIZE_MINT(mint, rent_sysvar, mint_authority, freeze_authority, decimals, accounts, accounts_len);
 */
static inline uint64_t token_initialize_mint(
    AccountInfo *mint,
    AccountInfo *rent_sysvar,
    const Pubkey *mint_authority,
    const Pubkey *freeze_authority,
    uint8_t decimals,
    AccountInfo *accounts,
    int accounts_len
) {
    /* InitializeMint: u8 instruction (0) + u8 decimals + Pubkey mint_authority
     * + COption<Pubkey> freeze_authority (u8 tag, then 32 bytes when present) */
    uint8_t ix_data[67];
    uint64_t data_len;

    ix_data[0] = TOKEN_IX_INITIALIZE_MINT;
    ix_data[1] = decimals;
    sol_memcpy_(ix_data + 2, mint_authority, sizeof(Pubkey));

    if (freeze_authority) {
        ix_data[34] = 1;
        sol_memcpy_(ix_data + 35, freeze_authority, sizeof(Pubkey));
        data_len = 67;
    } else {
        ix_data[34] = 0;
        data_len = 35;
    }

    AccountMeta metas[2] = {
        meta_writable(mint->key),
        meta_readonly(rent_sysvar->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = data_len,
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Initialize a token account for a given mint and owner.
 *
 * The account must already be allocated and owned by the token program
 * (e.g. via a system program create_account CPI) before calling this.
 *
 * @param token_account The token account to initialize
 * @param mint The mint this token account holds
 * @param owner The owner of the token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_INITIALIZE_ACCOUNT(token_account, mint, owner, accounts, accounts_len);
 */
static inline uint64_t token_initialize_account(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *owner,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_INITIALIZE_ACCOUNT };

    AccountMeta metas[4] = {
        meta_writable(token_account->key),
        meta_readonly(mint->key),
        meta_readonly(owner->key),
        meta_readonly((Pubkey *)&RENT_SYSVAR_ID),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 4,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Initialize a token account, invoking with PDA signer seeds.
 *
 * Provided for symmetry with the other signed helpers. Note that
 * InitializeAccount takes no signer accounts, so the seeds are not
 * consumed by the token program itself. Use this only when the
 * surrounding CPI context requires signed invocation.
 *
 * @param token_account The token account to initialize
 * @param mint The mint this token account holds
 * @param owner The owner of the token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_INITIALIZE_ACCOUNT_SIGNED(token_account, mint, owner, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_initialize_account_signed(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *owner,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_INITIALIZE_ACCOUNT };

    AccountMeta metas[4] = {
        meta_writable(token_account->key),
        meta_readonly(mint->key),
        meta_readonly(owner->key),
        meta_readonly((Pubkey *)&RENT_SYSVAR_ID),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 4,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

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
 * Burn tokens with PDA authority.
 *
 * @param token_account The token account to burn from
 * @param mint The mint of the tokens being burned
 * @param authority The authority to burn the tokens
 * @param amount The amount of tokens to burn
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_BURN_SIGNED(token_account, mint, authority, amount, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_burn_signed(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *authority,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
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

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
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
 * Approve a delegate with PDA owner.
 *
 * @param token_account The token account to approve the delegate for
 * @param delegate The delegate to approve
 * @param owner The owner of the token account
 * @param amount The amount of tokens the delegate may transfer
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_APPROVE_SIGNED(token_account, delegate, owner, amount, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_approve_signed(
    AccountInfo *token_account,
    AccountInfo *delegate,
    AccountInfo *owner,
    uint64_t amount,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
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

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Revoke a previously approved delegate.
 *
 * @param token_account The token account to revoke the delegate for
 * @param owner The owner of the token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_REVOKE(token_account, owner, accounts, accounts_len);
 */
static inline uint64_t token_revoke(
    AccountInfo *token_account,
    AccountInfo *owner,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_REVOKE };

    AccountMeta metas[2] = {
        meta_writable(token_account->key),
        meta_readonly_signer(owner->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Revoke a previously approved delegate with PDA owner.
 *
 * @param token_account The token account to revoke the delegate for
 * @param owner The owner of the token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_REVOKE_SIGNED(token_account, owner, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_revoke_signed(
    AccountInfo *token_account,
    AccountInfo *owner,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_REVOKE };

    AccountMeta metas[2] = {
        meta_writable(token_account->key),
        meta_readonly_signer(owner->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Set a new authority on a mint or token account.
 *
 * @param owned The mint or token account to change the authority of
 * @param current_authority The current authority for the given authority type
 * @param authority_type One of the TOKEN_AUTHORITY_* constants
 * @param new_authority The new authority, or NULL to remove the authority
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_SET_AUTHORITY(owned, current_authority, TOKEN_AUTHORITY_MINT_TOKENS, new_authority, accounts, accounts_len);
 */
static inline uint64_t token_set_authority(
    AccountInfo *owned,
    AccountInfo *current_authority,
    uint8_t authority_type,
    const Pubkey *new_authority,
    AccountInfo *accounts,
    int accounts_len
) {
    /* SetAuthority: u8 instruction (6) + u8 authority_type
     * + COption<Pubkey> new_authority (u8 tag, then 32 bytes when present) */
    uint8_t ix_data[35];
    uint64_t data_len;

    ix_data[0] = TOKEN_IX_SET_AUTHORITY;
    ix_data[1] = authority_type;

    if (new_authority) {
        ix_data[2] = 1;
        sol_memcpy_(ix_data + 3, new_authority, sizeof(Pubkey));
        data_len = 35;
    } else {
        ix_data[2] = 0;
        data_len = 3;
    }

    AccountMeta metas[2] = {
        meta_writable(owned->key),
        meta_readonly_signer(current_authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = data_len,
    };

    return invoke(&ix, accounts, accounts_len);
}

/**
 * Set a new authority on a mint or token account with PDA authority.
 *
 * @param owned The mint or token account to change the authority of
 * @param current_authority The current authority for the given authority type
 * @param authority_type One of the TOKEN_AUTHORITY_* constants
 * @param new_authority The new authority, or NULL to remove the authority
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_SET_AUTHORITY_SIGNED(owned, current_authority, TOKEN_AUTHORITY_MINT_TOKENS, new_authority, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_set_authority_signed(
    AccountInfo *owned,
    AccountInfo *current_authority,
    uint8_t authority_type,
    const Pubkey *new_authority,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[35];
    uint64_t data_len;

    ix_data[0] = TOKEN_IX_SET_AUTHORITY;
    ix_data[1] = authority_type;

    if (new_authority) {
        ix_data[2] = 1;
        sol_memcpy_(ix_data + 3, new_authority, sizeof(Pubkey));
        data_len = 35;
    } else {
        ix_data[2] = 0;
        data_len = 3;
    }

    AccountMeta metas[2] = {
        meta_writable(owned->key),
        meta_readonly_signer(current_authority->key),
    };

    Instruction ix = {
        .program_id   = (Pubkey *)&TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 2,
        .data         = ix_data,
        .data_len     = data_len,
    };

    return invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Freeze a token account.
 *
 * @param token_account The token account to freeze
 * @param mint The mint of the token account
 * @param freeze_authority The mint's freeze authority
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_FREEZE_ACCOUNT(token_account, mint, freeze_authority, accounts, accounts_len);
 */
static inline uint64_t token_freeze_account(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *freeze_authority,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_FREEZE_ACCOUNT };

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_readonly(mint->key),
        meta_readonly_signer(freeze_authority->key),
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
 * Freeze a token account with PDA freeze authority.
 *
 * @param token_account The token account to freeze
 * @param mint The mint of the token account
 * @param freeze_authority The mint's freeze authority
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_FREEZE_ACCOUNT_SIGNED(token_account, mint, freeze_authority, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_freeze_account_signed(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *freeze_authority,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_FREEZE_ACCOUNT };

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_readonly(mint->key),
        meta_readonly_signer(freeze_authority->key),
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
 * Thaw a frozen token account.
 *
 * @param token_account The token account to thaw
 * @param mint The mint of the token account
 * @param freeze_authority The mint's freeze authority
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_THAW_ACCOUNT(token_account, mint, freeze_authority, accounts, accounts_len);
 */
static inline uint64_t token_thaw_account(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *freeze_authority,
    AccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_THAW_ACCOUNT };

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_readonly(mint->key),
        meta_readonly_signer(freeze_authority->key),
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
 * Thaw a frozen token account with PDA freeze authority.
 *
 * @param token_account The token account to thaw
 * @param mint The mint of the token account
 * @param freeze_authority The mint's freeze authority
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds to use
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, ERROR_INVALID_ARGUMENT on failure
 *
 * @code TOKEN_THAW_ACCOUNT_SIGNED(token_account, mint, freeze_authority, accounts, accounts_len, signer_seeds, signer_seeds_len);
 */
static inline uint64_t token_thaw_account_signed(
    AccountInfo *token_account,
    AccountInfo *mint,
    AccountInfo *freeze_authority,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[1] = { TOKEN_IX_THAW_ACCOUNT };

    AccountMeta metas[3] = {
        meta_writable(token_account->key),
        meta_readonly(mint->key),
        meta_readonly_signer(freeze_authority->key),
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

/**
 * Create an associated token account for the given owner and mint.
 *
 * @param payer The account paying for the ATA creation (writable signer)
 * @param ata The associated token account to create (writable)
 * @param owner The owner of the associated token account
 * @param mint The mint of the associated token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, error code on failure
 */
static inline uint64_t create_associated_token_account(
    AccountInfo *payer,
    AccountInfo *ata,
    AccountInfo *owner,
    AccountInfo *mint,
    AccountInfo *accounts,
    int accounts_len
) {

  AccountMeta metas[6] = {
      meta_writable_signer(payer->key),
      meta_writable(ata->key),
      meta_readonly(owner->key),
      meta_readonly(mint->key),
      meta_readonly((Pubkey *)&SYSTEM_PROGRAM_ID),
      meta_readonly((Pubkey *)&TOKEN_PROGRAM_ID),
  };

  Instruction ix = {
      .program_id = (Pubkey *)&ASSOCIATED_TOKEN_PROGRAM_ID,
      .accounts = metas,
      .accounts_len = 6,
      .data = (uint8_t *)NULL,
      .data_len = 0,
  };

  return invoke(&ix, accounts, accounts_len);
}

/**
 * Create an associated token account, or no-op if it already exists.
 *
 * @param payer The account paying for the ATA creation (writable signer)
 * @param ata The associated token account to create (writable)
 * @param owner The owner of the associated token account
 * @param mint The mint of the associated token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @return SUCCESS on success, error code on failure
 */
static inline uint64_t create_associated_token_account_idempotent(
    AccountInfo *payer,
    AccountInfo *ata,
    AccountInfo *owner,
    AccountInfo *mint,
    AccountInfo *accounts,
    int accounts_len
) {

  AccountMeta metas[6] = {
      meta_writable_signer(payer->key),
      meta_writable(ata->key),
      meta_readonly(owner->key),
      meta_readonly(mint->key),
      meta_readonly((Pubkey *)&SYSTEM_PROGRAM_ID),
      meta_readonly((Pubkey *)&TOKEN_PROGRAM_ID),
  };

  uint8_t ix_data[1] = {1};

  Instruction ix = {
      .program_id = (Pubkey *)&ASSOCIATED_TOKEN_PROGRAM_ID,
      .accounts = metas,
      .accounts_len = 6,
      .data = ix_data,
      .data_len = 1,
  };

  return invoke(&ix, accounts, accounts_len);
}

/**
 * Create an associated token account with a PDA as the payer.
 *
 * @param payer The PDA paying for the ATA creation (writable signer)
 * @param ata The associated token account to create (writable)
 * @param owner The owner of the associated token account
 * @param mint The mint of the associated token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds for the PDA payer
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, error code on failure
 */
static inline uint64_t create_associated_token_account_signed(
    AccountInfo *payer,
    AccountInfo *ata,
    AccountInfo *owner,
    AccountInfo *mint,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {

  AccountMeta metas[6] = {
      meta_writable_signer(payer->key),
      meta_writable(ata->key),
      meta_readonly(owner->key),
      meta_readonly(mint->key),
      meta_readonly((Pubkey *)&SYSTEM_PROGRAM_ID),
      meta_readonly((Pubkey *)&TOKEN_PROGRAM_ID),
  };

  Instruction ix = {
      .program_id = (Pubkey *)&ASSOCIATED_TOKEN_PROGRAM_ID,
      .accounts = metas,
      .accounts_len = 6,
      .data = (uint8_t *)NULL,
      .data_len = 0,
  };

  return invoke_signed(&ix, accounts, accounts_len, signer_seeds, signer_seeds_len);
}

/**
 * Create an associated token account with a PDA as the payer, or no-op if it already exists.
 *
 * @param payer The PDA paying for the ATA creation (writable signer)
 * @param ata The associated token account to create (writable)
 * @param owner The owner of the associated token account
 * @param mint The mint of the associated token account
 * @param accounts The accounts to use
 * @param accounts_len The length of the accounts array
 * @param signer_seeds The signer seeds for the PDA payer
 * @param signer_seeds_len The length of the signer seeds array
 * @return SUCCESS on success, error code on failure
 */
static inline uint64_t create_associated_token_account_idempotent_signed(
    AccountInfo *payer,
    AccountInfo *ata,
    AccountInfo *owner,
    AccountInfo *mint,
    AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signer_seeds,
    int signer_seeds_len
) {

  uint8_t ix_data[1] = {1};

  AccountMeta metas[6] = {
      meta_writable_signer(payer->key),
      meta_writable(ata->key),
      meta_readonly(owner->key),
      meta_readonly(mint->key),
      meta_readonly((Pubkey *)&SYSTEM_PROGRAM_ID),
      meta_readonly((Pubkey *)&TOKEN_PROGRAM_ID),
  };

  Instruction ix = {
      .program_id = (Pubkey *)&ASSOCIATED_TOKEN_PROGRAM_ID,
      .accounts = metas,
      .accounts_len = 6,
      .data = ix_data,
      .data_len = 1,
  };

  return invoke_signed(&ix, accounts, accounts_len, signer_seeds, signer_seeds_len);
}

#endif /* TOKEN_H */
