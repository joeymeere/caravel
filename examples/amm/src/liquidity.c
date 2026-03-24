#include "pool.h"

/* @cvl:instruction add_liquidity 1 */
/* @cvl:args amount_a:u64 amount_b:u64 min_lp:u64 */
/* @cvl:instruction remove_liquidity 2 */
/* @cvl:args lp_amount:u64 min_a:u64 min_b:u64 */

#define LIQUIDITY_ACCOUNTS(X) \
    X(user,          CVL_SIGNER) \
    X(pool_state,    0) \
    X(vault_a,       CVL_WRITABLE) \
    X(vault_b,       CVL_WRITABLE) \
    X(user_token_a,  CVL_WRITABLE) \
    X(user_token_b,  CVL_WRITABLE) \
    X(lp_mint,       CVL_WRITABLE) \
    X(user_lp,       CVL_WRITABLE) \
    X(token_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(liquidity, LIQUIDITY_ACCOUNTS)

uint64_t handle_add_liquidity(CvlParameters *params) {
    liquidity_accounts_t ctx;
    CVL_TRY(liquidity_parse(params, &ctx));

    /* Parse: disc(1) + amount_a(8) + amount_b(8) + min_lp(8) = 25 */
    if (params->data_len < 25) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount_a = *(uint64_t *)(params->data + 1);
    uint64_t amount_b = *(uint64_t *)(params->data + 9);
    uint64_t min_lp   = *(uint64_t *)(params->data + 17);

    if (amount_a == 0 || amount_b == 0) {
        return AMM_ERROR_ZERO_AMOUNT;
    }

    PoolState *state = CVL_ACCOUNT_STATE(ctx.pool_state, PoolState);
    if (!state->is_initialized) {
        return AMM_ERROR_NOT_INITIALIZED;
    }

    CvlTokenAccount *va = CVL_TOKEN_ACCOUNT(ctx.vault_a);
    CvlTokenAccount *vb = CVL_TOKEN_ACCOUNT(ctx.vault_b);
    uint64_t reserve_a = va->amount;
    uint64_t reserve_b = vb->amount;

    CvlMintAccount *lp_mint = CVL_MINT_ACCOUNT(ctx.lp_mint);
    uint64_t supply = lp_mint->supply;

    uint64_t lp_amount;
    if (supply == 0) {
        /* First deposit: lp = sqrt(amount_a * amount_b) */
        lp_amount = isqrt_mul(amount_a, amount_b);
    } else {
        /* Subsequent: lp = min(amount_a * supply / reserve_a,
                                amount_b * supply / reserve_b) */
        uint64_t lp_a = mul_div_u64(amount_a, supply, reserve_a);
        uint64_t lp_b = mul_div_u64(amount_b, supply, reserve_b);
        lp_amount = lp_a < lp_b ? lp_a : lp_b;
    }

    if (lp_amount == 0) {
        return AMM_ERROR_ZERO_AMOUNT;
    }
    if (lp_amount < min_lp) {
        return AMM_ERROR_SLIPPAGE_EXCEEDED;
    }

    CvlSignerSeed pool_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(&state->mint_a),
        CVL_SEED_PUBKEY(&state->mint_b),
        CVL_SEED_U8(&state->bump),
    };
    CvlSignerSeeds pool_signer = { .seeds = pool_seeds, .len = 4 };

    /* Transfer token A from user to vault */
    CVL_TRY(cvl_token_transfer(
        ctx.user_token_a, ctx.vault_a, ctx.user, amount_a,
        params->accounts, (int)params->accounts_len
    ));

    /* Transfer token B from user to vault */
    CVL_TRY(cvl_token_transfer(
        ctx.user_token_b, ctx.vault_b, ctx.user, amount_b,
        params->accounts, (int)params->accounts_len
    ));

    CVL_TRY(cvl_token_mint_to_signed(
        ctx.lp_mint, ctx.user_lp, ctx.pool_state, lp_amount,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    return CVL_SUCCESS;
}

uint64_t handle_remove_liquidity(CvlParameters *params) {
    liquidity_accounts_t ctx;
    CVL_TRY(liquidity_parse(params, &ctx));

    /* Parse: disc(1) + lp_amount(8) + min_a(8) + min_b(8) = 25 */
    if (params->data_len < 25) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t lp_amount = *(uint64_t *)(params->data + 1);
    uint64_t min_a     = *(uint64_t *)(params->data + 9);
    uint64_t min_b     = *(uint64_t *)(params->data + 17);

    if (lp_amount == 0) {
        return AMM_ERROR_ZERO_AMOUNT;
    }

    PoolState *state = CVL_ACCOUNT_STATE(ctx.pool_state, PoolState);
    if (!state->is_initialized) {
        return AMM_ERROR_NOT_INITIALIZED;
    }

    CvlTokenAccount *va = CVL_TOKEN_ACCOUNT(ctx.vault_a);
    CvlTokenAccount *vb = CVL_TOKEN_ACCOUNT(ctx.vault_b);
    uint64_t reserve_a = va->amount;
    uint64_t reserve_b = vb->amount;

    CvlMintAccount *lp_mint_data = CVL_MINT_ACCOUNT(ctx.lp_mint);
    uint64_t supply = lp_mint_data->supply;

    if (lp_amount > supply) {
        return AMM_ERROR_INSUFFICIENT_LP;
    }

    uint64_t amount_a = mul_div_u64(lp_amount, reserve_a, supply);
    uint64_t amount_b = mul_div_u64(lp_amount, reserve_b, supply);

    if (amount_a < min_a || amount_b < min_b) {
        return AMM_ERROR_SLIPPAGE_EXCEEDED;
    }

    CvlSignerSeed pool_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(&state->mint_a),
        CVL_SEED_PUBKEY(&state->mint_b),
        CVL_SEED_U8(&state->bump),
    };
    CvlSignerSeeds pool_signer = { .seeds = pool_seeds, .len = 4 };

    CVL_TRY(cvl_token_burn(
        ctx.user_lp, ctx.lp_mint, ctx.user, lp_amount,
        params->accounts, (int)params->accounts_len
    ));

    CVL_TRY(cvl_token_transfer_signed(
        ctx.vault_a, ctx.user_token_a, ctx.pool_state, amount_a,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    CVL_TRY(cvl_token_transfer_signed(
        ctx.vault_b, ctx.user_token_b, ctx.pool_state, amount_b,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    return CVL_SUCCESS;
}
