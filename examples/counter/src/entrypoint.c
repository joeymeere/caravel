#include <caravel.h>
#include "state.h"

#define INIT_ACCOUNTS(X) \
    X(payer,          SIGNER | WRITABLE) \
    X(counter,        SIGNER | WRITABLE) \
    X(system_program, PROGRAM)

IX(0, initialize, INIT_ACCOUNTS)

static uint64_t initialize(
    initialize_accounts_t *ctx, initialize_args_t *args, Parameters *params
) {
    (void)args;

    Rent rent;
    get_rent(&rent);
    uint64_t lamports = minimum_balance(&rent, sizeof(CounterState));

    TRY(system_create_account(
        ctx->payer, ctx->counter,
        lamports, sizeof(CounterState), params->program_id,
        params->accounts, (int)params->accounts_len
    ));

    CounterState *state = ACCOUNT_STATE(ctx->counter, CounterState);
    state->count = 0;
    sol_memcpy_(state->authority.bytes, ctx->payer->key->bytes, 32);

    return SUCCESS;
}

#define INC_ACCOUNTS(X) \
    X(counter,   WRITABLE) \
    X(authority, SIGNER)

IX(1, increment, INC_ACCOUNTS)

static uint64_t increment(
    increment_accounts_t *ctx, increment_args_t *args, Parameters *params
) {
    (void)args; (void)params;

    CounterState *state = ACCOUNT_STATE(ctx->counter, CounterState);

    if (!pubkey_eq(&state->authority, ctx->authority->key))
        return ERROR_INVALID_ARGUMENT;

    state->count += 1;
    return SUCCESS;
}

#define DEC_ACCOUNTS(X) \
    X(counter,   WRITABLE) \
    X(authority, SIGNER)

IX(2, decrement, DEC_ACCOUNTS)

static uint64_t decrement(
    decrement_accounts_t *ctx, decrement_args_t *args, Parameters *params
) {
    (void)args; (void)params;

    CounterState *state = ACCOUNT_STATE(ctx->counter, CounterState);

    if (!pubkey_eq(&state->authority, ctx->authority->key))
        return ERROR_INVALID_ARGUMENT;

    if (state->count == 0)
        return ERROR_INVALID_ARGUMENT;

    state->count -= 1;
    return SUCCESS;
}

ENTRYPOINT(
    HANDLER(initialize)
    HANDLER(increment)
    HANDLER(decrement)
)
