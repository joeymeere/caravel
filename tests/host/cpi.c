#include "stubs.h"
#include "cvl_test.h"

enum { R = 0, W = 1, S = 2, WS = 3 }; /* meta roles */

static Pubkey keys[8];
static AccountInfo infos[8];

static void setup(void) {
    stub_reset();
    for (int i = 0; i < 8; i++) {
        memset(&keys[i], 0, sizeof(Pubkey));
        keys[i].bytes[0] = (uint8_t)(0xA0 + i);
        memset(&infos[i], 0, sizeof(AccountInfo));
        infos[i].key = &keys[i];
    }
}

static void expect_cpi(
    const Pubkey *program,
    int n,
    const Pubkey *const *metas,
    const int *roles,
    const uint8_t *data,
    uint64_t data_len
) {
    CHECK_EQ(stub_cpi.calls, 1);
    CHECK_MEM(stub_cpi.program_id.bytes, program->bytes, 32);
    CHECK_EQ(stub_cpi.metas_len, n);
    for (int i = 0; i < n && i < STUB_MAX_METAS; i++) {
        CHECK_MEM(stub_cpi.meta_keys[i].bytes, metas[i]->bytes, 32);
        CHECK_EQ(stub_cpi.meta_writable[i], (roles[i] & W) != 0);
        CHECK_EQ(stub_cpi.meta_signer[i], (roles[i] & S) != 0);
    }
    CHECK_EQ(stub_cpi.data_len, data_len);
    if (data_len) CHECK_MEM(stub_cpi.data, data, data_len);
}

static void le64(uint8_t *p, uint64_t v) {
    memcpy(p, &v, 8);
}

#define K(i) (&keys[i])
#define A(i) (&infos[i])
#define CTX  infos, 8

TEST(system_transfer_encoding) {
    setup();
    CHECK_EQ(system_transfer(A(0), A(1), 0x0102030405060708ULL, CTX), SUCCESS);
    uint8_t d[12] = {2, 0, 0, 0};
    le64(d + 4, 0x0102030405060708ULL);
    const Pubkey *m[] = {K(0), K(1)};
    expect_cpi(&SYSTEM_PROGRAM_ID, 2, m, (int[]){WS, W}, d, sizeof(d));
}

TEST(system_create_account_encoding) {
    setup();
    CHECK_EQ(system_create_account(A(0), A(1), 1000, 165, K(5), CTX), SUCCESS);
    uint8_t d[52] = {0, 0, 0, 0};
    le64(d + 4, 1000);
    le64(d + 12, 165);
    memcpy(d + 20, K(5)->bytes, 32);
    const Pubkey *m[] = {K(0), K(1)};
    expect_cpi(&SYSTEM_PROGRAM_ID, 2, m, (int[]){WS, WS}, d, sizeof(d));
}

TEST(system_allocate_and_assign_encoding) {
    setup();
    CHECK_EQ(system_allocate(A(2), 77, CTX), SUCCESS);
    uint8_t d[12] = {8, 0, 0, 0};
    le64(d + 4, 77);
    const Pubkey *m[] = {K(2)};
    expect_cpi(&SYSTEM_PROGRAM_ID, 1, m, (int[]){WS}, d, sizeof(d));

    setup();
    CHECK_EQ(system_assign(A(2), K(6), CTX), SUCCESS);
    uint8_t a[36] = {1, 0, 0, 0};
    memcpy(a + 4, K(6)->bytes, 32);
    expect_cpi(&SYSTEM_PROGRAM_ID, 1, m, (int[]){WS}, a, sizeof(a));
}

TEST(signed_variants_pass_seeds) {
    uint8_t bump       = 254;
    SignerSeed seeds[] = {SEED_STR("vault"), SEED_PUBKEY(K(0)), SEED_U8(&bump)};
    SignerSeeds signer = {.seeds = seeds, .len = 3};

    setup();
    CHECK_EQ(system_transfer_signed(A(0), A(1), 5, CTX, &signer, 1), SUCCESS);
    CHECK_EQ(stub_cpi.signers_len, 1);
    CHECK_EQ(stub_cpi.seeds_len[0], 3);

    setup();
    CHECK_EQ(
        token_transfer_signed(A(0), A(1), A(2), 5, CTX, &signer, 1), SUCCESS
    );
    CHECK_EQ(stub_cpi.signers_len, 1);
    CHECK_EQ(stub_cpi.seeds_len[0], 3);
}

TEST(cpi_error_propagates) {
    setup();
    stub_cpi.return_value = ERROR_INSUFFICIENT_FUNDS;
    CHECK_EQ(system_transfer(A(0), A(1), 1, CTX), ERROR_INSUFFICIENT_FUNDS);
}

TEST(token_amount_instructions) {
    struct {
        uint8_t disc;
        uint64_t (*fn)(
            AccountInfo *,
            AccountInfo *,
            AccountInfo *,
            uint64_t,
            AccountInfo *,
            int
        );
        int roles[3];
    } cases[] = {
        {3, token_transfer, {W, W, S}},
        {7, token_mint_to, {W, W, S}},
        {8, token_burn, {W, W, S}},
        {4, token_approve, {W, R, S}},
    };
    for (unsigned c = 0; c < sizeof(cases) / sizeof(*cases); c++) {
        setup();
        CHECK_EQ(
            cases[c].fn(A(0), A(1), A(2), 0xFFEEDDCCBBAA9988ULL, CTX), SUCCESS
        );
        uint8_t d[9] = {cases[c].disc};
        le64(d + 1, 0xFFEEDDCCBBAA9988ULL);
        const Pubkey *m[] = {K(0), K(1), K(2)};
        expect_cpi(&TOKEN_PROGRAM_ID, 3, m, cases[c].roles, d, sizeof(d));
    }
}

TEST(token_close_freeze_thaw_revoke) {
    const Pubkey *m3[] = {K(0), K(1), K(2)};

    setup();
    CHECK_EQ(token_close_account(A(0), A(1), A(2), CTX), SUCCESS);
    expect_cpi(&TOKEN_PROGRAM_ID, 3, m3, (int[]){W, W, S}, (uint8_t[]){9}, 1);

    setup();
    CHECK_EQ(token_freeze_account(A(0), A(1), A(2), CTX), SUCCESS);
    expect_cpi(&TOKEN_PROGRAM_ID, 3, m3, (int[]){W, R, S}, (uint8_t[]){10}, 1);

    setup();
    CHECK_EQ(token_thaw_account(A(0), A(1), A(2), CTX), SUCCESS);
    expect_cpi(&TOKEN_PROGRAM_ID, 3, m3, (int[]){W, R, S}, (uint8_t[]){11}, 1);

    setup();
    CHECK_EQ(token_revoke(A(0), A(1), CTX), SUCCESS);
    const Pubkey *m2[] = {K(0), K(1)};
    expect_cpi(&TOKEN_PROGRAM_ID, 2, m2, (int[]){W, S}, (uint8_t[]){5}, 1);

    setup();
    CHECK_EQ(token_sync_native(A(3), CTX), SUCCESS);
    const Pubkey *m1[] = {K(3)};
    expect_cpi(&TOKEN_PROGRAM_ID, 1, m1, (int[]){W}, (uint8_t[]){17}, 1);
}

TEST(token_initialize_mint_encoding) {
    const Pubkey *m[] = {K(0), K(1)};

    setup();
    CHECK_EQ(token_initialize_mint(A(0), A(1), K(4), K(5), 9, CTX), SUCCESS);
    uint8_t d[67] = {0, 9};
    memcpy(d + 2, K(4)->bytes, 32);
    d[34] = 1;
    memcpy(d + 35, K(5)->bytes, 32);
    expect_cpi(&TOKEN_PROGRAM_ID, 2, m, (int[]){W, R}, d, sizeof(d));

    /* no freeze authority: COption::None is a single 0 tag byte */
    setup();
    CHECK_EQ(token_initialize_mint(A(0), A(1), K(4), NULL, 6, CTX), SUCCESS);
    d[1]  = 6;
    d[34] = 0;
    expect_cpi(&TOKEN_PROGRAM_ID, 2, m, (int[]){W, R}, d, 35);
}

TEST(token_initialize_account_encoding) {
    setup();
    CHECK_EQ(token_initialize_account(A(0), A(1), A(2), CTX), SUCCESS);
    const Pubkey *m[] = {K(0), K(1), K(2), &RENT_SYSVAR_ID};
    expect_cpi(&TOKEN_PROGRAM_ID, 4, m, (int[]){W, R, R, R}, (uint8_t[]){1}, 1);
}

TEST(token_set_authority_encoding) {
    const Pubkey *m[] = {K(0), K(1)};

    setup();
    CHECK_EQ(
        token_set_authority(
            A(0), A(1), TOKEN_AUTHORITY_CLOSE_ACCOUNT, K(7), CTX
        ),
        SUCCESS
    );
    uint8_t d[35] = {6, 3, 1};
    memcpy(d + 3, K(7)->bytes, 32);
    expect_cpi(&TOKEN_PROGRAM_ID, 2, m, (int[]){W, S}, d, sizeof(d));

    setup();
    CHECK_EQ(
        token_set_authority(A(0), A(1), TOKEN_AUTHORITY_MINT_TOKENS, NULL, CTX),
        SUCCESS
    );
    expect_cpi(&TOKEN_PROGRAM_ID, 2, m, (int[]){W, S}, (uint8_t[]){6, 0, 0}, 3);
}

TEST(ata_create_encoding) {
    const Pubkey *m[] = {
        K(0), K(1), K(2), K(3), &SYSTEM_PROGRAM_ID, &TOKEN_PROGRAM_ID
    };
    int roles[] = {WS, W, R, R, R, R};

    setup();
    CHECK_EQ(
        create_associated_token_account(A(0), A(1), A(2), A(3), CTX), SUCCESS
    );
    expect_cpi(&ASSOCIATED_TOKEN_PROGRAM_ID, 6, m, roles, NULL, 0);

    setup();
    CHECK_EQ(
        create_associated_token_account_idempotent(A(0), A(1), A(2), A(3), CTX),
        SUCCESS
    );
    expect_cpi(&ASSOCIATED_TOKEN_PROGRAM_ID, 6, m, roles, (uint8_t[]){1}, 1);
}

static uint64_t cpi_invoke_macro(void) {
    AccountMeta metas[] = {meta_readonly_signer(K(4)), meta_writable(K(5))};
    uint8_t data[]      = {0xCA, 0xFE};
    CPI_INVOKE(K(6), metas, 2, data, sizeof(data), infos, 8);
    return SUCCESS;
}

TEST(cpi_invoke_macro_builds_instruction) {
    setup();
    CHECK_EQ(cpi_invoke_macro(), SUCCESS);
    const Pubkey *m[] = {K(4), K(5)};
    expect_cpi(K(6), 2, m, (int[]){S, W}, (uint8_t[]){0xCA, 0xFE}, 2);

    setup();
    stub_cpi.return_value = ERROR_CUSTOM(3);
    CHECK_EQ(cpi_invoke_macro(), ERROR_CUSTOM(3));
}

int main(void) {
    RUN(system_transfer_encoding);
    RUN(system_create_account_encoding);
    RUN(system_allocate_and_assign_encoding);
    RUN(signed_variants_pass_seeds);
    RUN(cpi_error_propagates);
    RUN(token_amount_instructions);
    RUN(token_close_freeze_thaw_revoke);
    RUN(token_initialize_mint_encoding);
    RUN(token_initialize_account_encoding);
    RUN(token_set_authority_encoding);
    RUN(ata_create_encoding);
    RUN(cpi_invoke_macro_builds_instruction);
    TEST_REPORT();
}
