#include "pool.h"

/* @cvl:instruction swap 3 */
/* @cvl:args amount_in:u64 min_amount_out:u64 direction:u8 */

#define SWAP_ACCOUNTS(X) \
    X(user,          CVL_SIGNER) \
    X(pool_state,    0) \
    X(vault_a,       CVL_WRITABLE) \
    X(vault_b,       CVL_WRITABLE) \
    X(user_token_a,  CVL_WRITABLE) \
    X(user_token_b,  CVL_WRITABLE) \
    X(token_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(swap, SWAP_ACCOUNTS)

uint64_t handle_swap(CvlParameters *params) {
    swap_accounts_t ctx;
    CVL_TRY(swap_parse(params, &ctx));

    /* Parse: disc(1) + amount_in(8) + min_amount_out(8) + direction(1) = 18 */
    if (params->data_len < 18) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount_in      = *(uint64_t *)(params->data + 1);
    uint64_t min_amount_out  = *(uint64_t *)(params->data + 9);
    uint8_t  direction       = params->data[17];

    if (amount_in == 0) {
        return AMM_ERROR_ZERO_AMOUNT;
    }
    if (direction > 1) {
        return AMM_ERROR_INVALID_DIRECTION;
    }

    PoolState *state = CVL_ACCOUNT_STATE(ctx.pool_state, PoolState);
    if (!state->is_initialized) {
        return AMM_ERROR_NOT_INITIALIZED;
    }

    CvlTokenAccount *va = CVL_TOKEN_ACCOUNT(ctx.vault_a);
    CvlTokenAccount *vb = CVL_TOKEN_ACCOUNT(ctx.vault_b);

    uint64_t reserve_in, reserve_out;
    CvlAccountInfo *vault_in, *vault_out, *user_in, *user_out;

    if (direction == 0) {
        reserve_in  = va->amount;
        reserve_out = vb->amount;
        user_in     = ctx.user_token_a;
        vault_in    = ctx.vault_a;
        user_out    = ctx.user_token_b;
        vault_out   = ctx.vault_b;
    } else {
        reserve_in  = vb->amount;
        reserve_out = va->amount;
        user_in     = ctx.user_token_b;
        vault_in    = ctx.vault_b;
        user_out    = ctx.user_token_a;
        vault_out   = ctx.vault_a;
    }

    /* out = (reserve_out * amount_in * 997) / (reserve_in * 1000 + amount_in * 997) */
    uint64_t amount_in_fee = amount_in * 997;
    uint64_t denom = reserve_in * 1000 + amount_in_fee;
    uint64_t amount_out = mul_div_u64(reserve_out, amount_in_fee, denom);

    if (amount_out < min_amount_out) {
        return AMM_ERROR_SLIPPAGE_EXCEEDED;
    }

    CvlSignerSeed pool_seeds[] = {
        CVL_SEED_STR("amm"),
        CVL_SEED_PUBKEY(&state->mint_a),
        CVL_SEED_PUBKEY(&state->mint_b),
        CVL_SEED_U8(&state->bump),
    };
    CvlSignerSeeds pool_signer = { .seeds = pool_seeds, .len = 4 };

    CVL_TRY(cvl_token_transfer(
        user_in, vault_in, ctx.user, amount_in,
        params->accounts, (int)params->accounts_len
    ));

    CVL_TRY(cvl_token_transfer_signed(
        vault_out, user_out, ctx.pool_state, amount_out,
        params->accounts, (int)params->accounts_len,
        &pool_signer, 1
    ));

    return CVL_SUCCESS;
}
