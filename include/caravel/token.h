#ifndef CVL_TOKEN_H
#define CVL_TOKEN_H

#include "types.h"
#include "error.h"
#include "cpi.h"

#define CVL_TOKEN_IX_INITIALIZE_MINT     0
#define CVL_TOKEN_IX_INITIALIZE_ACCOUNT  1
#define CVL_TOKEN_IX_TRANSFER            3
#define CVL_TOKEN_IX_APPROVE             4
#define CVL_TOKEN_IX_REVOKE              5
#define CVL_TOKEN_IX_SET_AUTHORITY       6
#define CVL_TOKEN_IX_MINT_TO             7
#define CVL_TOKEN_IX_BURN                8
#define CVL_TOKEN_IX_CLOSE_ACCOUNT       9
#define CVL_TOKEN_IX_SYNC_NATIVE        17

typedef struct __attribute__((packed)) {
    CvlPubkey mint;            /* 0..32   */
    CvlPubkey owner;           /* 32..64  */
    uint64_t  amount;          /* 64..72  */
    uint32_t  delegate_option; /* 72..76  (0 = None, 1 = Some) */
    CvlPubkey delegate;        /* 76..108 */
    uint8_t   state;           /* 108     (0=uninitialized, 1=initialized, 2=frozen) */
    uint32_t  is_native_option;/* 109..113 */
    uint64_t  is_native;       /* 113..121 */
    uint64_t  delegated_amount;/* 121..129 */
    uint32_t  close_authority_option; /* 129..133 */
    CvlPubkey close_authority; /* 133..165 */
} CvlTokenAccount;

typedef struct __attribute__((packed)) {
    uint32_t  mint_authority_option; /* 0..4 */
    CvlPubkey mint_authority;        /* 4..36 */
    uint64_t  supply;                /* 36..44 */
    uint8_t   decimals;              /* 44 */
    uint8_t   is_initialized;        /* 45 */
    uint32_t  freeze_authority_option; /* 46..50 */
    CvlPubkey freeze_authority;      /* 50..82 */
} CvlMintAccount;

/**
 * Cast account data to a CvlTokenAccount pointer.
 */
#define CVL_TOKEN_ACCOUNT(acc) ((CvlTokenAccount *)((acc)->data))

/**
 * Cast account data to a CvlMintAccount pointer.
 */
#define CVL_MINT_ACCOUNT(acc) ((CvlMintAccount *)((acc)->data))

/**
 * Transfer tokens from one token account to another.
 */
static inline uint64_t cvl_token_transfer(
    CvlAccountInfo *source,
    CvlAccountInfo *destination,
    CvlAccountInfo *authority,
    uint64_t amount,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    /* Transfer: u8 instruction (3) + u64 amount */
    uint8_t ix_data[9];
    ix_data[0] = CVL_TOKEN_IX_TRANSFER;
    *(uint64_t *)(ix_data + 1) = amount;

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(source->key),
        cvl_meta_writable(destination->key),
        cvl_meta_readonly_signer(authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Transfer tokens with PDA authority.
 */
static inline uint64_t cvl_token_transfer_signed(
    CvlAccountInfo *source,
    CvlAccountInfo *destination,
    CvlAccountInfo *authority,
    uint64_t amount,
    CvlAccountInfo *accounts,
    int accounts_len,
    const CvlSignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[9];
    ix_data[0] = CVL_TOKEN_IX_TRANSFER;
    *(uint64_t *)(ix_data + 1) = amount;

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(source->key),
        cvl_meta_writable(destination->key),
        cvl_meta_readonly_signer(authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Mint tokens to a token account.
 */
static inline uint64_t cvl_token_mint_to(
    CvlAccountInfo *mint,
    CvlAccountInfo *destination,
    CvlAccountInfo *mint_authority,
    uint64_t amount,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[9];
    ix_data[0] = CVL_TOKEN_IX_MINT_TO;
    *(uint64_t *)(ix_data + 1) = amount;

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(mint->key),
        cvl_meta_writable(destination->key),
        cvl_meta_readonly_signer(mint_authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Mint tokens with PDA mint authority.
 */
static inline uint64_t cvl_token_mint_to_signed(
    CvlAccountInfo *mint,
    CvlAccountInfo *destination,
    CvlAccountInfo *mint_authority,
    uint64_t amount,
    CvlAccountInfo *accounts,
    int accounts_len,
    const CvlSignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[9];
    ix_data[0] = CVL_TOKEN_IX_MINT_TO;
    *(uint64_t *)(ix_data + 1) = amount;

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(mint->key),
        cvl_meta_writable(destination->key),
        cvl_meta_readonly_signer(mint_authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Burn tokens from a token account.
 */
static inline uint64_t cvl_token_burn(
    CvlAccountInfo *token_account,
    CvlAccountInfo *mint,
    CvlAccountInfo *authority,
    uint64_t amount,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[9];
    ix_data[0] = CVL_TOKEN_IX_BURN;
    *(uint64_t *)(ix_data + 1) = amount;

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(token_account->key),
        cvl_meta_writable(mint->key),
        cvl_meta_readonly_signer(authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Close a token account, transferring remaining SOL to destination.
 */
static inline uint64_t cvl_token_close_account(
    CvlAccountInfo *token_account,
    CvlAccountInfo *destination,
    CvlAccountInfo *authority,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { CVL_TOKEN_IX_CLOSE_ACCOUNT };

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(token_account->key),
        cvl_meta_writable(destination->key),
        cvl_meta_readonly_signer(authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Close a token account with PDA authority.
 */
static inline uint64_t cvl_token_close_account_signed(
    CvlAccountInfo *token_account,
    CvlAccountInfo *destination,
    CvlAccountInfo *authority,
    CvlAccountInfo *accounts,
    int accounts_len,
    const CvlSignerSeeds *signer_seeds,
    int signer_seeds_len
) {
    uint8_t ix_data[1] = { CVL_TOKEN_IX_CLOSE_ACCOUNT };

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(token_account->key),
        cvl_meta_writable(destination->key),
        cvl_meta_readonly_signer(authority->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke_signed(&ix, accounts, accounts_len,
                              signer_seeds, signer_seeds_len);
}

/**
 * Approve a delegate to transfer tokens.
 */
static inline uint64_t cvl_token_approve(
    CvlAccountInfo *token_account,
    CvlAccountInfo *delegate,
    CvlAccountInfo *owner,
    uint64_t amount,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[9];
    ix_data[0] = CVL_TOKEN_IX_APPROVE;
    *(uint64_t *)(ix_data + 1) = amount;

    CvlAccountMeta metas[3] = {
        cvl_meta_writable(token_account->key),
        cvl_meta_readonly(delegate->key),
        cvl_meta_readonly_signer(owner->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 3,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

/**
 * Sync a native SOL token account's balance with its lamports.
 */
static inline uint64_t cvl_token_sync_native(
    CvlAccountInfo *token_account,
    CvlAccountInfo *accounts,
    int accounts_len
) {
    uint8_t ix_data[1] = { CVL_TOKEN_IX_SYNC_NATIVE };

    CvlAccountMeta metas[1] = {
        cvl_meta_writable(token_account->key),
    };

    CvlInstruction ix = {
        .program_id   = (CvlPubkey *)&CVL_TOKEN_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = ix_data,
        .data_len     = sizeof(ix_data),
    };

    return cvl_invoke(&ix, accounts, accounts_len);
}

#endif /* CVL_TOKEN_H */
