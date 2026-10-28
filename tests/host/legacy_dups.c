/* LEGACY_ENTRYPOINT + DISPATCH with ALLOW_DUPLICATE_ACCOUNTS */
#define ALLOW_DUPLICATE_ACCOUNTS
#include "stubs.h"
#include "cvl_test.h"
#include "input.h"

#define MOVE_ACCOUNTS(X) \
    X(from, WRITABLE) \
    X(to, WRITABLE)

IX(3, move, MOVE_ACCOUNTS)

static AccountInfo seen_accounts[MAX_ACCOUNTS];
static Parameters seen_params;
static int move_calls;

static uint64_t
move(move_accounts_t *ctx, move_args_t *args, Parameters *params) {
    (void)args;
    move_calls++;
    seen_params = *params;
    for (uint64_t i = 0; i < params->accounts_len; i++)
        seen_accounts[i] = params->accounts[i];
    seen_params.accounts = seen_accounts;

    /* aliasing: a duplicate shares the original's lamports */
    *ctx->from->lamports -= 10;
    *ctx->to->lamports += 10;
    return SUCCESS;
}

static uint64_t process(Parameters *params) {
    DISPATCH(params, HANDLER(move));
    return ERROR_UNKNOWN_INSTRUCTION;
}

LEGACY_ENTRYPOINT(process);

static const uint8_t PROGRAM_ID[32] = {0x11, 0x22};

TEST(duplicates_alias_the_original) {
    InputAccount a[2] = {input_account(1, 0, 1, 0), input_dup(0)};
    a[0].lamports     = 100;
    uint8_t data[]    = {3};

    Input in   = input_build(a, 2, data, 1, PROGRAM_ID);
    move_calls = 0;
    CHECK_EQ(entrypoint(in.buf), SUCCESS);
    CHECK_EQ(move_calls, 1);
    CHECK(seen_accounts[0].key == seen_accounts[1].key);
    CHECK(seen_accounts[0].lamports == seen_accounts[1].lamports);
    CHECK_EQ(*seen_accounts[0].lamports, 100); /* -10 then +10 */
    input_free(&in);
}

TEST(legacy_reads_ix_data_and_program_id) {
    InputAccount a[2] = {input_account(1, 0, 1, 0), input_account(2, 0, 1, 0)};
    a[0].data_len     = 13; /* forces padding before the next account */
    uint8_t data[]    = {3, 0xDE, 0xAD};

    Input in = input_build(a, 2, data, sizeof(data), PROGRAM_ID);
    CHECK_EQ(entrypoint(in.buf), SUCCESS);
    CHECK(seen_params.data == in.ix_data);
    CHECK_EQ(seen_params.data_len, sizeof(data));
    CHECK_MEM(seen_params.program_id->bytes, PROGRAM_ID, 32);
    input_free(&in);
}

TEST(legacy_rejects_unknown_and_empty) {
    InputAccount a[2] = {input_account(1, 0, 1, 0), input_account(2, 0, 1, 0)};
    uint8_t data[]    = {9};
    Input in          = input_build(a, 2, data, 1, PROGRAM_ID);
    CHECK_EQ(entrypoint(in.buf), ERROR_UNKNOWN_INSTRUCTION);
    input_free(&in);

    in = input_build(a, 2, NULL, 0, PROGRAM_ID);
    CHECK_EQ(entrypoint(in.buf), ERROR_INVALID_INSTRUCTION_DATA);
    input_free(&in);
}

/* random account sets with random duplicates */
TEST(randomized_round_trip_with_dups) {
    for (int iter = 0; iter < 2000; iter++) {
        uint64_t n = 2 + cvl_t_rand() % (MAX_ACCOUNTS - 1);
        InputAccount a[MAX_ACCOUNTS];
        int origin[MAX_ACCOUNTS]; /* index whose data account i should see */

        for (uint64_t i = 0; i < n; i++) {
            if (i >= 2 && cvl_t_rand() % 3 == 0) {
                int of    = (int)(cvl_t_rand() % i);
                of        = origin[of];
                a[i]      = input_dup(of);
                origin[i] = of;
                continue;
            }
            a[i] = input_account(
                cvl_t_rand(), cvl_t_rand() & 1, 1, cvl_t_rand() & 1
            );
            a[i].lamports   = cvl_t_rand() >> 1;
            a[i].rent_epoch = cvl_t_rand();
            a[i].data_len   = cvl_t_rand() % 40;
            origin[i]       = (int)i;
        }

        uint8_t data[] = {3, (uint8_t)cvl_t_rand()};
        Input in       = input_build(a, n, data, sizeof(data), PROGRAM_ID);
        uint64_t rc    = entrypoint(in.buf);
        CHECK_EQ(rc, SUCCESS);

        CHECK_EQ(seen_params.accounts_len, n);
        CHECK(seen_params.data == in.ix_data);
        CHECK_MEM(seen_params.program_id->bytes, PROGRAM_ID, 32);
        for (uint64_t i = 0; i < n; i++) {
            const InputAccount *o = &a[origin[i]];
            const AccountInfo *ai = &seen_accounts[i];
            CHECK_MEM(ai->key->bytes, o->key, 32);
            CHECK_EQ(ai->data_len, o->data_len);
            CHECK_EQ(ai->rent_epoch, o->rent_epoch);
            CHECK(ai->data == in.buf + in.data_offsets[origin[i]]);
        }
        input_free(&in);
    }
}

int main(void) {
    stub_reset();
    RUN(duplicates_alias_the_original);
    RUN(legacy_reads_ix_data_and_program_id);
    RUN(legacy_rejects_unknown_and_empty);
    RUN(randomized_round_trip_with_dups);
    TEST_REPORT();
}
