#ifndef CVL_INSTRUCTION_H
#define CVL_INSTRUCTION_H

#include "types.h"
#include "error.h"

/**
 * Get the instruction discriminator (first byte of instruction data).
 */
#define CVL_INSTRUCTION_DISCRIMINATOR(params) ((params)->data[0])

/**
 * Define a single raw instruction case in a dispatch table.
 * The handler receives CvlParameters * directly
 */
#define CVL_INSTRUCTION(disc, handler) \
    case (disc): return handler(params);

/**
 * Dispatch to instruction handlers based on the discriminator byte.
 */
#define CVL_DISPATCH(params, ...) \
    do { \
        if ((params)->data_len == 0) return CVL_ERROR_INVALID_INSTRUCTION_DATA; \
        uint8_t _cvl_disc = CVL_INSTRUCTION_DISCRIMINATOR(params); \
        switch (_cvl_disc) { \
            __VA_ARGS__ \
            default: return CVL_ERROR_UNKNOWN_INSTRUCTION; \
        } \
    } while (0)

/**
 * Get pointer to instruction data after the discriminator byte.
 */
#define CVL_IX_DATA(params)     ((params)->data + 1)

/**
 * Get length of instruction data after the discriminator byte.
 */
#define CVL_IX_DATA_LEN(params) ((params)->data_len - 1)

/**
 * Cast instruction data (after discriminator) to a typed struct pointer.
 */
#define CVL_IX_DATA_AS(params, type) ((type *)CVL_IX_DATA(params))


#define _CVL_ACCOUNT_VALIDATE(name, flags) \
    if (((flags) & CVL_SIGNER) && !params->accounts[_cvl_idx].is_signer) \
        return CVL_ERROR_MISSING_REQUIRED_SIGNATURE; \
    if (((flags) & CVL_WRITABLE) && !params->accounts[_cvl_idx].is_writable) \
        return CVL_ERROR_ACCOUNT_NOT_WRITABLE; \
    ctx->name = &params->accounts[_cvl_idx]; \
    _cvl_idx++;

/* generate validate function: packed flag checks + pointer assignment */
#define _CVL_VALIDATE_FN(prefix, accounts_table) \
    static inline uint64_t prefix##_validate( \
        CvlParameters *params, \
        prefix##_accounts_t *ctx \
    ) { \
        enum { _cvl_expected = 0 accounts_table(_CVL_ACCOUNT_COUNT) }; \
        if (CVL_UNLIKELY(params->accounts_len < _cvl_expected)) \
            return CVL_ERROR_NOT_ENOUGH_ACCOUNT_KEYS; \
        uint64_t _cvl_idx = 0; \
        accounts_table(_CVL_ACCOUNT_VALIDATE) \
        (void)_cvl_idx; \
        return CVL_SUCCESS; \
    }

/* internal - 3-arg (no ix args) */
#define _CVL_IX_3(disc, prefix, accounts_table) \
    CVL_DEFINE_ACCOUNTS(prefix, accounts_table) \
    _CVL_VALIDATE_FN(prefix, accounts_table) \
    typedef struct __attribute__((packed)) { } prefix##_args_t; \
    static inline uint64_t prefix##_parse_args( \
        CvlParameters *_p, prefix##_args_t *_o) { \
        (void)_p; (void)_o; return CVL_SUCCESS; \
    } \
    enum { _cvl_disc_##prefix = (disc) };

/* internal - 4-arg (with ix args) */
#define _CVL_IX_4(disc, prefix, accounts_table, args_type) \
    CVL_DEFINE_ACCOUNTS(prefix, accounts_table) \
    _CVL_VALIDATE_FN(prefix, accounts_table) \
    static inline uint64_t prefix##_parse_args( \
        CvlParameters *params, args_type *out) { \
        if (CVL_UNLIKELY(CVL_IX_DATA_LEN(params) < sizeof(*out))) \
            return CVL_ERROR_INVALID_INSTRUCTION_DATA; \
        sol_memcpy_(out, CVL_IX_DATA(params), sizeof(*out)); \
        return CVL_SUCCESS; \
    } \
    enum { _cvl_disc_##prefix = (disc) };

/* internal - select 3- or 4-arg variant */
#define _CVL_IX_SELECT(_1, _2, _3, _4, NAME, ...) NAME
#define CVL_IX(...) \
    _CVL_IX_SELECT(__VA_ARGS__, _CVL_IX_4, _CVL_IX_3, ~)(__VA_ARGS__)

#define CVL_HANDLER(prefix) \
    case _cvl_disc_##prefix: { \
        prefix##_accounts_t _cvl_ctx; \
        uint64_t _cvl_err = prefix##_validate(params, &_cvl_ctx); \
        if (_cvl_err) return _cvl_err; \
        if (CVL_UNLIKELY(CVL_IX_DATA_LEN(params) < sizeof(prefix##_args_t))) \
            return CVL_ERROR_INVALID_INSTRUCTION_DATA; \
        prefix##_args_t *_cvl_args = \
            (prefix##_args_t *)CVL_IX_DATA(params); \
        return prefix(&_cvl_ctx, _cvl_args, params); \
    }

#endif /* CVL_INSTRUCTION_H */
