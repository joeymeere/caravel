#include <caravel.h>
#include "state.h"

typedef struct __attribute__((packed)) {
    uint64_t amount;
} deposit_args_t;

#define DEPOSIT_ACCOUNTS(X) \
    X(authority,      CVL_SIGNER) \
    X(user_token,     CVL_WRITABLE) \
    X(vault_token,    CVL_WRITABLE) \
    X(vault_state,    CVL_WRITABLE) \
    X(mint,           0) \
    X(system_program, CVL_PROGRAM) \
    X(token_program,  CVL_PROGRAM)

CVL_IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)

static uint64_t deposit(
    deposit_accounts_t *ctx, deposit_args_t *args, CvlParameters *params
) {
    CvlSignerSeed state_derive[] = {
        CVL_SEED_STR("token_vault"),
        CVL_SEED_PUBKEY(ctx->mint->key),
        CVL_SEED_PUBKEY(ctx->authority->key),
    };
    CvlPubkey state_expected;
    uint8_t state_bump;
    cvl_find_program_address(state_derive, 3, params->program_id,
                             &state_expected, &state_bump);

    if (ctx->vault_state->data_len == 0) {
        CvlRent rent;
        cvl_get_rent(&rent);
        uint64_t rent_lamports = cvl_minimum_balance(&rent, sizeof(TokenVaultState));

        CvlSignerSeed state_signer_seeds[] = {
            CVL_SEED_STR("token_vault"),
            CVL_SEED_PUBKEY(ctx->mint->key),
            CVL_SEED_PUBKEY(ctx->authority->key),
            CVL_SEED_U8(&state_bump),
        };
        CvlSignerSeeds state_signer = { .seeds = state_signer_seeds, .len = 4 };

        CVL_TRY(cvl_system_create_account_signed(
            ctx->authority, ctx->vault_state,
            rent_lamports, sizeof(TokenVaultState), params->program_id,
            params->accounts, (int)params->accounts_len,
            &state_signer, 1
        ));

        TokenVaultState *state = CVL_ACCOUNT_STATE(ctx->vault_state, TokenVaultState);
        sol_memcpy_(state->authority.bytes, ctx->authority->key->bytes, 32);
        sol_memcpy_(state->mint.bytes, ctx->mint->key->bytes, 32);
        state->bump = state_bump;
    }

    if (!cvl_pubkey_eq(ctx->vault_state->key, &state_expected))
        return CVL_ERROR_INVALID_PDA;

    CVL_TRY(cvl_token_transfer(
        ctx->user_token, ctx->vault_token, ctx->authority, args->amount,
        params->accounts, (int)params->accounts_len
    ));

    return CVL_SUCCESS;
}

typedef struct __attribute__((packed)) {
    uint64_t amount;
} withdraw_args_t;

#define WITHDRAW_ACCOUNTS(X) \
    X(authority,     CVL_SIGNER) \
    X(user_token,    CVL_WRITABLE) \
    X(vault_token,   CVL_WRITABLE) \
    X(vault_state,   0) \
    X(mint,          0) \
    X(token_program, CVL_PROGRAM)

CVL_IX(1, withdraw, WITHDRAW_ACCOUNTS, withdraw_args_t)

static uint64_t withdraw(
    withdraw_accounts_t *ctx, withdraw_args_t *args, CvlParameters *params
) {
    TokenVaultState *state = CVL_ACCOUNT_STATE(ctx->vault_state, TokenVaultState);
    if (!cvl_pubkey_eq(&state->authority, ctx->authority->key))
        return CVL_ERROR_INVALID_ARGUMENT;
    if (!cvl_pubkey_eq(&state->mint, ctx->mint->key))
        return CVL_ERROR_INVALID_ARGUMENT;

    CvlSignerSeed signer_seeds[] = {
        CVL_SEED_STR("token_vault"),
        CVL_SEED_PUBKEY(ctx->mint->key),
        CVL_SEED_PUBKEY(ctx->authority->key),
        CVL_SEED_U8(&state->bump),
    };
    CvlSignerSeeds signer = { .seeds = signer_seeds, .len = 4 };

    CVL_TRY(cvl_token_transfer_signed(
        ctx->vault_token, ctx->user_token, ctx->vault_state, args->amount,
        params->accounts, (int)params->accounts_len,
        &signer, 1
    ));

    return CVL_SUCCESS;
}

static uint64_t process(CvlParameters *params) {
    CVL_DISPATCH(params,
        CVL_HANDLER(deposit)
        CVL_HANDLER(withdraw)
    );
    return CVL_ERROR_UNKNOWN_INSTRUCTION;
}

CVL_LEGACY_ENTRYPOINT(process);
