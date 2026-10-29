#define CARAVEL_TEST_MOLLUSK
#include <caravel/test.h>

#include "../src/instructions.h"
#include "../src/state.h"

static Pubkey payer, counter;

SETUP {
    payer   = keypair(1).pubkey;
    counter = keypair(2).pubkey;
    ctx_airdrop(ctx, &payer, SOL(10));
}

TEST(initializes_a_counter) {
    Ix ix = MAKE_IX(
        initialize, .payer = payer, .counter = counter,
        .system_program = SYSTEM_PROGRAM_ID
    );
    IxResult r = PROCESS(ctx, ix);
    ASSERT_OK(r);
    report_cu("mollusk initialize", r);

    CounterState state = READ_ACCOUNT(ctx, &counter, CounterState);
    ASSERT_EQ(state.count, 0);
    ASSERT_KEY_EQ(&state.authority, &payer);
}

TEST(increments_the_counter) {
    Ix ix      = MAKE_IX(increment, .counter = counter, .authority = payer);
    IxResult r = PROCESS(ctx, ix);
    ASSERT_OK(r);
    report_cu("mollusk increment", r);

    ASSERT_EQ(READ_ACCOUNT(ctx, &counter, CounterState).count, 1);
}

TEST(decrements_the_counter) {
    Ix ix      = MAKE_IX(decrement, .counter = counter, .authority = payer);
    IxResult r = PROCESS(ctx, ix);
    ASSERT_OK(r);
    report_cu("mollusk decrement", r);

    ASSERT_EQ(READ_ACCOUNT(ctx, &counter, CounterState).count, 0);
}

TEST(rejects_decrement_when_counter_is_zero) {
    Ix ix = MAKE_IX(decrement, .counter = counter, .authority = payer);
    ASSERT_ERR(PROCESS(ctx, ix), ERROR_INVALID_ARGUMENT);
}

TEST(stores_a_chain_only_if_every_instruction_succeeds) {
    Ix ixs[] = {
        MAKE_IX(increment, .counter = counter, .authority = payer),
        MAKE_IX(decrement, .counter = counter, .authority = payer),
        MAKE_IX(decrement, .counter = counter, .authority = payer),
    };
    ASSERT_ERR(PROCESS_CHAIN(ctx, ixs), ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(READ_ACCOUNT(ctx, &counter, CounterState).count, 0);
}

TEST(reports_which_instruction_of_a_transaction_failed) {
    Ix ixs[] = {
        MAKE_IX(increment, .counter = counter, .authority = payer),
        MAKE_IX(decrement, .counter = counter, .authority = payer),
        MAKE_IX(decrement, .counter = counter, .authority = payer),
    };
    IxResult r = PROCESS_TX(ctx, ixs, &payer);
    ASSERT_ERR(r, ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(r.error.instruction_index, 2);
}

TEST(rejects_a_non_executable_account_in_a_program_slot) {
    Pubkey fake = keypair(3).pubkey, fresh = keypair(4).pubkey;
    Ix ix = MAKE_IX(
        initialize, .payer = payer, .counter = fresh, .system_program = fake
    );
    ASSERT_ERR(PROCESS(ctx, ix), ERROR_ACCOUNT_NOT_EXECUTABLE);
}

TEST(rejects_duplicate_accounts) {
    Ix ix = MAKE_IX(increment, .counter = payer, .authority = payer);
    ASSERT_ERR(PROCESS(ctx, ix), ERROR_DUPLICATE_ACCOUNT);
}

TEST(rejects_increment_from_wrong_authority) {
    Pubkey wrong = keypair(5).pubkey;
    Ix ix        = MAKE_IX(increment, .counter = counter, .authority = wrong);
    ASSERT_ERR(PROCESS(ctx, ix), ERROR_INVALID_ARGUMENT);
}

TEST(runs_out_of_compute_units) {
    ctx_set_compute_unit_limit(ctx, 10);
    Ix ix      = MAKE_IX(increment, .counter = counter, .authority = payer);
    IxResult r = PROCESS(ctx, ix);
    ASSERT_FAILS(r);
    ASSERT_EQ(r.error.code, TEST_IX_ERR_PROGRAM_FAILED_TO_COMPLETE);
}
