#include <caravel.h>
#include "state.h"

/* @cvl:instruction deposit 0 */

#define DEPOSIT_ACCOUNTS(X) \
    X(user,           CVL_SIGNER | CVL_WRITABLE) \
    X(vault,          CVL_WRITABLE) \
    X(vault_state,    CVL_WRITABLE) \
    X(system_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(deposit, DEPOSIT_ACCOUNTS)

static uint64_t handle_deposit(CvlParameters *params) {
    deposit_accounts_t ctx;
    CVL_TRY(deposit_parse(params, &ctx));

    if (params->data_len < 9) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount = *(uint64_t *)(params->data + 1);

    /* Derive vault PDA to get bump and verify address */
    CvlSignerSeed vault_derive[] = {
        CVL_SEED_STR("vault"),
        CVL_SEED_PUBKEY(ctx.user->key),
    };
    CvlPubkey vault_expected;
    uint8_t vault_bump;
    cvl_find_program_address(vault_derive, 2, params->program_id,
                             &vault_expected, &vault_bump);

    if (!cvl_pubkey_equals(ctx.vault->key, &vault_expected)) {
        cvl_log_literal("Error: vault PDA mismatch");
        return CVL_ERROR_INVALID_PDA;
    }

    /* Initialize vault_state on first deposit */
    if (ctx.vault_state->data_len == 0) {
        /* Derive vault_state PDA */
        CvlSignerSeed state_derive[] = {
            CVL_SEED_STR("vault_state"),
            CVL_SEED_PUBKEY(ctx.user->key),
        };
        CvlPubkey state_expected;
        uint8_t state_bump;
        cvl_find_program_address(state_derive, 2, params->program_id,
                                 &state_expected, &state_bump);

        /* Create vault_state PDA account */
        CvlRent rent;
        cvl_get_rent(&rent);
        uint64_t rent_lamports = cvl_minimum_balance(&rent, sizeof(VaultState));

        CvlSignerSeed state_signer_seeds[] = {
            CVL_SEED_STR("vault_state"),
            CVL_SEED_PUBKEY(ctx.user->key),
            CVL_SEED_U8(&state_bump),
        };
        CvlSignerSeeds state_signer = { .seeds = state_signer_seeds, .len = 3 };

        CVL_TRY(cvl_system_create_account_signed(
            ctx.user, ctx.vault_state,
            rent_lamports, sizeof(VaultState), params->program_id,
            params->accounts, (int)params->accounts_len,
            &state_signer, 1
        ));

        /* Store authority and vault bump */
        VaultState *state = CVL_ACCOUNT_STATE(ctx.vault_state, VaultState);
        sol_memcpy_(state->authority.bytes, ctx.user->key->bytes, 32);
        state->bump = vault_bump;
    }

    /* Transfer SOL from user to vault PDA */
    CVL_TRY(cvl_system_transfer(
        ctx.user, ctx.vault, amount,
        params->accounts, (int)params->accounts_len
    ));

    cvl_log_literal("Deposit successful");
    return CVL_SUCCESS;
}

/* @cvl:instruction withdraw 1 */

#define WITHDRAW_ACCOUNTS(X) \
    X(user,           CVL_SIGNER | CVL_WRITABLE) \
    X(vault,          CVL_WRITABLE) \
    X(vault_state,    0) \
    X(system_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(withdraw, WITHDRAW_ACCOUNTS)

static uint64_t handle_withdraw(CvlParameters *params) {
    withdraw_accounts_t ctx;
    CVL_TRY(withdraw_parse(params, &ctx));

    if (params->data_len < 9) {
        cvl_log_literal("Error: instruction data too short");
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount = *(uint64_t *)(params->data + 1);

    /* Verify authority */
    VaultState *state = CVL_ACCOUNT_STATE(ctx.vault_state, VaultState);
    if (!cvl_pubkey_equals(&state->authority, ctx.user->key)) {
        cvl_log_literal("Error: authority mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    /* Transfer SOL from vault PDA back to user, signed with PDA seeds */
    CvlSignerSeed vault_signer_seeds[] = {
        CVL_SEED_STR("vault"),
        CVL_SEED_PUBKEY(ctx.user->key),
        CVL_SEED_U8(&state->bump),
    };
    CvlSignerSeeds vault_signer = { .seeds = vault_signer_seeds, .len = 3 };

    CVL_TRY(cvl_system_transfer_signed(
        ctx.vault, ctx.user, amount,
        params->accounts, (int)params->accounts_len,
        &vault_signer, 1
    ));

    cvl_log_literal("Withdrawal successful");
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
