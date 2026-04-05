#ifndef ENTRYPOINT_H
#define ENTRYPOINT_H

#include "types.h"
#include "util.h"

static inline uint64_t _deserialize_nodup(
    const uint8_t *input,
    Parameters *params,
    AccountInfo *account_buf,
    uint64_t account_buf_len
) {
    const uint8_t *ptr = input;

    uint64_t num_accounts = *(uint64_t *)ptr;
    ptr += 8;

    if (num_accounts > account_buf_len)
        return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;

    params->accounts = account_buf;
    params->accounts_len = num_accounts;

    for (uint64_t i = 0; i < num_accounts; i++) {
        uint8_t dup_marker = *ptr++;

        if (dup_marker != 0xFF) {
            return ERROR_INVALID_ARGUMENT;
        } else {
            account_buf[i].is_signer   = *ptr++;
            account_buf[i].is_writable = *ptr++;
            account_buf[i].executable  = *ptr++;
            ptr += 4;

            account_buf[i].key = (Pubkey *)ptr;
            ptr += 32;

            account_buf[i].owner = (Pubkey *)ptr;
            ptr += 32;

            account_buf[i].lamports = (uint64_t *)ptr;
            ptr += 8;

            uint64_t data_len = *(uint64_t *)ptr;
            ptr += 8;
            account_buf[i].data_len = data_len;
            account_buf[i].data = (uint8_t *)ptr;
            ptr += data_len;

            ptr += ((-data_len) & 7) + 10240;

            account_buf[i].rent_epoch = *(uint64_t *)ptr;
            ptr += 8;
        }
    }

    return 0;
}

static inline uint64_t _deserialize_accounts(
    const uint8_t *input,
    Parameters *params,
    AccountInfo *account_buf,
    uint64_t account_buf_len
) {
    const uint8_t *ptr = input;

    uint64_t num_accounts = *(uint64_t *)ptr;
    ptr += 8;

    if (num_accounts > account_buf_len) {
        return ERROR_NOT_ENOUGH_ACCOUNT_KEYS;
    }

    params->accounts = account_buf;
    params->accounts_len = num_accounts;

    for (uint64_t i = 0; i < num_accounts; i++) {
        uint8_t dup_marker = *ptr++;

        if (dup_marker != 0xFF) {
            account_buf[i] = account_buf[dup_marker];
            ptr += 7;
        } else {
            account_buf[i].is_signer   = *ptr++;
            account_buf[i].is_writable = *ptr++;
            account_buf[i].executable  = *ptr++;
            ptr += 4;

            account_buf[i].key = (Pubkey *)ptr;
            ptr += 32;

            account_buf[i].owner = (Pubkey *)ptr;
            ptr += 32;

            account_buf[i].lamports = (uint64_t *)ptr;
            ptr += 8;

            uint64_t data_len = *(uint64_t *)ptr;
            ptr += 8;
            account_buf[i].data_len = data_len;
            account_buf[i].data = (uint8_t *)ptr;
            ptr += data_len;

            ptr += ((-data_len) & 7) + 10240;

            account_buf[i].rent_epoch = *(uint64_t *)ptr;
            ptr += 8;
        }
    }

    return 0; /* SUCCESS */
}

static inline uint64_t _deserialize_full(
    const uint8_t *input,
    Parameters *params,
    AccountInfo *account_buf,
    uint64_t account_buf_len
) {
    uint64_t err = _deserialize_accounts(input, params,
                                              account_buf, account_buf_len);
    if (err) return err;

    const uint8_t *ptr = input;
    uint64_t num_accounts = *(uint64_t *)ptr;
    ptr += 8;

    for (uint64_t i = 0; i < num_accounts; i++) {
        uint8_t dup_marker = *ptr++;
        if (dup_marker != 0xFF) {
            ptr += 7;
        } else {
            ptr += 3 + 4 + 32 + 32 + 8; 
            uint64_t data_len = *(uint64_t *)ptr;
            ptr += 8 + data_len;
            ptr += ((-data_len) & 7) + 10240 + 8; 
        }
    }

    uint64_t data_len = *(uint64_t *)ptr;
    ptr += 8;
    params->data = (uint8_t *)ptr;
    params->data_len = data_len;
    ptr += data_len;

    params->program_id = (Pubkey *)ptr;

    return 0;
}

#if !defined(NO_HEAP) && !defined(CUSTOM_HEAP)
#define _HEAP_INIT() heap_init()
#else
#define _HEAP_INIT() ((void)0)
#endif

#define ENTRYPOINT(...) \
    uint64_t entrypoint(const uint8_t *input, const uint8_t *ix_data) { \
        _HEAP_INIT(); \
        uint64_t _ix_len = *(uint64_t *)(ix_data - 8); \
        if (_ix_len == 0) return ERROR_INVALID_INSTRUCTION_DATA; \
        AccountInfo _accts[MAX_ACCOUNTS]; \
        Parameters  _p; \
        uint64_t _err = _deserialize_nodup(input, &_p, \
                                _accts, MAX_ACCOUNTS); \
        if (_err) return _err; \
        _p.data       = (uint8_t *)ix_data; \
        _p.data_len   = _ix_len; \
        _p.program_id = (Pubkey *)(ix_data + _ix_len); \
        Parameters *params = &_p; \
        uint8_t _disc = ix_data[0]; \
        switch (_disc) { \
            __VA_ARGS__ \
            default: return ERROR_UNKNOWN_INSTRUCTION; \
        } \
    }

#define LEGACY_ENTRYPOINT(handler) \
    uint64_t entrypoint(const uint8_t *input) { \
        _HEAP_INIT(); \
        AccountInfo _accounts[MAX_ACCOUNTS]; \
        Parameters  _params; \
        uint64_t _err = _deserialize_full(input, &_params, \
                                                    _accounts, MAX_ACCOUNTS); \
        if (_err) return _err; \
        return handler(&_params); \
    }

/**
 * Noop entrypoint — passes the raw input buffer to your handler.
 *
 * @code
 *   uint64_t process(const uint8_t *input) { ... }
 *   RAW_ENTRYPOINT(process);
 */
#define RAW_ENTRYPOINT(handler) \
    void entrypoint(const uint8_t *input) { \
        handler(input); \
    }

#endif /* ENTRYPOINT_H */
