#include <caravel/test.h>

/*
 * Counter program C tests — mirrors the TypeScript test suite.
 *
 * State layout: uint64_t count (8 bytes) + uint8_t authority[32] = 40 bytes
 */

#define COUNTER_SIZE 40

static QuasarSvm *svm;
static uint8_t program_id[32];
static uint8_t payer[32];
static uint8_t counter_key[32];

CVL_TEST(test_initialize) {
    svm = quasar_svm_new();

    uint64_t elf_len;
    uint8_t *elf = cvl_test_load_elf("build/program.so", &elf_len);
    CVL_ASSERT(elf != NULL);

    cvl_test_keygen(program_id);
    quasar_svm_add_program(svm, &program_id, elf, elf_len, 2);
    free(elf);

    cvl_test_keygen(payer);
    cvl_test_keygen(counter_key);

    /* seed payer and counter accounts */
    CvlTestAccount accounts[] = {
        { .lamports = 10000000000ULL },  /* payer */
        { .lamports = 0 },               /* counter — will be created */
    };
    memcpy(accounts[0].pubkey, payer, 32);
    memcpy(accounts[1].pubkey, counter_key, 32);

    /* discriminator = 0 (initialize) */
    uint8_t data[] = { 0 };
    CvlTestMeta metas[] = {
        { .is_signer = true, .is_writable = true },   /* payer */
        { .is_signer = true, .is_writable = true },   /* counter */
        { .is_signer = false, .is_writable = false },  /* system program */
    };
    memcpy(metas[0].pubkey, payer, 32);
    memcpy(metas[1].pubkey, counter_key, 32);
    /* metas[2].pubkey is zeroed = system program */

    CvlTestInstruction ix = {
        .data = data, .data_len = 1,
        .metas = metas, .num_metas = 3,
    };
    memcpy(ix.program_id, program_id, 32);

    CvlTestResult result;
    cvl_test_send(svm, &ix, accounts, 2, &result);
    CVL_ASSERT_OK(result);

    /* verify counter state from modified accounts */
    uint64_t lam;
    uint32_t dlen;
    const uint8_t *acct_data = cvl_test_result_account(&result, counter_key, &lam, &dlen);
    CVL_ASSERT(acct_data != NULL);
    CVL_ASSERT_EQ(dlen, (uint32_t)COUNTER_SIZE);
    CVL_ASSERT_EQ(cvl_test_read_u64(acct_data), 0ULL);           /* count = 0 */
    CVL_ASSERT(memcmp(acct_data + 8, payer, 32) == 0);           /* authority = payer */

    cvl_test_result_free(&result);
    return 0;
}

CVL_TEST(test_increment) {
    uint8_t data[] = { 1 };  /* discriminator = 1 (increment) */
    CvlTestMeta metas[] = {
        { .is_signer = false, .is_writable = true },   /* counter */
        { .is_signer = true,  .is_writable = false },  /* authority */
    };
    memcpy(metas[0].pubkey, counter_key, 32);
    memcpy(metas[1].pubkey, payer, 32);

    CvlTestInstruction ix = {
        .data = data, .data_len = 1,
        .metas = metas, .num_metas = 2,
    };
    memcpy(ix.program_id, program_id, 32);

    CvlTestResult result;
    cvl_test_send(svm, &ix, NULL, 0, &result);
    CVL_ASSERT_OK(result);

    const uint8_t *acct_data = cvl_test_result_account(&result, counter_key, NULL, NULL);
    CVL_ASSERT(acct_data != NULL);
    CVL_ASSERT_EQ(cvl_test_read_u64(acct_data), 1ULL);  /* count = 1 */

    cvl_test_result_free(&result);
    return 0;
}

CVL_TEST(test_decrement) {
    uint8_t data[] = { 2 };  /* discriminator = 2 (decrement) */
    CvlTestMeta metas[] = {
        { .is_signer = false, .is_writable = true },   /* counter */
        { .is_signer = true,  .is_writable = false },  /* authority */
    };
    memcpy(metas[0].pubkey, counter_key, 32);
    memcpy(metas[1].pubkey, payer, 32);

    CvlTestInstruction ix = {
        .data = data, .data_len = 1,
        .metas = metas, .num_metas = 2,
    };
    memcpy(ix.program_id, program_id, 32);

    CvlTestResult result;
    cvl_test_send(svm, &ix, NULL, 0, &result);
    CVL_ASSERT_OK(result);

    const uint8_t *acct_data = cvl_test_result_account(&result, counter_key, NULL, NULL);
    CVL_ASSERT(acct_data != NULL);
    CVL_ASSERT_EQ(cvl_test_read_u64(acct_data), 0ULL);  /* count back to 0 */

    cvl_test_result_free(&result);
    return 0;
}

CVL_TEST(test_reject_decrement_zero) {
    uint8_t data[] = { 2 };  /* decrement again — should fail at zero */
    CvlTestMeta metas[] = {
        { .is_signer = false, .is_writable = true },
        { .is_signer = true,  .is_writable = false },
    };
    memcpy(metas[0].pubkey, counter_key, 32);
    memcpy(metas[1].pubkey, payer, 32);

    CvlTestInstruction ix = {
        .data = data, .data_len = 1,
        .metas = metas, .num_metas = 2,
    };
    memcpy(ix.program_id, program_id, 32);

    CvlTestResult result;
    cvl_test_send(svm, &ix, NULL, 0, &result);
    CVL_ASSERT_FAIL(result);

    cvl_test_result_free(&result);
    return 0;
}

CVL_TEST(test_reject_wrong_authority) {
    uint8_t wrong_auth[32];
    cvl_test_keygen(wrong_auth);

    /* seed wrong authority account with lamports */
    CvlTestAccount accounts[] = {
        { .lamports = 1000000000ULL },
    };
    memcpy(accounts[0].pubkey, wrong_auth, 32);

    uint8_t data[] = { 1 };  /* increment from wrong authority */
    CvlTestMeta metas[] = {
        { .is_signer = false, .is_writable = true },
        { .is_signer = true,  .is_writable = false },
    };
    memcpy(metas[0].pubkey, counter_key, 32);
    memcpy(metas[1].pubkey, wrong_auth, 32);

    CvlTestInstruction ix = {
        .data = data, .data_len = 1,
        .metas = metas, .num_metas = 2,
    };
    memcpy(ix.program_id, program_id, 32);

    CvlTestResult result;
    cvl_test_send(svm, &ix, accounts, 1, &result);
    CVL_ASSERT_FAIL(result);

    cvl_test_result_free(&result);
    return 0;
}

CVL_TEST_MAIN(
    test_initialize,
    test_increment,
    test_decrement,
    test_reject_decrement_zero,
    test_reject_wrong_authority
)
