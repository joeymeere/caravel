#include "pool.h"

uint64_t initialize_pool(
    initialize_pool_accounts_t *ctx, initialize_pool_args_t *args,
    Parameters *params
) {
    (void)args;

    SignerSeed derive_seeds[] = {
        SEED_STR("amm"),
        SEED_PUBKEY(ctx->mint_a->key),
        SEED_PUBKEY(ctx->mint_b->key),
    };
    Pubkey expected;
    uint8_t bump;
    find_program_address(derive_seeds, 3, params->program_id,
                             &expected, &bump);

    if (!pubkey_eq(ctx->pool_state->key, &expected))
        return ERROR_INVALID_PDA;

    if (ctx->pool_state->data_len > 0) {
        PoolState *existing = ACCOUNT_STATE(ctx->pool_state, PoolState);
        if (existing->is_initialized)
            return AMM_ERROR_ALREADY_INITIALIZED;
    }

    TokenAccount *va = TOKEN_ACCOUNT(ctx->vault_a);
    if (!pubkey_eq(&va->owner, &expected))
        return AMM_ERROR_INVALID_VAULT_OWNER;
    if (!pubkey_eq(&va->mint, ctx->mint_a->key))
        return ERROR_INVALID_ARGUMENT;

    TokenAccount *vb = TOKEN_ACCOUNT(ctx->vault_b);
    if (!pubkey_eq(&vb->owner, &expected))
        return AMM_ERROR_INVALID_VAULT_OWNER;
    if (!pubkey_eq(&vb->mint, ctx->mint_b->key))
        return ERROR_INVALID_ARGUMENT;

    MintAccount *lp = MINT_ACCOUNT(ctx->lp_mint);
    if (!pubkey_eq(&lp->mint_authority, &expected))
        return AMM_ERROR_INVALID_LP_AUTHORITY;

    Rent rent;
    get_rent(&rent);
    uint64_t lamports = minimum_balance(&rent, sizeof(PoolState));

    SignerSeed signer_seeds[] = {
        SEED_STR("amm"),
        SEED_PUBKEY(ctx->mint_a->key),
        SEED_PUBKEY(ctx->mint_b->key),
        SEED_U8(&bump),
    };
    SignerSeeds signer = { .seeds = signer_seeds, .len = 4 };

    TRY(system_create_account_signed(
        ctx->payer, ctx->pool_state,
        lamports, sizeof(PoolState), params->program_id,
        params->accounts, (int)params->accounts_len,
        &signer, 1
    ));

    PoolState *state = ACCOUNT_STATE(ctx->pool_state, PoolState);
    state->is_initialized = 1;
    state->bump = bump;
    sol_memcpy_(state->mint_a.bytes, ctx->mint_a->key->bytes, 32);
    sol_memcpy_(state->mint_b.bytes, ctx->mint_b->key->bytes, 32);
    sol_memcpy_(state->vault_a.bytes, ctx->vault_a->key->bytes, 32);
    sol_memcpy_(state->vault_b.bytes, ctx->vault_b->key->bytes, 32);
    sol_memcpy_(state->lp_mint.bytes, ctx->lp_mint->key->bytes, 32);

    return SUCCESS;
}
