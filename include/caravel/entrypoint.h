#ifndef CVL_ENTRYPOINT_H
#define CVL_ENTRYPOINT_H

#include "types.h"
#include "util.h"

static inline uint64_t _cvl_deserialize_nodup(
    const uint8_t *input,
    CvlParameters *params,
    CvlAccountInfo *account_buf,
    uint64_t account_buf_len
) {
    const uint8_t *ptr = input;

    uint64_t num_accounts = *(uint64_t *)ptr;
    ptr += 8;

    if (num_accounts > account_buf_len)
        return CVL_ERROR_NOT_ENOUGH_ACCOUNT_KEYS;

    params->accounts = account_buf;
    params->accounts_len = num_accounts;

    for (uint64_t i = 0; i < num_accounts; i++) {
        uint8_t dup_marker = *ptr++;

        if (dup_marker != 0xFF) {
            return CVL_ERROR_INVALID_ARGUMENT;
        } else {
            account_buf[i].is_signer   = *ptr++;
            account_buf[i].is_writable = *ptr++;
            account_buf[i].executable  = *ptr++;
            ptr += 4;

            account_buf[i].key = (CvlPubkey *)ptr;
            ptr += 32;

            account_buf[i].owner = (CvlPubkey *)ptr;
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

static inline uint64_t _cvl_deserialize_accounts(
    const uint8_t *input,
    CvlParameters *params,
    CvlAccountInfo *account_buf,
    uint64_t account_buf_len
) {
    const uint8_t *ptr = input;

    uint64_t num_accounts = *(uint64_t *)ptr;
    ptr += 8;

    if (num_accounts > account_buf_len) {
        return CVL_ERROR_NOT_ENOUGH_ACCOUNT_KEYS;
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

            account_buf[i].key = (CvlPubkey *)ptr;
            ptr += 32;

            account_buf[i].owner = (CvlPubkey *)ptr;
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

    return 0; /* CVL_SUCCESS */
}

static inline uint64_t _cvl_deserialize_full(
    const uint8_t *input,
    CvlParameters *params,
    CvlAccountInfo *account_buf,
    uint64_t account_buf_len
) {
    uint64_t err = _cvl_deserialize_accounts(input, params,
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

    params->program_id = (CvlPubkey *)ptr;

    return 0;
}

#if !defined(CVL_NO_HEAP) && !defined(CVL_CUSTOM_HEAP)
#define _CVL_HEAP_INIT() cvl_heap_init()
#else
#define _CVL_HEAP_INIT() ((void)0)
#endif

#define CVL_ENTRYPOINT(...) \
    uint64_t entrypoint(const uint8_t *input, const uint8_t *ix_data) { \
        _CVL_HEAP_INIT(); \
        uint64_t _cvl_ix_len = *(uint64_t *)(ix_data - 8); \
        if (_cvl_ix_len == 0) return CVL_ERROR_INVALID_INSTRUCTION_DATA; \
        CvlAccountInfo _cvl_accts[CVL_MAX_ACCOUNTS]; \
        CvlParameters  _cvl_p; \
        uint64_t _cvl_err = _cvl_deserialize_nodup(input, &_cvl_p, \
                                _cvl_accts, CVL_MAX_ACCOUNTS); \
        if (_cvl_err) return _cvl_err; \
        _cvl_p.data       = (uint8_t *)ix_data; \
        _cvl_p.data_len   = _cvl_ix_len; \
        _cvl_p.program_id = (CvlPubkey *)(ix_data + _cvl_ix_len); \
        CvlParameters *params = &_cvl_p; \
        uint8_t _cvl_disc = ix_data[0]; \
        switch (_cvl_disc) { \
            __VA_ARGS__ \
            default: return CVL_ERROR_UNKNOWN_INSTRUCTION; \
        } \
    }

#define CVL_LEGACY_ENTRYPOINT(handler) \
    uint64_t entrypoint(const uint8_t *input) { \
        _CVL_HEAP_INIT(); \
        CvlAccountInfo _cvl_accounts[CVL_MAX_ACCOUNTS]; \
        CvlParameters  _cvl_params; \
        uint64_t _cvl_err = _cvl_deserialize_full(input, &_cvl_params, \
                                                    _cvl_accounts, CVL_MAX_ACCOUNTS); \
        if (_cvl_err) return _cvl_err; \
        return handler(&_cvl_params); \
    }

/**
 * Noop entrypoint — passes the raw input buffer to your handler.
 *
 * @code
 *   uint64_t process(const uint8_t *input) { ... }
 *   CVL_RAW_ENTRYPOINT(process);
 */
#define CVL_RAW_ENTRYPOINT(handler) \
    void entrypoint(const uint8_t *input) { \
        handler(input); \
    }

#endif /* CVL_ENTRYPOINT_H */
