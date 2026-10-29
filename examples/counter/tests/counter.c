#include <caravel/test.h>

#include "../src/instructions.h"
#include "../src/state.h"

static Keypair payer, counter;

SETUP {
    payer   = keypair(1);
    counter = keypair(2);
    svm_airdrop(svm, &payer.pubkey, SOL(10));
}

TEST(initializes_a_counter) {
    Ix ix = MAKE_IX(
        initialize, .payer = payer.pubkey, .counter = counter.pubkey,
        .system_program = SYSTEM_PROGRAM_ID
    );
    TxResult r = SEND(svm, ix, &payer, &counter);
    ASSERT_OK(r);
    report_cu("initialize", r);

    CounterState state = READ_ACCOUNT(svm, &counter.pubkey, CounterState);
    ASSERT_EQ(state.count, 0);
    ASSERT_KEY_EQ(&state.authority, &payer.pubkey);
}

TEST(increments_the_counter) {
    Ix ix = MAKE_IX(
        increment, .counter = counter.pubkey, .authority = payer.pubkey
    );
    TxResult r = SEND(svm, ix, &payer);
    ASSERT_OK(r);
    report_cu("increment", r);

    ASSERT_EQ(READ_ACCOUNT(svm, &counter.pubkey, CounterState).count, 1);
}

TEST(decrements_the_counter) {
    Ix ix = MAKE_IX(
        decrement, .counter = counter.pubkey, .authority = payer.pubkey
    );
    TxResult r = SEND(svm, ix, &payer);
    ASSERT_OK(r);
    report_cu("decrement", r);

    ASSERT_EQ(READ_ACCOUNT(svm, &counter.pubkey, CounterState).count, 0);
}

TEST(rejects_decrement_when_counter_is_zero) {
    Ix ix = MAKE_IX(
        decrement, .counter = counter.pubkey, .authority = payer.pubkey
    );
    ASSERT_ERR(SEND(svm, ix, &payer), ERROR_INVALID_ARGUMENT);
}

TEST(rejects_a_non_executable_account_in_a_program_slot) {
    Keypair fake = keypair(3), fresh = keypair(4);
    Ix ix = MAKE_IX(
        initialize, .payer = payer.pubkey, .counter = fresh.pubkey,
        .system_program = fake.pubkey
    );
    ASSERT_ERR(SEND(svm, ix, &payer, &fresh), ERROR_ACCOUNT_NOT_EXECUTABLE);
}

TEST(rejects_duplicate_accounts) {
    Ix ix =
        MAKE_IX(increment, .counter = payer.pubkey, .authority = payer.pubkey);
    ASSERT_ERR(SEND(svm, ix, &payer), ERROR_DUPLICATE_ACCOUNT);
}

TEST(rejects_increment_from_wrong_authority) {
    Keypair wrong = keypair(5);
    svm_airdrop(svm, &wrong.pubkey, SOL(1));

    Ix ix = MAKE_IX(
        increment, .counter = counter.pubkey, .authority = wrong.pubkey
    );
    ASSERT_ERR(SEND(svm, ix, &wrong), ERROR_INVALID_ARGUMENT);
}
