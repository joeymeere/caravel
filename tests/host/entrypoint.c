/* ENTRYPOINT + IX/HANDLER: dispatch, account validation, args, duplicates */
#include "stubs.h"
#include "cvl_test.h"
#include "input.h"

typedef struct __attribute__((packed)) {
    uint64_t amount;
    uint8_t flag;
} deposit_args_t;

#define DEPOSIT_ACCOUNTS(X) \
    X(user, SIGNER | WRITABLE) \
    X(vault, WRITABLE) \
    X(system_program, PROGRAM)

IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)

#define PING_ACCOUNTS(X) X(any, 0)

IX(7, ping, PING_ACCOUNTS)

static AccountInfo seen_user, seen_vault, seen_program;
static AccountInfo seen_accounts[MAX_ACCOUNTS];
static deposit_args_t seen_args;
static Parameters seen_params;
static int deposit_calls, ping_calls;

static uint64_t
deposit(deposit_accounts_t *ctx, deposit_args_t *args, Parameters *params) {
    deposit_calls++;
    seen_user    = *ctx->user;
    seen_vault   = *ctx->vault;
    seen_program = *ctx->system_program;
    seen_args    = *args;
    seen_params  = *params;
    for (uint64_t i = 0; i < params->accounts_len; i++)
        seen_accounts[i] = params->accounts[i];
    seen_params.accounts = seen_accounts;
    if (args->flag == 0xEE) return ERROR_CUSTOM(99);
    return SUCCESS;
}

static uint64_t
ping(ping_accounts_t *ctx, ping_args_t *args, Parameters *params) {
    (void)ctx;
    (void)args;
    (void)params;
    ping_calls++;
    return SUCCESS;
}

ENTRYPOINT(HANDLER(deposit) HANDLER(ping))

static const uint8_t PROGRAM_ID[32] = {0xAA, 0xBB, 0xCC};

static uint64_t
run(const InputAccount *accts,
    uint64_t n,
    const uint8_t *data,
    uint64_t len,
    Input *keep) {
    Input in    = input_build(accts, n, data, len, PROGRAM_ID);
    uint64_t rc = entrypoint(in.buf, in.ix_data);
    if (keep)
        *keep = in;
    else
        input_free(&in);
    return rc;
}

static void deposit_accounts(InputAccount a[3]) {
    a[0] = input_account(1, 1, 1, 0);
    a[1] = input_account(2, 0, 1, 0);
    a[2] = input_account(3, 0, 0, 1);
}

static const uint8_t DEPOSIT_DATA[] = {
    0,                                              /* disc */
    0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, /* amount */
    0x05,                                           /* flag */
};

TEST(dispatches_with_accounts_and_args) {
    InputAccount a[3];
    deposit_accounts(a);
    a[1].lamports   = 4242;
    a[1].data_len   = 16;
    a[1].rent_epoch = 77;

    deposit_calls = 0;
    Input in;
    CHECK_EQ(run(a, 3, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), &in), SUCCESS);
    CHECK_EQ(deposit_calls, 1);
    CHECK_EQ(seen_args.amount, 0x1122334455667788ULL);
    CHECK_EQ(seen_args.flag, 5);

    CHECK_EQ(seen_user.key->bytes[0], 1);
    CHECK_EQ(seen_vault.key->bytes[0], 2);
    CHECK_EQ(seen_program.key->bytes[0], 3);
    CHECK_EQ(*seen_vault.lamports, 4242);
    CHECK_EQ(seen_vault.data_len, 16);
    CHECK_EQ(seen_vault.rent_epoch, 77);

    /* pointers alias the input buffer, so writes reach the runtime */
    CHECK(seen_vault.data == in.buf + in.data_offsets[1]);
    *seen_vault.lamports = 1;
    uint64_t lamports;
    memcpy(&lamports, in.buf + in.data_offsets[1] - 16, 8);
    CHECK_EQ(lamports, 1);

    CHECK_EQ(seen_params.accounts_len, 3);
    CHECK_EQ(seen_params.data_len, sizeof(DEPOSIT_DATA));
    CHECK_MEM(seen_params.program_id->bytes, PROGRAM_ID, 32);
    input_free(&in);
}

TEST(handler_error_propagates) {
    InputAccount a[3];
    deposit_accounts(a);
    uint8_t data[sizeof(DEPOSIT_DATA)];
    memcpy(data, DEPOSIT_DATA, sizeof(data));
    data[9] = 0xEE;
    CHECK_EQ(run(a, 3, data, sizeof(data), NULL), ERROR_CUSTOM(99));
}

TEST(rejects_missing_signer) {
    InputAccount a[3];
    deposit_accounts(a);
    a[0].is_signer = 0;
    deposit_calls  = 0;
    CHECK_EQ(
        run(a, 3, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL),
        ERROR_MISSING_REQUIRED_SIGNATURE
    );
    CHECK_EQ(deposit_calls, 0);
    CHECK(strstr(stub_last_log, "user must be signer") != NULL);
}

TEST(rejects_not_writable) {
    InputAccount a[3];
    deposit_accounts(a);
    a[1].is_writable = 0;
    CHECK_EQ(
        run(a, 3, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL),
        ERROR_ACCOUNT_NOT_WRITABLE
    );
    CHECK(strstr(stub_last_log, "vault must be writable") != NULL);
}

TEST(rejects_non_executable_program) {
    InputAccount a[3];
    deposit_accounts(a);
    a[2].executable = 0;
    CHECK_EQ(
        run(a, 3, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL),
        ERROR_ACCOUNT_NOT_EXECUTABLE
    );
}

TEST(rejects_too_few_accounts) {
    InputAccount a[3];
    deposit_accounts(a);
    CHECK_EQ(
        run(a, 2, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL),
        ERROR_NOT_ENOUGH_ACCOUNT_KEYS
    );
}

TEST(accepts_extra_accounts) {
    InputAccount a[5];
    deposit_accounts(a);
    a[3] = input_account(4, 0, 0, 0);
    a[4] = input_account(5, 1, 1, 0);
    CHECK_EQ(run(a, 5, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL), SUCCESS);
    CHECK_EQ(seen_params.accounts_len, 5);
}

TEST(rejects_more_than_max_accounts) {
    InputAccount a[MAX_ACCOUNTS + 1];
    for (uint64_t i = 0; i < MAX_ACCOUNTS + 1; i++)
        a[i] = input_account(i + 1, 1, 1, 1);
    CHECK_EQ(
        run(a, MAX_ACCOUNTS + 1, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL),
        ERROR_NOT_ENOUGH_ACCOUNT_KEYS
    );
}

TEST(rejects_short_args) {
    InputAccount a[3];
    deposit_accounts(a);
    deposit_calls = 0;
    CHECK_EQ(
        run(a, 3, DEPOSIT_DATA, sizeof(DEPOSIT_DATA) - 1, NULL),
        ERROR_INVALID_INSTRUCTION_DATA
    );
    CHECK_EQ(deposit_calls, 0);
}

TEST(rejects_empty_and_unknown_instruction) {
    InputAccount a[3];
    deposit_accounts(a);
    CHECK_EQ(run(a, 3, NULL, 0, NULL), ERROR_INVALID_INSTRUCTION_DATA);

    uint8_t unknown[] = {42};
    CHECK_EQ(run(a, 3, unknown, 1, NULL), ERROR_UNKNOWN_INSTRUCTION);
}

TEST(dispatches_no_arg_instruction) {
    InputAccount a = input_account(9, 0, 0, 0);
    uint8_t data[] = {7};
    ping_calls     = 0;
    CHECK_EQ(run(&a, 1, data, 1, NULL), SUCCESS);
    CHECK_EQ(ping_calls, 1);
}

TEST(rejects_duplicates_by_default) {
    InputAccount a[3];
    deposit_accounts(a);
    a[1]          = input_dup(0);
    deposit_calls = 0;
    CHECK_EQ(
        run(a, 3, DEPOSIT_DATA, sizeof(DEPOSIT_DATA), NULL),
        ERROR_DUPLICATE_ACCOUNT
    );
    CHECK_EQ(deposit_calls, 0);
}

TEST(randomized_round_trip) {
    for (int iter = 0; iter < 2000; iter++) {
        uint64_t n = 3 + cvl_t_rand() % (MAX_ACCOUNTS - 2);
        InputAccount a[MAX_ACCOUNTS];
        uint8_t blobs[MAX_ACCOUNTS][64];

        for (uint64_t i = 0; i < n; i++) {
            a[i] = input_account(
                cvl_t_rand(), cvl_t_rand() & 1, cvl_t_rand() & 1,
                cvl_t_rand() & 1
            );
            for (int j = 0; j < 32; j++) {
                a[i].key[j]   = (uint8_t)cvl_t_rand();
                a[i].owner[j] = (uint8_t)cvl_t_rand();
            }
            a[i].lamports   = cvl_t_rand();
            a[i].rent_epoch = cvl_t_rand();
            a[i].data_len   = cvl_t_rand() % 64; /* exercises alignment */
            for (uint64_t j = 0; j < a[i].data_len; j++)
                blobs[i][j] = (uint8_t)cvl_t_rand();
            a[i].data = blobs[i];
        }
        /* make deposit's flag checks pass */
        a[0].is_signer = a[0].is_writable = 1;
        a[1].is_writable                  = 1;
        a[2].executable                   = 1;

        uint8_t data[sizeof(DEPOSIT_DATA)];
        memcpy(data, DEPOSIT_DATA, sizeof(data));
        uint64_t amount = cvl_t_rand();
        memcpy(data + 1, &amount, 8);

        Input in;
        uint64_t rc = run(a, n, data, sizeof(data), &in);
        CHECK_EQ(rc, SUCCESS);
        if (rc != SUCCESS) {
            input_free(&in);
            return;
        }

        CHECK_EQ(seen_args.amount, amount);
        CHECK_EQ(seen_params.accounts_len, n);
        CHECK(seen_params.data == in.ix_data);
        CHECK_MEM(seen_params.program_id->bytes, PROGRAM_ID, 32);

        for (uint64_t i = 0; i < n; i++) {
            const AccountInfo *ai = &seen_params.accounts[i];
            CHECK_MEM(ai->key->bytes, a[i].key, 32);
            CHECK_MEM(ai->owner->bytes, a[i].owner, 32);
            CHECK_EQ(*ai->lamports, a[i].lamports);
            CHECK_EQ(ai->data_len, a[i].data_len);
            CHECK_MEM(ai->data, blobs[i], a[i].data_len);
            CHECK_EQ(ai->rent_epoch, a[i].rent_epoch);
            CHECK_EQ(ai->is_signer, a[i].is_signer);
            CHECK_EQ(ai->is_writable, a[i].is_writable);
            CHECK_EQ(ai->executable, a[i].executable);
        }
        input_free(&in);
    }
}

int main(void) {
    stub_reset();
    RUN(dispatches_with_accounts_and_args);
    RUN(handler_error_propagates);
    RUN(rejects_missing_signer);
    RUN(rejects_not_writable);
    RUN(rejects_non_executable_program);
    RUN(rejects_too_few_accounts);
    RUN(accepts_extra_accounts);
    RUN(rejects_more_than_max_accounts);
    RUN(rejects_short_args);
    RUN(rejects_empty_and_unknown_instruction);
    RUN(dispatches_no_arg_instruction);
    RUN(rejects_duplicates_by_default);
    RUN(randomized_round_trip);
    TEST_REPORT();
}
