/* math, util, PDA derivation, error encoding, version, logging */
#include "stubs.h"
#include "cvl_test.h"

_Static_assert(SUCCESS == 0, "success is zero");
_Static_assert(
    ERROR_INVALID_ARGUMENT == (2ULL << 32), "builtin errors are n << 32"
);
_Static_assert(ERROR_INCORRECT_AUTHORITY == (26ULL << 32), "last builtin");
_Static_assert(ERROR_CUSTOM(5) == 0x105, "custom errors are low 32 bits");
_Static_assert(
    (ERROR_UNKNOWN_INSTRUCTION >> 32) == 0, "caravel errors are Custom"
);
_Static_assert(
    CARAVEL_VERSION == CARAVEL_VERSION_MAJOR * 10000 +
                           CARAVEL_VERSION_MINOR * 100 + CARAVEL_VERSION_PATCH,
    "version integer"
);

TEST(version_string) {
    char want[32];
    snprintf(
        want, sizeof(want), "%d.%d.%d", CARAVEL_VERSION_MAJOR,
        CARAVEL_VERSION_MINOR, CARAVEL_VERSION_PATCH
    );
    CHECK(strcmp(CARAVEL_VERSION_STRING, want) == 0);
}

TEST(checked_math) {
    uint64_t r = 0;
    CHECK_EQ(checked_add_u64(1, 2, &r), SUCCESS);
    CHECK_EQ(r, 3);
    CHECK_EQ(checked_add_u64(UINT64_MAX, 0, &r), SUCCESS);
    CHECK_EQ(r, UINT64_MAX);
    CHECK(checked_add_u64(UINT64_MAX, 1, &r) != SUCCESS);

    CHECK_EQ(checked_sub_u64(5, 5, &r), SUCCESS);
    CHECK_EQ(r, 0);
    CHECK(checked_sub_u64(5, 6, &r) != SUCCESS);

    CHECK_EQ(checked_mul_u64(0, UINT64_MAX, &r), SUCCESS);
    CHECK_EQ(r, 0);
    CHECK_EQ(checked_mul_u64(UINT64_MAX, 0, &r), SUCCESS);
    CHECK_EQ(r, 0);
    CHECK_EQ(checked_mul_u64(1ULL << 32, (1ULL << 32) - 1, &r), SUCCESS);
    CHECK(checked_mul_u64(1ULL << 32, 1ULL << 32, &r) != SUCCESS);

    /* the result is untouched on failure */
    r = 77;
    checked_add_u64(UINT64_MAX, 1, &r);
    CHECK_EQ(r, 77);

    for (int i = 0; i < 100000; i++) {
        uint64_t a          = cvl_t_rand() >> (cvl_t_rand() % 64);
        uint64_t b          = cvl_t_rand() >> (cvl_t_rand() % 64);
        unsigned __int128 p = (unsigned __int128)a * b;
        uint64_t got;
        uint64_t rc = checked_mul_u64(a, b, &got);
        if ((p >> 64) != 0) {
            CHECK(rc != SUCCESS);
        } else {
            CHECK_EQ(rc, SUCCESS);
            CHECK_EQ(got, (uint64_t)p);
        }

        rc = checked_add_u64(a, b, &got);
        if (a + b < a) {
            CHECK(rc != SUCCESS);
        } else {
            CHECK_EQ(rc, SUCCESS);
            CHECK_EQ(got, a + b);
        }
    }
}

TEST(saturating_math) {
    CHECK_EQ(saturating_add_u64(UINT64_MAX - 1, 5), UINT64_MAX);
    CHECK_EQ(saturating_add_u64(2, 3), 5);
    CHECK_EQ(saturating_sub_u64(2, 3), 0);
    CHECK_EQ(saturating_sub_u64(3, 2), 1);
    CHECK_EQ(min_u64(3, 9), 3);
    CHECK_EQ(max_u64(3, 9), 9);
}

TEST(pubkey_helpers_handle_unaligned_keys) {
    /* Pubkeys inside packed state are often not 8-byte aligned */
    uint8_t buf[80];
    for (int i = 0; i < 80; i++) buf[i] = (uint8_t)i;
    const Pubkey *a = (const Pubkey *)(buf + 1);
    Pubkey b;
    memcpy(b.bytes, buf + 1, 32);
    CHECK(pubkey_eq(a, &b));

    for (int i = 0; i < 32; i++) {
        b.bytes[i] ^= 0x80;
        CHECK(!pubkey_eq(a, &b));
        b.bytes[i] ^= 0x80;
    }

    pubkey_cpy(buf + 41, a);
    CHECK_MEM(buf + 41, buf + 1, 32);
}

TEST(read_helpers) {
    const uint8_t bytes[] = {0x01, 0x02, 0x03, 0x04, 0x05,
                             0x06, 0x07, 0x08, 0x09, 0x0A};
    CHECK_EQ(READ_U16(bytes + 1), 0x0302);
    CHECK_EQ(READ_U32(bytes + 1), 0x05040302);

    const uint8_t *p = bytes + 1; /* unaligned */
    CHECK_EQ(READ_U64(p), 0x0908070605040302ULL);
    CHECK(p == bytes + 9);
    CHECK_EQ(READ_U8(p), 0x0A);
}

TEST(vec) {
    Vec(uint64_t, 4) v = VEC_INIT;
    CHECK_EQ(vec_cap(&v), 4);
    CHECK_EQ(vec_len(&v), 0);
    vec_push(&v, 10);
    vec_push(&v, 20);
    CHECK_EQ(vec_len(&v), 2);
    CHECK(!vec_full(&v));
    vec_push(&v, 30);
    vec_push(&v, 40);
    CHECK(vec_full(&v));
    CHECK_EQ(vec_pop(&v), 40);
    CHECK_EQ(v.data[0], 10);
    vec_clear(&v);
    CHECK_EQ(vec_len(&v), 0);
}

TEST(derive_address_matches_kit) {
    static const uint8_t want[32] = {
        0x3b, 0xda, 0xe7, 0xa1, 0xc7, 0xb8, 0xa1, 0x8f, 0x03, 0x03, 0x29,
        0x56, 0xcd, 0xa6, 0x86, 0x2a, 0x31, 0xfe, 0x09, 0x1b, 0x1b, 0x06,
        0x17, 0xf8, 0x31, 0x5d, 0x64, 0x88, 0x94, 0xd6, 0x24, 0x57,
    };
    Pubkey user;
    memset(&user, 0, sizeof(user));
    user.bytes[31] = 1;
    uint8_t bump   = 254;

    SignerSeed seeds[] = {
        SEED_STR("vault"),
        SEED_PUBKEY(&user),
        SEED_U8(&bump),
    };
    Pubkey out;
    derive_address(seeds, 3, &TOKEN_PROGRAM_ID, &out);
    CHECK_MEM(out.bytes, want, 32);

    SignerSeed seeds2[] = {
        SEED_BYTES("vault", 5),
        SEED_PUBKEY(&user),
        SEED_U8(&bump),
    };
    Pubkey out2;
    derive_address(seeds2, 3, &TOKEN_PROGRAM_ID, &out2);
    CHECK(pubkey_eq(&out, &out2));
}

TEST(log_helpers) {
    stub_reset();
    log_str("runtime length");
    CHECK(strcmp(stub_last_log, "runtime length") == 0);
    log_literal("compile-time length");
    CHECK(strcmp(stub_last_log, "compile-time length") == 0);
    CHECK_EQ(cvl_strlen(""), 0);
    CHECK_EQ(cvl_strlen("abc"), 3);
}

int main(void) {
    RUN(version_string);
    RUN(checked_math);
    RUN(saturating_math);
    RUN(pubkey_helpers_handle_unaligned_keys);
    RUN(read_helpers);
    RUN(vec);
    RUN(derive_address_matches_kit);
    RUN(log_helpers);
    TEST_REPORT();
}
