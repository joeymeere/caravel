#ifndef CARAVEL_TEST_IX_H
#define CARAVEL_TEST_IX_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** an instruction under construction - self-contained, safe to copy */
typedef struct {
    Pubkey program_id;
    size_t accounts_len;
    Pubkey keys[CVL_TEST_MAX_ACCOUNTS];
    bool is_signer[CVL_TEST_MAX_ACCOUNTS];
    bool is_writable[CVL_TEST_MAX_ACCOUNTS];
    uint8_t data[CVL_TEST_MAX_DATA];
    size_t data_len;
    const char *error; /* set by builders, reported when sent */
} Ix;

/** empty instruction for `program_id` - add accounts and data with ix_* */
_CVLT_API Ix ix_new(const Pubkey *program_id) {
    Ix ix;
    memset(&ix, 0, sizeof ix);
    ix.program_id = *program_id;
    return ix;
}

_CVLT_API void
ix_account(Ix *ix, const Pubkey *key, bool is_signer, bool is_writable) {
    if (ix->accounts_len == CVL_TEST_MAX_ACCOUNTS) {
        ix->error = "too many accounts";
        return;
    }
    ix->keys[ix->accounts_len]        = *key;
    ix->is_signer[ix->accounts_len]   = is_signer;
    ix->is_writable[ix->accounts_len] = is_writable;
    ix->accounts_len++;
}

_CVLT_API void ix_data(Ix *ix, const void *data, size_t len) {
    if (ix->data_len + len > CVL_TEST_MAX_DATA) {
        ix->error = "instruction data too large";
        return;
    }
    memcpy(ix->data + ix->data_len, data, len);
    ix->data_len += len;
}

_CVLT_API void ix_u8(Ix *ix, uint8_t v) {
    ix_data(ix, &v, 1);
}

_CVLT_API void ix_u64(Ix *ix, uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; i++) b[i] = (uint8_t)(v >> (8 * i));
    ix_data(ix, b, 8);
}

_CVLT_API Ix _cvlt_ix_from_table(
    uint8_t disc,
    const Pubkey *keys,
    const uint8_t *flags,
    const char *const *names,
    size_t n
) {
    Ix ix                    = ix_new(&PROGRAM_ID);
    static const Pubkey zero = {{0}};
    for (size_t i = 0; i < n; i++) {
        if (!(flags[i] & PROGRAM) && memcmp(&keys[i], &zero, 32) == 0) {
            static char msg[96];
            snprintf(msg, sizeof msg, "account '%s' was not set", names[i]);
            ix.error = msg;
        }
        ix_account(&ix, &keys[i], flags[i] & SIGNER, flags[i] & WRITABLE);
    }
    ix_u8(&ix, disc);
    return ix;
}

#define MAKE_IX(prefix, ...) \
    _cvlt_ix_from_table( \
        (uint8_t)_disc_##prefix, \
        (const Pubkey *)&(_cvl_keys_##prefix){__VA_ARGS__}, \
        _cvl_flags_##prefix, _cvl_names_##prefix, \
        sizeof(_cvl_keys_##prefix) / sizeof(Pubkey) \
    )

/** append the handler's args struct, built from designated initializers */
#define IX_ARGS(ix_ptr, prefix, ...) \
    ix_data( \
        (ix_ptr), &(_cvl_args_type_##prefix){__VA_ARGS__}, \
        sizeof(_cvl_args_type_##prefix) \
    )

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_IX_H */
