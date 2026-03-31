#include "pool.h"

uint64_t swap(
    swap_accounts_t *ctx, swap_args_t *args, Parameters *params
) {
    if (args->amount_in == 0)
        return AMM_ERROR_ZERO_AMOUNT;
    if (args->direction > 1)
        return AMM_ERROR_INVALID_DIRECTION;

    PoolState *state = ACCOUNT_STATE(ctx->pool_state, PoolState);
    if (!state->is_initialized)
        return AMM_ERROR_NOT_INITIALIZED;

    TokenAccount *va = TOKEN_ACCOUNT(ctx->vault_a);
    TokenAccount *vb = TOKEN_ACCOUNT(ctx->vault_b);

    uint64_t reserve_in, reserve_out;
    AccountInfo *vault_in, *vault_out, *user_in, *user_out;

    if (args->direction == 0) {
        reserve_in  = va->amount;
        reserve_out = vb->amount;
        user_in     = ctx->user_token_a;
        vault_in    = ctx->vault_a;
        user_out    = ctx->user_token_b;
        vault_out   = ctx->vault_b;
    } else {
        reserve_in  = vb->amount;
        reserve_out = va->amount;
        user_in     = ctx->user_token_b;
        vault_in    = ctx->vault_b;
        user_out    = ctx->user_token_a;
        vault_out   = ctx->vault_a;
    }

    uint64_t amount_in_fee = args->amount_in * 997;
    uint64_t denom = reserve_in * 1000 + amount_in_fee;
    uint64_t amount_out = mul_div_u64(reserve_out, amount_in_fee, denom);

    if (amount_out < args->min_amount_out)
        return AMM_ERROR_SLIPPAGE_EXCEEDED;

    SignerSeed pool_seeds[] = {
        SEED_STR("amm"),
        SEED_PUBKEY(&state->mint_a),
        SEED_PUBKEY(&state->mint_b),
        SEED_U8(&state->bump),
    };
    SignerSeeds pool_signer = { .seeds = pool_seeds, .len = 4 };

    TRY(token_transfer(
        user_in, vault_in, ctx->user, args->amount_in,
        params->accounts, (int)params->accounts_len
    ));

    TRY(token_transfer_signed(
        vault_out, user_out, ctx->pool_state, amount_out,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    return SUCCESS;
}
