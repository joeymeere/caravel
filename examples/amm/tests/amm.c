#include <caravel/test.h>

#include "../src/pool.h"

static Keypair user, mint_a, mint_b, lp_mint;
static Pubkey pool, vault_a, vault_b, user_a, user_b, user_lp;
static uint8_t pool_bump;

SETUP {
    user    = keypair(1);
    mint_a  = keypair(2);
    mint_b  = keypair(3);
    lp_mint = keypair(4);
    svm_airdrop(svm, &user.pubkey, SOL(100));

    pool = FIND_PDA(
        &PROGRAM_ID, &pool_bump, SEED_STR("amm"), SEED_PUBKEY(&mint_a.pubkey),
        SEED_PUBKEY(&mint_b.pubkey)
    );

    svm_create_mint(svm, &user, &mint_a, &user.pubkey, 6);
    svm_create_mint(svm, &user, &mint_b, &user.pubkey, 6);
    svm_create_mint(svm, &user, &lp_mint, &pool, 6);

    user_a  = svm_create_ata(svm, &user, &user.pubkey, &mint_a.pubkey);
    user_b  = svm_create_ata(svm, &user, &user.pubkey, &mint_b.pubkey);
    vault_a = svm_create_ata(svm, &user, &pool, &mint_a.pubkey);
    vault_b = svm_create_ata(svm, &user, &pool, &mint_b.pubkey);
    user_lp = svm_create_ata(svm, &user, &user.pubkey, &lp_mint.pubkey);

    svm_mint_to(svm, &mint_a.pubkey, &user_a, &user, 10000000000000000ULL);
    svm_mint_to(svm, &mint_b.pubkey, &user_b, &user, 10000000000000000ULL);
}

static Ix swap_ix(uint64_t amount_in, uint64_t min_out, uint8_t direction) {
    Ix ix = MAKE_IX(
        swap, .user = user.pubkey, .pool_state = pool, .vault_a = vault_a,
        .vault_b = vault_b, .user_token_a = user_a, .user_token_b = user_b,
        .token_program = TOKEN_PROGRAM_ID
    );
    IX_ARGS(
        &ix, swap, .amount_in = amount_in, .min_amount_out = min_out,
        .direction = direction
    );
    return ix;
}

TEST(initializes_the_pool) {
    Ix ix = MAKE_IX(
        initialize_pool, .payer = user.pubkey, .pool_state = pool,
        .vault_a = vault_a, .vault_b = vault_b, .lp_mint = lp_mint.pubkey,
        .mint_a = mint_a.pubkey, .mint_b = mint_b.pubkey,
        .system_program = SYSTEM_PROGRAM_ID
    );
    TxResult r = SEND(svm, ix, &user);
    ASSERT_OK(r);
    report_cu("initialize_pool", r);

    PoolState state = READ_ACCOUNT(svm, &pool, PoolState);
    ASSERT_EQ(state.is_initialized, 1);
    ASSERT_EQ(state.bump, pool_bump);
    ASSERT_KEY_EQ(&state.mint_a, &mint_a.pubkey);
    ASSERT_KEY_EQ(&state.mint_b, &mint_b.pubkey);
}

TEST(adds_initial_liquidity) {
    Ix ix = MAKE_IX(
        add_liquidity, .user = user.pubkey, .pool_state = pool,
        .vault_a = vault_a, .vault_b = vault_b, .user_token_a = user_a,
        .user_token_b = user_b, .lp_mint = lp_mint.pubkey, .user_lp = user_lp,
        .token_program = TOKEN_PROGRAM_ID
    );
    IX_ARGS(
        &ix, add_liquidity, .amount_a = 100000000000000ULL,
        .amount_b = 400000000000000ULL, .min_lp = 0
    );
    TxResult r = SEND(svm, ix, &user);
    ASSERT_OK(r);
    report_cu("add_liquidity", r);

    ASSERT_EQ(TOKEN_BALANCE(svm, &user_lp), 200000000000000ULL);
    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_a), 100000000000000ULL);
    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_b), 400000000000000ULL);
}

TEST(swaps_a_to_b) {
    Ix ix      = swap_ix(10000000000000ULL, 0, 0);
    TxResult r = SEND(svm, ix, &user);
    ASSERT_OK(r);
    report_cu("swap A->B", r);

    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_a), 110000000000000ULL);
    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_b), 363735564244795ULL);
}

TEST(swaps_b_to_a) {
    Ix ix      = swap_ix(20000000000000ULL, 0, 1);
    TxResult r = SEND(svm, ix, &user);
    ASSERT_OK(r);
    report_cu("swap B->A", r);

    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_a), 104283190788245ULL);
    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_b), 383735564244795ULL);
}

TEST(removes_lp) {
    Ix ix = MAKE_IX(
        remove_liquidity, .user = user.pubkey, .pool_state = pool,
        .vault_a = vault_a, .vault_b = vault_b, .user_token_a = user_a,
        .user_token_b = user_b, .lp_mint = lp_mint.pubkey, .user_lp = user_lp,
        .token_program = TOKEN_PROGRAM_ID
    );
    IX_ARGS(
        &ix, remove_liquidity, .lp_amount = 50000000000000ULL, .min_a = 0,
        .min_b = 0
    );
    TxResult r = SEND(svm, ix, &user);
    ASSERT_OK(r);
    report_cu("remove_liquidity", r);

    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_a), 78212393091184ULL);
    ASSERT_EQ(TOKEN_BALANCE(svm, &vault_b), 287801673183597ULL);
    ASSERT_EQ(MINT_SUPPLY(svm, &lp_mint.pubkey), 150000000000000ULL);
}

TEST(rejects_swap_with_excessive_slippage) {
    Ix ix = swap_ix(10000000000000ULL, 999999999999999ULL, 0);
    ASSERT_ERR(SEND(svm, ix, &user), AMM_ERROR_SLIPPAGE_EXCEEDED);
}
