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
#include "util.h"

/**
 * Deserialize the raw input buffer into CvlParameters - account_buf must be provided by the caller
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
            /* Duplicate */
            account_buf[i].key         = account_buf[dup_marker].key;
            account_buf[i].lamports    = account_buf[dup_marker].lamports;
            account_buf[i].data_len    = account_buf[dup_marker].data_len;
            account_buf[i].data        = account_buf[dup_marker].data;
            account_buf[i].owner       = account_buf[dup_marker].owner;
            account_buf[i].rent_epoch  = account_buf[dup_marker].rent_epoch;
            account_buf[i].is_signer   = account_buf[dup_marker].is_signer;
            account_buf[i].is_writable = account_buf[dup_marker].is_writable;
            account_buf[i].executable  = account_buf[dup_marker].executable;
            /* Skip padding to align to 8 bytes */
            ptr += 7;
        } else {
            /* is_signer, is_writable, executable */
            account_buf[i].is_signer   = *ptr++;
            account_buf[i].is_writable = *ptr++;
            account_buf[i].executable  = *ptr++;

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
            ptr += ((-data_len) & 7) + 10240;

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
 *   ix_data - 8  → u64 instruction_data_len
 *   ix_data      → [instruction_data_len] instruction data bytes
 *   ix_data + len → [32] program_id
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
            account_buf[i].key         = account_buf[dup_marker].key;
            account_buf[i].lamports    = account_buf[dup_marker].lamports;
            account_buf[i].data_len    = account_buf[dup_marker].data_len;
            account_buf[i].data        = account_buf[dup_marker].data;
            account_buf[i].owner       = account_buf[dup_marker].owner;
            account_buf[i].rent_epoch  = account_buf[dup_marker].rent_epoch;
            account_buf[i].is_signer   = account_buf[dup_marker].is_signer;
            account_buf[i].is_writable = account_buf[dup_marker].is_writable;
            account_buf[i].executable  = account_buf[dup_marker].executable;
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

    uint64_t data_len = *(uint64_t *)(ix_data - 8);
    params->data = (uint8_t *)ix_data;
    params->data_len = data_len;
    params->program_id = (CvlPubkey *)(ix_data + data_len);

    return 0; /* CVL_SUCCESS */
}

/**
 * Generates the `entrypoint` function, handles deserialization, and dispatches
 * to your `process` handler
 *
 * @code:
 *   uint64_t process(CvlParameters *params) { ... }
 *   CVL_ENTRYPOINT(process);
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
 * Generates the `entrypoint` function using ix_data in r2, avoiding the 
 * need to walk past all accounts in the input buffer, and thus saving CUs.
 * 
 * @note Use of this entrypoint is contingent on activation of SIMD-0321 (5xXZc66h4UdB6Yq7FzdBxBiRAFMMScMLwHxk2QZDaNZL).
 *       See https://github.com/anza-xyz/agave/wiki/Feature-Gate-Tracker-Schedule for more details on activation.
 *
 * @code
 *   uint64_t process(CvlParameters *params) { ... }
 *   CVL_EXP_ENTRYPOINT(process);
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
 * Noop entrypoint that simply passes the raw input buffer to your handler
 *
 * @code
 *   uint64_t process(const uint8_t *input) { ... }
 *   CVL_LAZY_ENTRYPOINT(process);
 */
#define CVL_LAZY_ENTRYPOINT(handler) \
    uint64_t entrypoint(const uint8_t *input) { \
        return handler(input); \
    }

/**
 * Similar to `CVL_LAZY_ENTRYPOINT`, but requires no return value.
 * This is useful for optimizing codegen by avoiding an extra load
 *
 * @warning Any return values from your handler will be ignored.
 *          Using this macro implicitly requires you to use CVL_EXIT for any error conditions.
 *
 * @code
 *   void process(const uint8_t *input) { ... }
 *   CVL_LAZY_ENTRYPOINT_NORETURN(process);
 */
 #define CVL_RAW_ENTRYPOINT(handler) \
    void entrypoint(const uint8_t *input) { \
        handler(input); \
    }

#endif /* CVL_ENTRYPOINT_H */
