#include "pool.h"

uint64_t add_liquidity(
    add_liquidity_accounts_t *ctx, add_liquidity_args_t *args,
    Parameters *params
) {
    if (args->amount_a == 0 || args->amount_b == 0)
        return AMM_ERROR_ZERO_AMOUNT;

    PoolState *state = ACCOUNT_STATE(ctx->pool_state, PoolState);
    if (!state->is_initialized)
        return AMM_ERROR_NOT_INITIALIZED;

    TokenAccount *va = TOKEN_ACCOUNT(ctx->vault_a);
    TokenAccount *vb = TOKEN_ACCOUNT(ctx->vault_b);
    uint64_t reserve_a = va->amount;
    uint64_t reserve_b = vb->amount;

    MintAccount *lp_mint = MINT_ACCOUNT(ctx->lp_mint);
    uint64_t supply = lp_mint->supply;

    uint64_t lp_amount;
    if (supply == 0) {
        lp_amount = isqrt_mul(args->amount_a, args->amount_b);
    } else {
        uint64_t lp_a = mul_div_u64(args->amount_a, supply, reserve_a);
        uint64_t lp_b = mul_div_u64(args->amount_b, supply, reserve_b);
        lp_amount = lp_a < lp_b ? lp_a : lp_b;
    }

    if (lp_amount == 0)
        return AMM_ERROR_ZERO_AMOUNT;
    if (lp_amount < args->min_lp)
        return AMM_ERROR_SLIPPAGE_EXCEEDED;

    SignerSeed pool_seeds[] = {
        SEED_STR("amm"),
        SEED_PUBKEY(&state->mint_a),
        SEED_PUBKEY(&state->mint_b),
        SEED_U8(&state->bump),
    };
    SignerSeeds pool_signer = { .seeds = pool_seeds, .len = 4 };

    TRY(token_transfer(
        ctx->user_token_a, ctx->vault_a, ctx->user, args->amount_a,
        params->accounts, (int)params->accounts_len
    ));

    TRY(token_transfer(
        ctx->user_token_b, ctx->vault_b, ctx->user, args->amount_b,
        params->accounts, (int)params->accounts_len
    ));

    TRY(token_mint_to_signed(
        ctx->lp_mint, ctx->user_lp, ctx->pool_state, lp_amount,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    return SUCCESS;
}

uint64_t remove_liquidity(
    remove_liquidity_accounts_t *ctx, remove_liquidity_args_t *args,
    Parameters *params
) {
    if (args->lp_amount == 0)
        return AMM_ERROR_ZERO_AMOUNT;

    PoolState *state = ACCOUNT_STATE(ctx->pool_state, PoolState);
    if (!state->is_initialized)
        return AMM_ERROR_NOT_INITIALIZED;

    TokenAccount *va = TOKEN_ACCOUNT(ctx->vault_a);
    TokenAccount *vb = TOKEN_ACCOUNT(ctx->vault_b);
    uint64_t reserve_a = va->amount;
    uint64_t reserve_b = vb->amount;

    MintAccount *lp_mint_data = MINT_ACCOUNT(ctx->lp_mint);
    uint64_t supply = lp_mint_data->supply;

    if (args->lp_amount > supply)
        return AMM_ERROR_INSUFFICIENT_LP;

    uint64_t amount_a = mul_div_u64(args->lp_amount, reserve_a, supply);
    uint64_t amount_b = mul_div_u64(args->lp_amount, reserve_b, supply);

    if (amount_a < args->min_a || amount_b < args->min_b)
        return AMM_ERROR_SLIPPAGE_EXCEEDED;

    SignerSeed pool_seeds[] = {
        SEED_STR("amm"),
        SEED_PUBKEY(&state->mint_a),
        SEED_PUBKEY(&state->mint_b),
        SEED_U8(&state->bump),
    };
    SignerSeeds pool_signer = { .seeds = pool_seeds, .len = 4 };

    TRY(token_burn(
        ctx->user_lp, ctx->lp_mint, ctx->user, args->lp_amount,
        params->accounts, (int)params->accounts_len
    ));

    TRY(token_transfer_signed(
        ctx->vault_a, ctx->user_token_a, ctx->pool_state, amount_a,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    TRY(token_transfer_signed(
        ctx->vault_b, ctx->user_token_b, ctx->pool_state, amount_b,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    return SUCCESS;
}
