/* Sysvars: rent/clock getters, minimum_balance, instructions sysvar parsing */
#include "stubs.h"
#include "cvl_test.h"

static Rent make_rent(uint64_t lamports_per_byte_year, double threshold) {
    Rent r;
    memset(&r, 0, sizeof(r));
    r.lamports_per_byte_year = lamports_per_byte_year;
    memcpy(r.exemption_threshold, &threshold, 8);
    return r;
}

static uint64_t ref_minimum_balance(uint64_t lpby, double thr, uint64_t len) {
    double v = (double)((ACCOUNT_STORAGE_OVERHEAD + len) * lpby) * thr;
    if (v >= 18446744073709551616.0) return UINT64_MAX;
    if (!(v > 0)) return 0;
    return (uint64_t)v;
}

/* todo: probably remove this once rent reductions are stable */
TEST(rent_mainnet_values) {
    /* mainnet after SIMD-0194: 5080 lamports/byte-year, threshold 1.0;
     * `solana rent 0 -um` reports 0.00065024 SOL */
    Rent r = make_rent(5080, 1.0);
    CHECK_EQ(minimum_balance(&r, 0), 650240);
    CHECK_EQ(minimum_balance(&r, 165), (128 + 165) * 5080);

    /* pre-SIMD-0194: 3480 lamports/byte-year, threshold 2.0 */
    r = make_rent(3480, 2.0);
    CHECK_EQ(minimum_balance(&r, 0), 890880);
}

TEST(rent_reads_sysvar) {
    stub_rent = make_rent(5080, 1.0);
    Rent r;
    CHECK_EQ(get_rent(&r), SUCCESS);
    CHECK_EQ(r.lamports_per_byte_year, 5080);
    CHECK_EQ(minimum_balance(&r, 10), 138 * 5080);
}

TEST(rent_matches_f64_reference) {
    static const double thresholds[] = {
        1.0, 2.0, 0.5, 1.5, 3.0, 2.25, 0.1, 1.0 / 3, 7.77, 1e-9, 4096.0,
    };
    static const uint64_t lpbys[] = {3480, 5080, 1, 1000000};
    static const uint64_t lens[]  = {0, 8, 165, 10240, 10 * 1024 * 1024};

    for (unsigned t = 0; t < sizeof(thresholds) / sizeof(*thresholds); t++)
        for (unsigned l = 0; l < 4; l++)
            for (unsigned d = 0; d < 5; d++) {
                Rent r = make_rent(lpbys[l], thresholds[t]);
                CHECK_EQ(
                    minimum_balance(&r, lens[d]),
                    ref_minimum_balance(lpbys[l], thresholds[t], lens[d])
                );
            }

    for (int i = 0; i < 200000; i++) {
        uint64_t lpby = cvl_t_rand() >> (cvl_t_rand() % 64);
        uint64_t len  = cvl_t_rand() >> (cvl_t_rand() % 64);
        if (len > UINT64_MAX - 128) len = 0;
        uint64_t bits = (cvl_t_rand() & 0x000FFFFFFFFFFFFFULL) |
                        ((uint64_t)(963 + cvl_t_rand() % 120) << 52);
        double thr;
        memcpy(&thr, &bits, 8);

        uint64_t cost = (128 + len) * lpby; /* wraps like on-chain */
        double v      = (double)cost * thr;
        uint64_t want = v >= 18446744073709551616.0 ? UINT64_MAX : (uint64_t)v;

        Rent r       = make_rent(lpby, thr);
        uint64_t got = minimum_balance(&r, len);
        if (got != want) {
            CHECK_EQ(got, want);
            return;
        }
    }
    CHECK(1);
}

TEST(rent_degenerate_thresholds) {
    Rent r = make_rent(5080, 0.0);
    CHECK_EQ(minimum_balance(&r, 100), 0);
    r = make_rent(5080, -1.0);
    CHECK_EQ(minimum_balance(&r, 100), 0);
    r = make_rent(5080, 1e300);
    CHECK_EQ(minimum_balance(&r, 100), UINT64_MAX);
}

TEST(clock_reads_sysvar) {
    stub_clock.slot           = 123;
    stub_clock.unix_timestamp = -5;
    stub_clock.epoch          = 900;
    Clock c;
    CHECK_EQ(get_clock(&c), SUCCESS);
    CHECK_EQ(c.slot, 123);
    CHECK_EQ(c.unix_timestamp, (uint64_t)-5);
    CHECK_EQ(c.epoch, 900);
}

typedef struct {
    const Pubkey *program_id;
    int n_accounts;
    const Pubkey *keys[4];
    uint8_t flags[4]; /* bit 0 signer, bit 1 writable */
    const uint8_t *data;
    uint16_t data_len;
} SysvarIx;

static void put_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint64_t
build_ix_sysvar(uint8_t *buf, const SysvarIx *ixs, int n, uint16_t current) {
    uint8_t *p = buf;
    put_u16(p, (uint16_t)n);
    p += 2;
    uint8_t *offsets = p;
    p += 2 * n;
    for (int i = 0; i < n; i++) {
        put_u16(offsets + 2 * i, (uint16_t)(p - buf));
        put_u16(p, (uint16_t)ixs[i].n_accounts);
        p += 2;
        for (int a = 0; a < ixs[i].n_accounts; a++) {
            *p++ = ixs[i].flags[a];
            memcpy(p, ixs[i].keys[a]->bytes, 32);
            p += 32;
        }
        memcpy(p, ixs[i].program_id->bytes, 32);
        p += 32;
        put_u16(p, ixs[i].data_len);
        p += 2;
        if (ixs[i].data_len) memcpy(p, ixs[i].data, ixs[i].data_len);
        p += ixs[i].data_len;
    }
    put_u16(p, current);
    p += 2;
    return (uint64_t)(p - buf);
}

static Pubkey pk(uint8_t b) {
    Pubkey k;
    memset(&k, b, 32);
    return k;
}

TEST(instructions_sysvar_parsing) {
    Pubkey prog_a = pk(0xA1), prog_b = pk(0xB2), me = pk(0xCC);
    Pubkey k1 = pk(1), k2 = pk(2), k3 = pk(3);
    uint8_t d0[] = {9, 8, 7}, d1[] = {0x42};

    SysvarIx ixs[] = {
        {&prog_a, 3, {&k1, &k2, &k3}, {0x3, 0x2, 0x0}, d0, sizeof(d0)},
        {&me, 1, {&k2}, {0x1}, d1, sizeof(d1)},
        {&prog_b, 0, {0}, {0}, NULL, 0},
    };
    uint8_t buf[512];
    uint64_t len = build_ix_sysvar(buf, ixs, 3, 1);

    AccountInfo acc;
    memset(&acc, 0, sizeof(acc));
    acc.key      = (Pubkey *)&INSTRUCTIONS_SYSVAR_ID;
    acc.data     = buf;
    acc.data_len = len;

    InstructionsSysvar sv;
    CHECK_EQ(instructions_sysvar_init(&sv, &acc), SUCCESS);
    CHECK_EQ(instructions_count(&sv), 3);
    CHECK_EQ(instructions_current_index(&sv), 1);

    LoadedInstruction ix;
    CHECK_EQ(instructions_get(&sv, 0, &ix), SUCCESS);
    CHECK(pubkey_eq(ix.program_id, &prog_a));
    CHECK_EQ(ix.accounts_len, 3);
    CHECK_EQ(ix.data_len, 3);
    CHECK_MEM(ix.data, d0, 3);

    /* account metas: flags byte then pubkey */
    const Pubkey *key;
    bool signer, writable;
    loaded_ix_account(&ix, 0, &key, &signer, &writable);
    CHECK(pubkey_eq(key, &k1));
    CHECK(signer);
    CHECK(writable);
    loaded_ix_account(&ix, 1, &key, &signer, &writable);
    CHECK(pubkey_eq(key, &k2));
    CHECK(!signer);
    CHECK(writable);
    loaded_ix_account(&ix, 2, &key, &signer, &writable);
    CHECK(pubkey_eq(key, &k3));
    CHECK(!signer);
    CHECK(!writable);

    CHECK_EQ(instructions_get(&sv, 1, &ix), SUCCESS);
    loaded_ix_account(&ix, 0, &key, &signer, &writable);
    CHECK(pubkey_eq(key, &k2));
    CHECK(signer);
    CHECK(!writable);

    CHECK_EQ(instructions_get(&sv, 2, &ix), SUCCESS);
    CHECK(pubkey_eq(ix.program_id, &prog_b));
    CHECK_EQ(ix.accounts_len, 0);
    CHECK_EQ(ix.data_len, 0);

    CHECK_EQ(instructions_get(&sv, 3, &ix), ERROR_INVALID_ARGUMENT);

    /* reentrancy: only the current instruction invokes `me` */
    CHECK_EQ(assert_no_reentrancy(&sv, &me), SUCCESS);
    CHECK_EQ(assert_no_reentrancy(&sv, &prog_b), ERROR_INVALID_ARGUMENT);
}

TEST(instructions_sysvar_rejects_wrong_account) {
    uint8_t buf[8]    = {0};
    Pubkey not_sysvar = pk(7);
    AccountInfo acc;
    memset(&acc, 0, sizeof(acc));
    acc.key      = &not_sysvar;
    acc.data     = buf;
    acc.data_len = sizeof(buf);
    InstructionsSysvar sv;
    CHECK_EQ(instructions_sysvar_init(&sv, &acc), ERROR_INVALID_ARGUMENT);
}

int main(void) {
    stub_reset();
    RUN(rent_mainnet_values);
    RUN(rent_reads_sysvar);
    RUN(rent_matches_f64_reference);
    RUN(rent_degenerate_thresholds);
    RUN(clock_reads_sysvar);
    RUN(instructions_sysvar_parsing);
    RUN(instructions_sysvar_rejects_wrong_account);
    TEST_REPORT();
}
