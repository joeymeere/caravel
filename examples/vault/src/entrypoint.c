#include <caravel.h>

typedef struct __attribute__((packed)) {
    uint64_t amount;
} deposit_args_t;

#define DEPOSIT_ACCOUNTS(X) \
    X(user,           SIGNER | WRITABLE) \
    X(vault,          WRITABLE) \
    X(system_program, PROGRAM)

IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)

static uint64_t deposit(
    deposit_accounts_t *ctx, deposit_args_t *args, Parameters *params
) {
    SignerSeed seeds[] = {
        SEED_STR("vault"),
        SEED_PUBKEY(ctx->user->key),
    };
    Pubkey expected;
    uint8_t bump;
    TRY(find_program_address(
        seeds, 2, params->program_id, &expected, &bump));

    if (UNLIKELY(!pubkey_eq(ctx->vault->key, &expected)))
        return ERROR_INVALID_PDA;

    TRY(system_transfer(
        ctx->user, ctx->vault, args->amount,
        params->accounts, (int)params->accounts_len
    ));

    return SUCCESS;
}

typedef struct __attribute__((packed)) {
    uint64_t amount;
} withdraw_args_t;

#define WITHDRAW_ACCOUNTS(X) \
    X(user,  SIGNER | WRITABLE) \
    X(vault, WRITABLE)

IX(1, withdraw, WITHDRAW_ACCOUNTS, withdraw_args_t)

static uint64_t withdraw(
    withdraw_accounts_t *ctx, withdraw_args_t *args, Parameters *params
) {
    SignerSeed seeds[] = {
        SEED_STR("vault"),
        SEED_PUBKEY(ctx->user->key),
    };
    Pubkey expected;
    uint8_t bump;
    TRY(find_program_address(
        seeds, 2, params->program_id, &expected, &bump));

    if (UNLIKELY(!pubkey_eq(ctx->vault->key, &expected)))
        return ERROR_INVALID_PDA;

    *ctx->vault->lamports -= args->amount;
    *ctx->user->lamports += args->amount;

    return SUCCESS;
}

ENTRYPOINT(
    HANDLER(deposit)
    HANDLER(withdraw)
)
