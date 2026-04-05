#include <caravel.h>
#include "state.h"

typedef struct __attribute__((packed)) {
    uint64_t amount;
} deposit_args_t;

#define DEPOSIT_ACCOUNTS(X) \
    X(authority,      SIGNER) \
    X(user_token,     WRITABLE) \
    X(vault_token,    WRITABLE) \
    X(vault_state,    WRITABLE) \
    X(mint,           0) \
    X(system_program, PROGRAM) \
    X(token_program,  PROGRAM)

IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)

static uint64_t deposit(
    deposit_accounts_t *ctx, deposit_args_t *args, Parameters *params
) {
    SignerSeed state_derive[] = {
        SEED_STR("token_vault"),
        SEED_PUBKEY(ctx->mint->key),
        SEED_PUBKEY(ctx->authority->key),
    };
    Pubkey state_expected;
    uint8_t state_bump;
    find_program_address(state_derive, 3, params->program_id,
                             &state_expected, &state_bump);

    if (ctx->vault_state->data_len == 0) {
        Rent rent;
        get_rent(&rent);
        uint64_t rent_lamports = minimum_balance(&rent, sizeof(TokenVaultState));

        SignerSeed state_signer_seeds[] = {
            SEED_STR("token_vault"),
            SEED_PUBKEY(ctx->mint->key),
            SEED_PUBKEY(ctx->authority->key),
            SEED_U8(&state_bump),
        };
        SignerSeeds state_signer = { .seeds = state_signer_seeds, .len = 4 };

        TRY(system_create_account_signed(
            ctx->authority, ctx->vault_state,
            rent_lamports, sizeof(TokenVaultState), params->program_id,
            params->accounts, (int)params->accounts_len,
            &state_signer, 1
        ));

        TokenVaultState *state = ACCOUNT_STATE(ctx->vault_state, TokenVaultState);
        sol_memcpy_(state->authority.bytes, ctx->authority->key->bytes, 32);
        sol_memcpy_(state->mint.bytes, ctx->mint->key->bytes, 32);
        state->bump = state_bump;
    }

    if (!pubkey_eq(ctx->vault_state->key, &state_expected))
        return ERROR_INVALID_PDA;

    TRY(token_transfer(
        ctx->user_token, ctx->vault_token, ctx->authority, args->amount,
        params->accounts, (int)params->accounts_len
    ));

    return SUCCESS;
}

typedef struct __attribute__((packed)) {
    uint64_t amount;
} withdraw_args_t;

#define WITHDRAW_ACCOUNTS(X) \
    X(authority,     SIGNER) \
    X(user_token,    WRITABLE) \
    X(vault_token,   WRITABLE) \
    X(vault_state,   0) \
    X(mint,          0) \
    X(token_program, PROGRAM)

IX(1, withdraw, WITHDRAW_ACCOUNTS, withdraw_args_t)

static uint64_t withdraw(
    withdraw_accounts_t *ctx, withdraw_args_t *args, Parameters *params
) {
    TokenVaultState *state = ACCOUNT_STATE(ctx->vault_state, TokenVaultState);
    if (!pubkey_eq(&state->authority, ctx->authority->key))
        return ERROR_INVALID_ARGUMENT;
    if (!pubkey_eq(&state->mint, ctx->mint->key))
        return ERROR_INVALID_ARGUMENT;

    SignerSeed signer_seeds[] = {
        SEED_STR("token_vault"),
        SEED_PUBKEY(ctx->mint->key),
        SEED_PUBKEY(ctx->authority->key),
        SEED_U8(&state->bump),
    };
    SignerSeeds signer = { .seeds = signer_seeds, .len = 4 };

    TRY(token_transfer_signed(
        ctx->vault_token, ctx->user_token, ctx->vault_state, args->amount,
        params->accounts, (int)params->accounts_len,
        &signer, 1
    ));

    return SUCCESS;
}

static uint64_t process(Parameters *params) {
    DISPATCH(params,
        HANDLER(deposit)
        HANDLER(withdraw)
    );
    return ERROR_UNKNOWN_INSTRUCTION;
}

LEGACY_ENTRYPOINT(process);
