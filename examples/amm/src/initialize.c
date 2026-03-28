#include "pool.h"

/* @cvl:instruction initialize_pool 0 */

#define INIT_POOL_ACCOUNTS(X) \
    X(payer,          CVL_SIGNER | CVL_WRITABLE) \
    X(pool_state,     CVL_WRITABLE) \
    X(vault_a,        0) \
    X(vault_b,        0) \
    X(lp_mint,        0) \
    X(mint_a,         0) \
    X(mint_b,         0) \
    X(system_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(init_pool, INIT_POOL_ACCOUNTS)

uint64_t handle_initialize_pool(CvlParameters *params) {
    init_pool_accounts_t ctx;
    CVL_TRY(init_pool_parse(params, &ctx));

    CvlSignerSeed derive_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(ctx.mint_a->key),
        CVL_SEED_PUBKEY(ctx.mint_b->key),
    };
    CvlPubkey expected;
    uint8_t bump;
    cvl_find_program_address(derive_seeds, 3, params->program_id,
                             &expected, &bump);

    if (!cvl_pubkey_eq(ctx.pool_state->key, &expected)) {
        cvl_log_literal("Error: pool_state PDA mismatch");
        return CVL_ERROR_INVALID_PDA;
    }

    if (ctx.pool_state->data_len > 0) {
        PoolState *existing = CVL_ACCOUNT_STATE(ctx.pool_state, PoolState);
        if (existing->is_initialized) {
            return AMM_ERROR_ALREADY_INITIALIZED;
        }
    }

    CvlTokenAccount *va = CVL_TOKEN_ACCOUNT(ctx.vault_a);
    if (!cvl_pubkey_eq(&va->owner, &expected)) {
        cvl_log_literal("Error: vault_a owner mismatch");
        return AMM_ERROR_INVALID_VAULT_OWNER;
    }
    if (!cvl_pubkey_eq(&va->mint, ctx.mint_a->key)) {
        cvl_log_literal("Error: vault_a mint mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    CvlTokenAccount *vb = CVL_TOKEN_ACCOUNT(ctx.vault_b);
    if (!cvl_pubkey_eq(&vb->owner, &expected)) {
        cvl_log_literal("Error: vault_b owner mismatch");
        return AMM_ERROR_INVALID_VAULT_OWNER;
    }
    if (!cvl_pubkey_eq(&vb->mint, ctx.mint_b->key)) {
        cvl_log_literal("Error: vault_b mint mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    CvlMintAccount *lp = CVL_MINT_ACCOUNT(ctx.lp_mint);
    if (!cvl_pubkey_eq(&lp->mint_authority, &expected)) {
        cvl_log_literal("Error: LP mint authority mismatch");
        return AMM_ERROR_INVALID_LP_AUTHORITY;
    }

    CvlRent rent;
    cvl_get_rent(&rent);
    uint64_t lamports = cvl_minimum_balance(&rent, sizeof(PoolState));

    CvlSignerSeed signer_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(ctx.mint_a->key),
        CVL_SEED_PUBKEY(ctx.mint_b->key),
        CVL_SEED_U8(&bump),
    };
    CvlSignerSeeds signer = { .seeds = signer_seeds, .len = 4 };

    CVL_TRY(cvl_system_create_account_signed(
        ctx.payer, ctx.pool_state,
        lamports, sizeof(PoolState), params->program_id,
        params->accounts, (int)params->accounts_len,
        &signer, 1
    ));

    PoolState *state = CVL_ACCOUNT_STATE(ctx.pool_state, PoolState);
    state->is_initialized = 1;
    state->bump = bump;
    sol_memcpy_(state->mint_a.bytes, ctx.mint_a->key->bytes, 32);
    sol_memcpy_(state->mint_b.bytes, ctx.mint_b->key->bytes, 32);
    sol_memcpy_(state->vault_a.bytes, ctx.vault_a->key->bytes, 32);
    sol_memcpy_(state->vault_b.bytes, ctx.vault_b->key->bytes, 32);
    sol_memcpy_(state->lp_mint.bytes, ctx.lp_mint->key->bytes, 32);

    cvl_log_literal("Pool initialized");
    return CVL_SUCCESS;
}
