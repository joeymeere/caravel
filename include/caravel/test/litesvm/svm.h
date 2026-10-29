#ifndef CARAVEL_TEST_LITESVM_SVM_H
#define CARAVEL_TEST_LITESVM_SVM_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

_CVLT_API void svm_airdrop(Svm *svm, const Pubkey *key, uint64_t lamports) {
    if (litesvm_airdrop(svm, key->bytes, lamports) != 0) FAIL("airdrop failed");
}

_CVLT_API uint64_t svm_lamports(Svm *svm, const Pubkey *key) {
    uint64_t v = 0;
    litesvm_get_balance(svm, key->bytes, &v);
    return v;
}

_CVLT_API bool svm_exists(Svm *svm, const Pubkey *key) {
    LiteSvmAccount *a = litesvm_get_account(svm, key->bytes);
    bool exists       = a != NULL;
    litesvm_account_free(a);
    return exists;
}

_CVLT_API uint64_t svm_rent_exempt(Svm *svm, size_t data_len) {
    return litesvm_minimum_balance_for_rent_exemption(svm, data_len);
}

/** Create or overwrite an account */
_CVLT_API void svm_set_account(
    Svm *svm,
    const Pubkey *key,
    uint64_t lamports,
    const Pubkey *owner,
    const void *data,
    size_t len
) {
    LiteSvmAccount *a = litesvm_account_new(
        lamports, (const uint8_t *)data, len, owner->bytes, false, 0
    );
    if (!a || litesvm_set_account(svm, key->bytes, a) != 0)
        FAIL("set_account failed");
    litesvm_account_free(a);
}

_CVLT_API void *_cvlt_read(
    Svm *svm, const Pubkey *key, size_t size, const char *file, int line
) {
    char b58[45];
    LiteSvmAccount *a = litesvm_get_account(svm, key->bytes);
    if (!a)
        _cvlt_fail(
            file, line, "account %s does not exist", pubkey_to_base58(key, b58)
        );
    size_t len = litesvm_account_data_len(a);
    if (len < size) {
        litesvm_account_free(a);
        _cvlt_fail(
            file, line, "account %s holds %zu bytes, expected at least %zu",
            pubkey_to_base58(key, b58), len, size
        );
    }
    void *buf = calloc(1, len ? len : 1);
    litesvm_account_data_copy(a, buf, len);
    litesvm_account_free(a);
    return _cvlt_own(buf, free, file, line);
}

/* alt for litesvm */
#define READ_ACCOUNT(svm, key, type) \
    (*(type *)_cvlt_read((svm), (key), sizeof(type), __FILE__, __LINE__))

#define READ_BYTES(svm, key, offset, out, size) \
    memcpy( \
        (out), \
        (uint8_t *)_cvlt_read( \
            (svm), (key), (offset) + (size), __FILE__, __LINE__ \
        ) + (offset), \
        (size) \
    )

_CVLT_API void svm_set_compute_unit_limit(Svm *svm, uint64_t limit) {
    if (litesvm_set_compute_unit_limit(svm, limit) != 0)
        FAIL("set_compute_unit_limit failed");
}

/** TODO @joey consolidate these into a single `warp` function for titular unixts and slot */

/** Warp the clock to a specific slot */
_CVLT_API void svm_warp_to_slot(Svm *svm, uint64_t slot) {
    if (litesvm_warp_to_slot(svm, slot) != 0) FAIL("warp_to_slot failed");
}

/** Set only Clock.unix_timestamp */
_CVLT_API void svm_set_unix_timestamp(Svm *svm, int64_t ts) {
    LiteSvmClock c;
    if (litesvm_get_clock(svm, &c) != 0) FAIL("get_clock failed");
    c.unix_timestamp = ts;
    if (litesvm_set_clock(svm, &c) != 0) FAIL("set_clock failed");
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_LITESVM_SVM_H */
