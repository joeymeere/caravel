#ifndef CVL_ENTRYPOINT_H
#define CVL_ENTRYPOINT_H

/**
 * serialization format:
 *   - u64 num_accounts
 *   - For each account:
 *     - u8  dup_marker (0xFF = not duplicate, otherwise index of duplicate)
 *     - if not duplicate:
 *       - u8   is_signer
 *       - u8   is_writable
 *       - u8   executable
 *       - u8   padding[4]
 *       - [32] key
 *       - [32] owner
 *       - u64  lamports
 *       - u64  data_len
 *       - [data_len] data
 *       - padding to 8-byte alignment
 *       - u64  rent_epoch
 *   - u64 instruction_data_len
 *   - [instruction_data_len] instruction_data
 *   - [32] program_id
 */

#include "types.h"

#define CVL_READ_U8(ptr)  (*(ptr)++)
#define CVL_READ_U64(ptr) ({ \
    uint64_t _v = *(uint64_t *)(ptr); \
    (ptr) += 8; \
    _v; \
})

/**
 * Deserialize the raw input buffer into CvlParameters.
 * account_buf must be provided by the caller (stack-allocated)
 */
static inline uint64_t cvl_deserialize(
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
            /* Duplicate account — points to same data as account[dup_marker] */
            account_buf[i] = account_buf[dup_marker];
            /* Skip padding to align to 8 bytes */
            ptr += 7;
        } else {
            /* is_signer, is_writable, executable */
            account_buf[i].is_signer   = (*ptr++) != 0;
            account_buf[i].is_writable = (*ptr++) != 0;
            account_buf[i].executable  = (*ptr++) != 0;

            /* 4 bytes padding */
            ptr += 4;

            /* key (32 bytes) */
            account_buf[i].key = (CvlPubkey *)ptr;
            ptr += 32;

            /* owner (32 bytes) */
            account_buf[i].owner = (CvlPubkey *)ptr;
            ptr += 32;

            /* lamports (u64, writable pointer) */
            account_buf[i].lamports = (uint64_t *)ptr;
            ptr += 8;

            /* data_len + data */
            uint64_t data_len = *(uint64_t *)ptr;
            ptr += 8;
            account_buf[i].data_len = data_len;
            account_buf[i].data = (uint8_t *)ptr;
            ptr += data_len;

            /* Padding to 8-byte alignment */
            uint64_t padding = (8 - (data_len % 8)) % 8;
            /* Also skip the additional 10240 bytes max realloc padding */
            ptr += padding + 10240;

            /* rent_epoch */
            account_buf[i].rent_epoch = *(uint64_t *)ptr;
            ptr += 8;
        }
    }

    uint64_t data_len = *(uint64_t *)ptr;
    ptr += 8;
    params->data = (uint8_t *)ptr;
    params->data_len = data_len;
    ptr += data_len;

    params->program_id = (CvlPubkey *)ptr;

    return 0; /* CVL_SUCCESS */
}

/**
 * Experimental deserializer — uses the second entrypoint parameter
 * to read instruction data + program_id directly, skipping the walk
 * past all account entries in the input buffer.
 *
 * Layout at ix_data:
 *   ix_data - 8  → u64 instruction_data_len
 *   ix_data      → [instruction_data_len] instruction data bytes
 *   ix_data + len → [32] program_id
 *
 * Accounts are still deserialized from `input` the standard way.
 */
 static inline uint64_t cvl_deserialize_exp(
    const uint8_t *input,
    const uint8_t *ix_data,
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
            account_buf[i].is_signer   = (*ptr++) != 0;
            account_buf[i].is_writable = (*ptr++) != 0;
            account_buf[i].executable  = (*ptr++) != 0;
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

            uint64_t padding = (8 - (data_len % 8)) % 8;
            ptr += padding + 10240;

            account_buf[i].rent_epoch = *(uint64_t *)ptr;
            ptr += 8;
        }
    }

    uint64_t data_len = *(uint64_t *)(ix_data - 8);
    params->data = (uint8_t *)ix_data;
    params->data_len = data_len;
    params->program_id = (CvlPubkey *)(ix_data + data_len);

    return 0; /* CVL_SUCCESS */
}

/**
 * Standard entrypoint macro. Generates the `entrypoint` function that
 * the Solana runtime calls, handles deserialization, and dispatches
 * to your handler
 *
 * Usage:
 * ```c
 *   uint64_t process(CvlParameters *params) { ... }
 *   CVL_ENTRYPOINT(process);
 * ```
 */
#if !defined(CVL_NO_HEAP) && !defined(CVL_CUSTOM_HEAP)
#define _CVL_HEAP_INIT() cvl_heap_init()
#else
#define _CVL_HEAP_INIT() ((void)0)
#endif

#define CVL_ENTRYPOINT(handler) \
    uint64_t entrypoint(const uint8_t *input) { \
        _CVL_HEAP_INIT(); \
        CvlAccountInfo _cvl_accounts[CVL_MAX_ACCOUNTS]; \
        CvlParameters  _cvl_params; \
        uint64_t _cvl_err = cvl_deserialize(input, &_cvl_params, \
                                             _cvl_accounts, CVL_MAX_ACCOUNTS); \
        if (_cvl_err) return _cvl_err; \
        return handler(&_cvl_params); \
    }

/**
 * Experimental entrypoint — uses the 2-param entrypoint signature
 * where the runtime passes a direct pointer to instruction data as
 * the second argument, avoiding the need to walk past all accounts
 *
 * Usage:
 * ```c
 *   uint64_t process(CvlParameters *params) { ... }
 *   CVL_EXP_ENTRYPOINT(process);
 * ```
 */
#define CVL_EXP_ENTRYPOINT(handler) \
    uint64_t entrypoint(const uint8_t *input, const uint8_t *ix_data) { \
        _CVL_HEAP_INIT(); \
        CvlAccountInfo _cvl_accounts[CVL_MAX_ACCOUNTS]; \
        CvlParameters  _cvl_params; \
        uint64_t _cvl_err = cvl_deserialize_exp(input, ix_data, &_cvl_params, \
                                                 _cvl_accounts, CVL_MAX_ACCOUNTS); \
        if (_cvl_err) return _cvl_err; \
        return handler(&_cvl_params); \
    }

/**
 * Lazy entrypoint — passes the raw input buffer to the handler.
 * Use when you want to deserialize manually or partially
 *
 * Usage:
 * ```c
 *   uint64_t process(const uint8_t *input) { ... }
 *   CVL_LAZY_ENTRYPOINT(process);
 * ```
 */
#define CVL_LAZY_ENTRYPOINT(handler) \
    uint64_t entrypoint(const uint8_t *input) { \
        return handler(input); \
    }

#endif /* CVL_ENTRYPOINT_H */
