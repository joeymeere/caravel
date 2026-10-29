#define CARAVEL_TEST_SEASHELL
#include <caravel/test.h>

#include "../src/instructions.h"
#include "../src/state.h"

static Pubkey payer, counter;

SETUP {
    payer   = keypair(1).pubkey;
    counter = keypair(2).pubkey;
    shell_airdrop(shell, &payer, SOL(10));
}

TEST(initializes_a_counter) {
    Ix ix = MAKE_IX(
        initialize, .payer = payer, .counter = counter,
        .system_program = SYSTEM_PROGRAM_ID
    );
    IxResult r = PROCESS(shell, ix);
    ASSERT_OK(r);
    report_cu("seashell initialize", r);

    CounterState state = READ_ACCOUNT(shell, &counter, CounterState);
    ASSERT_EQ(state.count, 0);
    ASSERT_KEY_EQ(&state.authority, &payer);
}

TEST(increments_the_counter) {
    Ix ix      = MAKE_IX(increment, .counter = counter, .authority = payer);
    IxResult r = PROCESS(shell, ix);
    ASSERT_OK(r);
    report_cu("seashell increment", r);

    ASSERT_EQ(READ_ACCOUNT(shell, &counter, CounterState).count, 1);
}

TEST(decrements_the_counter) {
    Ix ix      = MAKE_IX(decrement, .counter = counter, .authority = payer);
    IxResult r = PROCESS(shell, ix);
    ASSERT_OK(r);
    report_cu("seashell decrement", r);

    ASSERT_EQ(READ_ACCOUNT(shell, &counter, CounterState).count, 0);
}

TEST(rejects_decrement_when_counter_is_zero) {
    Ix ix = MAKE_IX(decrement, .counter = counter, .authority = payer);
    ASSERT_ERR(PROCESS(shell, ix), ERROR_INVALID_ARGUMENT);
}

TEST(rejects_a_non_executable_account_in_a_program_slot) {
    Pubkey fake = keypair(3).pubkey, fresh = keypair(4).pubkey;
    Ix ix = MAKE_IX(
        initialize, .payer = payer, .counter = fresh, .system_program = fake
    );
    ASSERT_ERR(PROCESS(shell, ix), ERROR_ACCOUNT_NOT_EXECUTABLE);
}

TEST(rejects_duplicate_accounts) {
    Ix ix = MAKE_IX(increment, .counter = payer, .authority = payer);
    ASSERT_ERR(PROCESS(shell, ix), ERROR_DUPLICATE_ACCOUNT);
}

TEST(rejects_increment_from_wrong_authority) {
    Pubkey wrong = keypair(5).pubkey;
    Ix ix        = MAKE_IX(increment, .counter = counter, .authority = wrong);
    ASSERT_ERR(PROCESS(shell, ix), ERROR_INVALID_ARGUMENT);
}

TEST(runs_out_of_compute_units) {
    shell_set_compute_unit_limit(shell, 10);
    Ix ix      = MAKE_IX(increment, .counter = counter, .authority = payer);
    IxResult r = PROCESS(shell, ix);
    ASSERT_FAILS(r);
    ASSERT_EQ(r.error.code, TEST_IX_ERR_PROGRAM_FAILED_TO_COMPLETE);
}
