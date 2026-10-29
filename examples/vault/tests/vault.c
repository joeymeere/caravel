#define CARAVEL_TEST_PROGRAM_ID "33333333333333333333333333333333333333333333"
#include <caravel/test.h>

#include "../src/instructions.h"

static Keypair user;
static Pubkey vault;

SETUP {
    user = keypair(1);
    svm_airdrop(svm, &user.pubkey, SOL(10));

    vault = FIND_PDA(
        &PROGRAM_ID, NULL, SEED_STR("vault"), SEED_PUBKEY(&user.pubkey)
    );
    svm_set_account(svm, &vault, svm_rent_exempt(svm, 0), &PROGRAM_ID, NULL, 0);
}

static TxResult deposit(Svm *svm, uint64_t amount) {
    Ix ix = MAKE_IX(
        deposit, .user = user.pubkey, .vault = vault,
        .system_program = SYSTEM_PROGRAM_ID
    );
    IX_ARGS(&ix, deposit, .amount = amount);
    return SEND(svm, ix, &user);
}

TEST(deposits_sol_into_the_vault_initial) {
    TxResult r = deposit(svm, SOL(1));
    ASSERT_OK(r);
    report_cu("deposit (initial)", r);
    ASSERT(svm_lamports(svm, &vault) >= SOL(1));
}

TEST(deposits_sol_into_the_vault_subsequent) {
    TxResult r = deposit(svm, SOL(2));
    ASSERT_OK(r);
    report_cu("deposit (subsequent)", r);
    ASSERT(svm_lamports(svm, &vault) >= SOL(3));
}

TEST(withdraws_sol_from_the_vault) {
    uint64_t before = svm_lamports(svm, &vault);

    Ix ix = MAKE_IX(withdraw, .user = user.pubkey, .vault = vault);
    IX_ARGS(&ix, withdraw, .amount = SOL(1) / 2);
    TxResult r = SEND(svm, ix, &user);
    ASSERT_OK(r);
    report_cu("withdraw", r);

    ASSERT_EQ(before - svm_lamports(svm, &vault), SOL(1) / 2);
}

TEST(rejects_withdrawal_from_wrong_authority) {
    Keypair wrong = keypair(2);
    svm_airdrop(svm, &wrong.pubkey, SOL(1));

    Ix ix = MAKE_IX(withdraw, .user = wrong.pubkey, .vault = vault);
    IX_ARGS(&ix, withdraw, .amount = SOL(1) / 2);
    ASSERT_ERR(SEND(svm, ix, &wrong), ERROR_INVALID_PDA);
}
