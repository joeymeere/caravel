#include <caravel.h>
#include "instructions.h"

static uint64_t
deposit(deposit_accounts_t *ctx, deposit_args_t *args, Parameters *params) {
    SignerSeed seeds[] = {
        SEED_STR("vault"),
        SEED_PUBKEY(ctx->user->key),
    };
    Pubkey expected;
    uint8_t bump;
    TRY(find_program_address(seeds, 2, params->program_id, &expected, &bump));

    if (UNLIKELY(!pubkey_eq(ctx->vault->key, &expected)))
        return ERROR_INVALID_PDA;

    TRY(system_transfer(
        ctx->user, ctx->vault, args->amount, params->accounts,
        (int)params->accounts_len
    ));

    return SUCCESS;
}

static uint64_t
withdraw(withdraw_accounts_t *ctx, withdraw_args_t *args, Parameters *params) {
    SignerSeed seeds[] = {
        SEED_STR("vault"),
        SEED_PUBKEY(ctx->user->key),
    };
    Pubkey expected;
    uint8_t bump;
    TRY(find_program_address(seeds, 2, params->program_id, &expected, &bump));

    if (UNLIKELY(!pubkey_eq(ctx->vault->key, &expected)))
        return ERROR_INVALID_PDA;

    *ctx->vault->lamports -= args->amount;
    *ctx->user->lamports += args->amount;

    return SUCCESS;
}

ENTRYPOINT(HANDLER(deposit) HANDLER(withdraw))
