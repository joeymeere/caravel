#ifndef CARAVEL_TEST_MOLLUSK_CTX_H
#define CARAVEL_TEST_MOLLUSK_CTX_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** create or overwrite a stored account */
_CVLT_API void ctx_set_account(
    Ctx *ctx,
    const Pubkey *key,
    uint64_t lamports,
    const Pubkey *owner,
    const void *data,
    size_t len
) {
    MolluskKeyedAccount a = {key->bytes, lamports,     (const uint8_t *)data,
                             len,        owner->bytes, false,
                             0};
    if (mollusk_context_set_account(ctx, &a) != 0)
        FAIL("set_account failed: %s", _cvlt_mollusk_error());
}

/** add lamports to an account, creating it if absent */
_CVLT_API void ctx_airdrop(Ctx *ctx, const Pubkey *key, uint64_t lamports) {
    MolluskAccount *a = mollusk_context_get_account(ctx, key->bytes);
    if (!a) {
        ctx_set_account(ctx, key, lamports, &SYSTEM_PROGRAM_ID, NULL, 0);
        return;
    }
    MolluskKeyedAccount k;
    mollusk_account_keyed(a, key->bytes, &k);
    k.lamports += lamports;
    int rc = mollusk_context_set_account(ctx, &k);
    mollusk_account_free(a);
    if (rc != 0) FAIL("airdrop failed: %s", _cvlt_mollusk_error());
}

_CVLT_API uint64_t ctx_lamports(Ctx *ctx, const Pubkey *key) {
    MolluskAccount *a = mollusk_context_get_account(ctx, key->bytes);
    uint64_t v        = a ? mollusk_account_lamports(a) : 0;
    mollusk_account_free(a);
    return v;
}

/** whether the store holds the account */
_CVLT_API bool ctx_exists(Ctx *ctx, const Pubkey *key) {
    MolluskAccount *a = mollusk_context_get_account(ctx, key->bytes);
    bool exists       = a != NULL;
    mollusk_account_free(a);
    return exists;
}

_CVLT_API uint64_t ctx_rent_exempt(Ctx *ctx, size_t data_len) {
    return mollusk_minimum_balance_for_rent_exemption(
        _cvlt_mollusk(ctx), data_len
    );
}

_CVLT_API void *_cvlt_read(
    Ctx *ctx, const Pubkey *key, size_t size, const char *file, int line
) {
    char b58[45];
    MolluskAccount *a = mollusk_context_get_account(ctx, key->bytes);
    if (!a)
        _cvlt_fail(
            file, line, "account %s does not exist", pubkey_to_base58(key, b58)
        );
    size_t len = mollusk_account_data_len(a);
    if (len < size) {
        mollusk_account_free(a);
        _cvlt_fail(
            file, line, "account %s holds %zu bytes, expected at least %zu",
            pubkey_to_base58(key, b58), len, size
        );
    }
    void *buf = calloc(1, len ? len : 1);
    mollusk_account_data_copy(a, buf, len);
    mollusk_account_free(a);
    return _cvlt_own(buf, free, file, line);
}

/* alt for mollusk */
#define READ_ACCOUNT(ctx, key, type) \
    (*(type *)_cvlt_read((ctx), (key), sizeof(type), __FILE__, __LINE__))

#define READ_BYTES(ctx, key, offset, out, size) \
    memcpy( \
        (out), \
        (uint8_t *)_cvlt_read( \
            (ctx), (key), (offset) + (size), __FILE__, __LINE__ \
        ) + (offset), \
        (size) \
    )

_CVLT_API void ctx_set_compute_unit_limit(Ctx *ctx, uint64_t limit) {
    if (mollusk_set_compute_unit_limit(_cvlt_mollusk(ctx), limit) != 0)
        FAIL("set_compute_unit_limit failed: %s", _cvlt_mollusk_error());
}

/** TODO @joey consolidate these into a single `warp` function for titular unixts and slot */

/** warp relevant sysvars to `slot` (not just clock but others like slothashes) */
_CVLT_API void ctx_warp_to_slot(Ctx *ctx, uint64_t slot) {
    if (mollusk_warp_to_slot(_cvlt_mollusk(ctx), slot) != 0)
        FAIL("warp_to_slot failed: %s", _cvlt_mollusk_error());
}

/** set  only Clock.unix_timestamp */
_CVLT_API void ctx_set_unix_timestamp(Ctx *ctx, int64_t ts) {
    MolluskClock c;
    if (mollusk_get_clock(_cvlt_mollusk(ctx), &c) != 0)
        FAIL("get_clock failed: %s", _cvlt_mollusk_error());
    c.unix_timestamp = ts;
    if (mollusk_set_clock(_cvlt_mollusk(ctx), &c) != 0)
        FAIL("set_clock failed: %s", _cvlt_mollusk_error());
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_MOLLUSK_CTX_H */
