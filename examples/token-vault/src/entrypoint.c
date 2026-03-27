#include <caravel.h>
#include "state.h"

/* @cvl:instruction deposit 0 */
/* @cvl:args amount:u64 */

#define DEPOSIT_ACCOUNTS(X) \
    X(authority,      CVL_SIGNER) \
    X(user_token,     CVL_WRITABLE) \
    X(vault_token,    CVL_WRITABLE) \
    X(vault_state,    CVL_WRITABLE) \
    X(mint,           0) \
    X(system_program, CVL_PROGRAM) \
    X(token_program,  CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(deposit, DEPOSIT_ACCOUNTS)

static uint64_t handle_deposit(CvlParameters *params) {
    deposit_accounts_t ctx;
    CVL_TRY(deposit_parse(params, &ctx));

    if (params->data_len < 9) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount = *(uint64_t *)(params->data + 1);

    CvlSignerSeed state_derive[] = {
        CVL_SEED_STR("token_vault"),
        CVL_SEED_PUBKEY(ctx.mint->key),
        CVL_SEED_PUBKEY(ctx.authority->key),
    };
    CvlPubkey state_expected;
    uint8_t state_bump;
    cvl_find_program_address(state_derive, 3, params->program_id,
                             &state_expected, &state_bump);

    if (ctx.vault_state->data_len == 0) {
        CvlRent rent;
        cvl_get_rent(&rent);
        uint64_t rent_lamports = cvl_minimum_balance(&rent, sizeof(TokenVaultState));

        CvlSignerSeed state_signer_seeds[] = {
            CVL_SEED_STR("token_vault"),
            CVL_SEED_PUBKEY(ctx.mint->key),
            CVL_SEED_PUBKEY(ctx.authority->key),
            CVL_SEED_U8(&state_bump),
        };
        CvlSignerSeeds state_signer = { .seeds = state_signer_seeds, .len = 4 };

        CVL_TRY(cvl_system_create_account_signed(
            ctx.authority, ctx.vault_state,
            rent_lamports, sizeof(TokenVaultState), params->program_id,
            params->accounts, (int)params->accounts_len,
            &state_signer, 1
        ));

        TokenVaultState *state = CVL_ACCOUNT_STATE(ctx.vault_state, TokenVaultState);
        sol_memcpy_(state->authority.bytes, ctx.authority->key->bytes, 32);
        sol_memcpy_(state->mint.bytes, ctx.mint->key->bytes, 32);
        state->bump = state_bump;
    }

    if (!cvl_pubkey_eq(ctx.vault_state->key, &state_expected)) {
        cvl_log_literal("Error: vault_state PDA mismatch");
        return CVL_ERROR_INVALID_PDA;
    }

    CVL_TRY(cvl_token_transfer(
        ctx.user_token, ctx.vault_token, ctx.authority, amount,
        params->accounts, (int)params->accounts_len
    ));

    cvl_log_literal("Token deposit successful");
    return CVL_SUCCESS;
}

/* @cvl:instruction withdraw 1 */
/* @cvl:args amount:u64 */

#define WITHDRAW_ACCOUNTS(X) \
    X(authority,     CVL_SIGNER) \
    X(user_token,    CVL_WRITABLE) \
    X(vault_token,   CVL_WRITABLE) \
    X(vault_state,   0) \
    X(mint,          0) \
    X(token_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(withdraw, WITHDRAW_ACCOUNTS)

static uint64_t handle_withdraw(CvlParameters *params) {
    withdraw_accounts_t ctx;
    CVL_TRY(withdraw_parse(params, &ctx));

    if (params->data_len < 9) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount = *(uint64_t *)(params->data + 1);

    TokenVaultState *state = CVL_ACCOUNT_STATE(ctx.vault_state, TokenVaultState);
    if (!cvl_pubkey_eq(&state->authority, ctx.authority->key)) {
        cvl_log_literal("Error: authority mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }
    if (!cvl_pubkey_eq(&state->mint, ctx.mint->key)) {
        cvl_log_literal("Error: mint mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    CvlSignerSeed signer_seeds[] = {
        CVL_SEED_STR("token_vault"),
        CVL_SEED_PUBKEY(ctx.mint->key),
        CVL_SEED_PUBKEY(ctx.authority->key),
        CVL_SEED_U8(&state->bump),
    };
    CvlSignerSeeds signer = { .seeds = signer_seeds, .len = 4 };

    CVL_TRY(cvl_token_transfer_signed(
        ctx.vault_token, ctx.user_token, ctx.vault_state, amount,
        params->accounts, (int)params->accounts_len,
        &signer, 1
    ));

    cvl_log_literal("Token withdrawal successful");
    return CVL_SUCCESS;
}

static uint64_t process(CvlParameters *params) {
    CVL_DISPATCH(params,
        CVL_INSTRUCTION(0, handle_deposit)
        CVL_INSTRUCTION(1, handle_withdraw)
    );
    return CVL_ERROR_UNKNOWN_INSTRUCTION;
}

CVL_ENTRYPOINT(process);
