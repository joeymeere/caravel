#include "pool.h"

uint64_t initialize_pool(
    initialize_pool_accounts_t *ctx, initialize_pool_args_t *args,
    CvlParameters *params
) {
    (void)args;

    CvlSignerSeed derive_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(ctx->mint_a->key),
        CVL_SEED_PUBKEY(ctx->mint_b->key),
    };
    CvlPubkey expected;
    uint8_t bump;
    cvl_find_program_address(derive_seeds, 3, params->program_id,
                             &expected, &bump);

    if (!cvl_pubkey_eq(ctx->pool_state->key, &expected))
        return CVL_ERROR_INVALID_PDA;

    if (ctx->pool_state->data_len > 0) {
        PoolState *existing = CVL_ACCOUNT_STATE(ctx->pool_state, PoolState);
        if (existing->is_initialized)
            return AMM_ERROR_ALREADY_INITIALIZED;
    }

    CvlTokenAccount *va = CVL_TOKEN_ACCOUNT(ctx->vault_a);
    if (!cvl_pubkey_eq(&va->owner, &expected))
        return AMM_ERROR_INVALID_VAULT_OWNER;
    if (!cvl_pubkey_eq(&va->mint, ctx->mint_a->key))
        return CVL_ERROR_INVALID_ARGUMENT;

    CvlTokenAccount *vb = CVL_TOKEN_ACCOUNT(ctx->vault_b);
    if (!cvl_pubkey_eq(&vb->owner, &expected))
        return AMM_ERROR_INVALID_VAULT_OWNER;
    if (!cvl_pubkey_eq(&vb->mint, ctx->mint_b->key))
        return CVL_ERROR_INVALID_ARGUMENT;

    CvlMintAccount *lp = CVL_MINT_ACCOUNT(ctx->lp_mint);
    if (!cvl_pubkey_eq(&lp->mint_authority, &expected))
        return AMM_ERROR_INVALID_LP_AUTHORITY;

    CvlRent rent;
    cvl_get_rent(&rent);
    uint64_t lamports = cvl_minimum_balance(&rent, sizeof(PoolState));

    CvlSignerSeed signer_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(ctx->mint_a->key),
        CVL_SEED_PUBKEY(ctx->mint_b->key),
        CVL_SEED_U8(&bump),
    };
    CvlSignerSeeds signer = { .seeds = signer_seeds, .len = 4 };

    CVL_TRY(cvl_system_create_account_signed(
        ctx->payer, ctx->pool_state,
        lamports, sizeof(PoolState), params->program_id,
        params->accounts, (int)params->accounts_len,
        &signer, 1
    ));

    PoolState *state = CVL_ACCOUNT_STATE(ctx->pool_state, PoolState);
    state->is_initialized = 1;
    state->bump = bump;
    sol_memcpy_(state->mint_a.bytes, ctx->mint_a->key->bytes, 32);
    sol_memcpy_(state->mint_b.bytes, ctx->mint_b->key->bytes, 32);
    sol_memcpy_(state->vault_a.bytes, ctx->vault_a->key->bytes, 32);
    sol_memcpy_(state->vault_b.bytes, ctx->vault_b->key->bytes, 32);
    sol_memcpy_(state->lp_mint.bytes, ctx->lp_mint->key->bytes, 32);

    return CVL_SUCCESS;
}
