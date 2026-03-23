#include <caravel.h>
#include "state.h"

/* @cvl:instruction initialize 0 */

#define INIT_ACCOUNTS(X) \
    X(payer,          CVL_SIGNER | CVL_WRITABLE) \
    X(counter,        CVL_SIGNER | CVL_WRITABLE) \
    X(system_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(initialize, INIT_ACCOUNTS)

static uint64_t handle_initialize(CvlParameters *params) {
    initialize_accounts_t ctx;
    CVL_TRY(initialize_parse(params, &ctx));

    CvlRent rent;
    cvl_get_rent(&rent);
    uint64_t lamports = cvl_minimum_balance(&rent, sizeof(CounterState));

    CVL_TRY(cvl_system_create_account(
        ctx.payer, ctx.counter,
        lamports, sizeof(CounterState), params->program_id,
        params->accounts, (int)params->accounts_len
    ));

    CounterState *state = CVL_ACCOUNT_STATE(ctx.counter, CounterState);
    state->count = 0;
    sol_memcpy_(state->authority.bytes, ctx.payer->key->bytes, 32);

    return CVL_SUCCESS;
}

/* @cvl:instruction increment 1 */

#define INC_ACCOUNTS(X) \
    X(counter,   CVL_WRITABLE) \
    X(authority, CVL_SIGNER)

CVL_DEFINE_ACCOUNTS(increment, INC_ACCOUNTS)

static uint64_t handle_increment(CvlParameters *params) {
    increment_accounts_t ctx;
    CVL_TRY(increment_parse(params, &ctx));

    CounterState *state = CVL_ACCOUNT_STATE(ctx.counter, CounterState);

    if (!cvl_pubkey_equals(&state->authority, ctx.authority->key)) {
        cvl_log_literal("Error: authority mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    state->count += 1;
    return CVL_SUCCESS;
}

/* @cvl:instruction decrement 2 */

#define DEC_ACCOUNTS(X) \
    X(counter,   CVL_WRITABLE) \
    X(authority, CVL_SIGNER)

CVL_DEFINE_ACCOUNTS(decrement, DEC_ACCOUNTS)

static uint64_t handle_decrement(CvlParameters *params) {
    decrement_accounts_t ctx;
    CVL_TRY(decrement_parse(params, &ctx));

    CounterState *state = CVL_ACCOUNT_STATE(ctx.counter, CounterState);

    if (!cvl_pubkey_equals(&state->authority, ctx.authority->key)) {
        cvl_log_literal("Error: authority mismatch");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    if (state->count == 0) {
        cvl_log_literal("Error: counter already at zero");
        return CVL_ERROR_INVALID_ARGUMENT;
    }

    state->count -= 1;
    return CVL_SUCCESS;
}

static uint64_t process(CvlParameters *params) {
    CVL_DISPATCH(params,
        CVL_INSTRUCTION(0, handle_initialize)
        CVL_INSTRUCTION(1, handle_increment)
        CVL_INSTRUCTION(2, handle_decrement)
    );
    return CVL_ERROR_UNKNOWN_INSTRUCTION;
}

CVL_ENTRYPOINT(process);
