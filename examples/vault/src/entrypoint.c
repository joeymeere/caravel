#include <caravel.h>

typedef struct __attribute__((packed)) {
    uint64_t amount;
} deposit_args_t;

#define DEPOSIT_ACCOUNTS(X) \
    X(user,           CVL_SIGNER | CVL_WRITABLE) \
    X(vault,          CVL_WRITABLE) \
    X(system_program, CVL_PROGRAM)

CVL_IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)

static uint64_t deposit(
    deposit_accounts_t *ctx, deposit_args_t *args, CvlParameters *params
) {
    CvlSignerSeed seeds[] = {
        CVL_SEED_STR("vault"),
        CVL_SEED_PUBKEY(ctx->user->key),
    };
    CvlPubkey expected;
    uint8_t bump;
    CVL_TRY(cvl_find_program_address(
        seeds, 2, params->program_id, &expected, &bump));

    if (CVL_UNLIKELY(!cvl_pubkey_eq(ctx->vault->key, &expected)))
        return CVL_ERROR_INVALID_PDA;

    CVL_TRY(cvl_system_transfer(
        ctx->user, ctx->vault, args->amount,
        params->accounts, (int)params->accounts_len
    ));

    return CVL_SUCCESS;
}

typedef struct __attribute__((packed)) {
    uint64_t amount;
} withdraw_args_t;

#define WITHDRAW_ACCOUNTS(X) \
    X(user,  CVL_SIGNER | CVL_WRITABLE) \
    X(vault, CVL_WRITABLE)

CVL_IX(1, withdraw, WITHDRAW_ACCOUNTS, withdraw_args_t)

static uint64_t withdraw(
    withdraw_accounts_t *ctx, withdraw_args_t *args, CvlParameters *params
) {
    CvlSignerSeed seeds[] = {
        CVL_SEED_STR("vault"),
        CVL_SEED_PUBKEY(ctx->user->key),
    };
    CvlPubkey expected;
    uint8_t bump;
    CVL_TRY(cvl_find_program_address(
        seeds, 2, params->program_id, &expected, &bump));

    if (CVL_UNLIKELY(!cvl_pubkey_eq(ctx->vault->key, &expected)))
        return CVL_ERROR_INVALID_PDA;

    *ctx->vault->lamports -= args->amount;
    *ctx->user->lamports += args->amount;

    return CVL_SUCCESS;
}

CVL_ENTRYPOINT(
    CVL_HANDLER(deposit)
    CVL_HANDLER(withdraw)
)
