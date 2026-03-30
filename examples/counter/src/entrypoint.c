#include <caravel.h>
#include "state.h"

#define INIT_ACCOUNTS(X) \
    X(payer,          CVL_SIGNER | CVL_WRITABLE) \
    X(counter,        CVL_SIGNER | CVL_WRITABLE) \
    X(system_program, CVL_PROGRAM)

CVL_IX(0, initialize, INIT_ACCOUNTS)

static uint64_t initialize(
    initialize_accounts_t *ctx, initialize_args_t *args, CvlParameters *params
) {
    (void)args;

    CvlRent rent;
    cvl_get_rent(&rent);
    uint64_t lamports = cvl_minimum_balance(&rent, sizeof(CounterState));

    CVL_TRY(cvl_system_create_account(
        ctx->payer, ctx->counter,
        lamports, sizeof(CounterState), params->program_id,
        params->accounts, (int)params->accounts_len
    ));

    CounterState *state = CVL_ACCOUNT_STATE(ctx->counter, CounterState);
    state->count = 0;
    sol_memcpy_(state->authority.bytes, ctx->payer->key->bytes, 32);

    return CVL_SUCCESS;
}

#define INC_ACCOUNTS(X) \
    X(counter,   CVL_WRITABLE) \
    X(authority, CVL_SIGNER)

CVL_IX(1, increment, INC_ACCOUNTS)

static uint64_t increment(
    increment_accounts_t *ctx, increment_args_t *args, CvlParameters *params
) {
    (void)args; (void)params;

    CounterState *state = CVL_ACCOUNT_STATE(ctx->counter, CounterState);

    if (!cvl_pubkey_eq(&state->authority, ctx->authority->key))
        return CVL_ERROR_INVALID_ARGUMENT;

    state->count += 1;
    return CVL_SUCCESS;
}

#define DEC_ACCOUNTS(X) \
    X(counter,   CVL_WRITABLE) \
    X(authority, CVL_SIGNER)

CVL_IX(2, decrement, DEC_ACCOUNTS)

static uint64_t decrement(
    decrement_accounts_t *ctx, decrement_args_t *args, CvlParameters *params
) {
    (void)args; (void)params;

    CounterState *state = CVL_ACCOUNT_STATE(ctx->counter, CounterState);

    if (!cvl_pubkey_eq(&state->authority, ctx->authority->key))
        return CVL_ERROR_INVALID_ARGUMENT;

    if (state->count == 0)
        return CVL_ERROR_INVALID_ARGUMENT;

    state->count -= 1;
    return CVL_SUCCESS;
}

CVL_ENTRYPOINT(
    CVL_HANDLER(initialize)
    CVL_HANDLER(increment)
    CVL_HANDLER(decrement)
)
