#define CARAVEL_TEST_SEASHELL
#include <caravel/test.h>

#include "../src/pool.h"

#define USDC_DECIMALS 6
#define USDC(n)       ((uint64_t)(n) * 1000000ULL)

static Pubkey usdc, user, mint_b, lp_mint;
static Pubkey pool, vault_a, vault_b, user_a, user_b, user_lp;

SETUP {
    SCENARIO("usdc");

    usdc    = PUBKEY("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
    user    = PUBKEY("4A4Qm2UrqUf364fhWPqJDWQmt3fyhAsqQG3bHPn3ZDBS");
    mint_b  = PUBKEY("5LGp7ne3Aq2HFh7pQCLjoacBW3CLrcAUpKgmdAmBRwT9");
    lp_mint = PUBKEY("HrTisrtkiA21NsWs7dH7UhuXhBinyctPmBoAD7tyXz2f");
    shell_airdrop(shell, &user, SOL(100));

    pool = FIND_PDA(
        &PROGRAM_ID, NULL, SEED_STR("amm"), SEED_PUBKEY(&usdc),
        SEED_PUBKEY(&mint_b)
    );
    shell_create_mint(shell, &user, &mint_b, &user, USDC_DECIMALS);
    shell_create_mint(shell, &user, &lp_mint, &pool, USDC_DECIMALS);

    user_a  = shell_create_ata(shell, &user, &user, &usdc);
    user_b  = shell_create_ata(shell, &user, &user, &mint_b);
    vault_a = shell_create_ata(shell, &user, &pool, &usdc);
    vault_b = shell_create_ata(shell, &user, &pool, &mint_b);
    user_lp = shell_create_ata(shell, &user, &user, &lp_mint);
    shell_mint_to(shell, &mint_b, &user_b, &user, USDC(1000000));
}

TEST(reads_the_recorded_usdc_mint) {
    MintAccount mint = READ_ACCOUNT(shell, &usdc, MintAccount);
    ASSERT_EQ(mint.is_initialized, 1);
    ASSERT_EQ(mint.decimals, USDC_DECIMALS);
    ASSERT(mint.supply > USDC(1000000000)); /* over a billion */
}

TEST(mints_usdc_as_its_real_authority) {
    Pubkey authority = READ_ACCOUNT(shell, &usdc, MintAccount).mint_authority;
    shell_mint_to(shell, &usdc, &user_a, &authority, USDC(1000000));
    ASSERT_EQ(TOKEN_BALANCE(shell, &user_a), USDC(1000000));
}

TEST(initializes_a_usdc_pool) {
    Ix ix = MAKE_IX(
        initialize_pool, .payer = user, .pool_state = pool, .vault_a = vault_a,
        .vault_b = vault_b, .lp_mint = lp_mint, .mint_a = usdc,
        .mint_b = mint_b, .system_program = SYSTEM_PROGRAM_ID
    );
    IxResult r = PROCESS(shell, ix);
    ASSERT_OK(r);
    report_cu("seashell initialize_pool", r);

    PoolState state = READ_ACCOUNT(shell, &pool, PoolState);
    ASSERT_KEY_EQ(&state.mint_a, &usdc);
    ASSERT_KEY_EQ(&state.mint_b, &mint_b);
}

TEST(adds_usdc_liquidity) {
    Ix ix = MAKE_IX(
        add_liquidity, .user = user, .pool_state = pool, .vault_a = vault_a,
        .vault_b = vault_b, .user_token_a = user_a, .user_token_b = user_b,
        .lp_mint = lp_mint, .user_lp = user_lp,
        .token_program = TOKEN_PROGRAM_ID
    );
    IX_ARGS(
        &ix, add_liquidity, .amount_a = USDC(100000), .amount_b = USDC(400000),
        .min_lp = 0
    );
    IxResult r = PROCESS(shell, ix);
    ASSERT_OK(r);
    report_cu("seashell add_liquidity", r);

    ASSERT_EQ(TOKEN_BALANCE(shell, &vault_a), USDC(100000));
    ASSERT_EQ(TOKEN_BALANCE(shell, &user_lp), USDC(200000));
}

TEST(swaps_usdc_for_the_local_token) {
    uint64_t b_before = TOKEN_BALANCE(shell, &user_b);
    Ix ix             = MAKE_IX(
        swap, .user = user, .pool_state = pool, .vault_a = vault_a,
        .vault_b = vault_b, .user_token_a = user_a, .user_token_b = user_b,
        .token_program = TOKEN_PROGRAM_ID
    );
    IX_ARGS(
        &ix, swap, .amount_in = USDC(10000), .min_amount_out = 1, .direction = 0
    );
    IxResult r = PROCESS(shell, ix);
    ASSERT_OK(r);
    report_cu("seashell swap A->B", r);

    ASSERT_EQ(TOKEN_BALANCE(shell, &vault_a), USDC(110000));
    ASSERT(TOKEN_BALANCE(shell, &user_b) > b_before);
}
