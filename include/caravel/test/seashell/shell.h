#ifndef CARAVEL_TEST_SEASHELL_SHELL_H
#define CARAVEL_TEST_SEASHELL_SHELL_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** Create or overwrite an account */
_CVLT_API void shell_set_account(
    Shell *shell,
    const Pubkey *key,
    uint64_t lamports,
    const Pubkey *owner,
    const void *data,
    size_t len
) {
    SeashellKeyedAccount a = {
        .pubkey   = key->bytes,
        .lamports = lamports,
        .data     = (const uint8_t *)data,
        .data_len = len,
        .owner    = owner->bytes,
    };
    if (seashell_set_account(shell, &a) != 0)
        FAIL("set_account failed: %s", _cvlt_seashell_error());
}

/** Add lamports to an account, creating it if absent */
_CVLT_API void
shell_airdrop(Shell *shell, const Pubkey *key, uint64_t lamports) {
    if (seashell_airdrop(shell, key->bytes, lamports) != 0)
        FAIL("airdrop failed: %s", _cvlt_seashell_error());
}

_CVLT_API uint64_t shell_lamports(Shell *shell, const Pubkey *key) {
    SeashellAccount *a = seashell_get_account(shell, key->bytes);
    uint64_t v         = a ? seashell_account_lamports(a) : 0;
    seashell_account_free(a);
    return v;
}

_CVLT_API bool shell_exists(Shell *shell, const Pubkey *key) {
    SeashellAccount *a = seashell_get_account(shell, key->bytes);
    bool exists        = a != NULL;
    seashell_account_free(a);
    return exists;
}

_CVLT_API uint64_t shell_rent_exempt(Shell *shell, size_t data_len) {
    return seashell_minimum_balance_for_rent_exemption(shell, data_len);
}

_CVLT_API void *_cvlt_read(
    Shell *shell, const Pubkey *key, size_t size, const char *file, int line
) {
    char b58[45];
    SeashellAccount *a = seashell_get_account(shell, key->bytes);
    if (!a)
        _cvlt_fail(
            file, line, "account %s does not exist", pubkey_to_base58(key, b58)
        );
    size_t len = seashell_account_data_len(a);
    if (len < size) {
        seashell_account_free(a);
        _cvlt_fail(
            file, line, "account %s holds %zu bytes, expected at least %zu",
            pubkey_to_base58(key, b58), len, size
        );
    }
    void *buf = calloc(1, len ? len : 1);
    seashell_account_data_copy(a, buf, len);
    seashell_account_free(a);
    return _cvlt_own(buf, free, file, line);
}

/* alt for seashell */
#define READ_ACCOUNT(shell, key, type) \
    (*(type *)_cvlt_read((shell), (key), sizeof(type), __FILE__, __LINE__))

#define READ_BYTES(shell, key, offset, out, size) \
    memcpy( \
        (out), \
        (uint8_t *)_cvlt_read( \
            (shell), (key), (offset) + (size), __FILE__, __LINE__ \
        ) + (offset), \
        (size) \
    )

_CVLT_API void shell_set_compute_unit_limit(Shell *shell, uint64_t limit) {
    if (seashell_set_compute_unit_limit(shell, limit) != 0)
        FAIL("set_compute_unit_limit failed: %s", _cvlt_seashell_error());
}

/** Warp the clock to a specific slot and unix timestamp */
_CVLT_API void shell_warp(Shell *shell, uint64_t slot, int64_t unix_timestamp) {
    if (seashell_warp(shell, slot, unix_timestamp) != 0)
        FAIL("warp failed: %s", _cvlt_seashell_error());
}

/** 
  Replace SPL Token with P-Token 
  TODO @joey shouldn't really need this since p-token is the default
*/
_CVLT_API void shell_use_p_token(Shell *shell) {
    if (seashell_use_p_token(shell) != 0)
        FAIL("use_p_token failed: %s", _cvlt_seashell_error());
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_SEASHELL_SHELL_H */
