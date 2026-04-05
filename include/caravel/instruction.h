#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "types.h"
#include "error.h"

/**
 * Get the instruction discriminator (first byte of instruction data).
 */
#define INSTRUCTION_DISCRIMINATOR(params) ((params)->data[0])

/**
 * Define a single raw instruction case in a dispatch table.
 * The handler receives Parameters * directly
 */
#define INSTRUCTION(disc, handler) \
    case (disc): return handler(params);

/**
 * Dispatch to instruction handlers based on the discriminator byte.
 */
#define DISPATCH(params, ...) \
    do { \
        if ((params)->data_len == 0) return ERROR_INVALID_INSTRUCTION_DATA; \
        uint8_t _disc = INSTRUCTION_DISCRIMINATOR(params); \
        switch (_disc) { \
            __VA_ARGS__ \
            default: return ERROR_UNKNOWN_INSTRUCTION; \
        } \
    } while (0)

/**
 * Get pointer to instruction data after the discriminator byte.
 */
#define IX_DATA(params)     ((params)->data + 1)

/**
 * Get length of instruction data after the discriminator byte.
 */
#define IX_DATA_LEN(params) ((params)->data_len - 1)

/**
 * Cast instruction data (after discriminator) to a typed struct pointer.
 */
#define IX_DATA_AS(params, type) ((type *)IX_DATA(params))


#define _ACCOUNT_VALIDATE(name, flags) \
    if (((flags) & SIGNER) && !params->accounts[_idx].is_signer) \
        return ERROR_MISSING_REQUIRED_SIGNATURE; \
    if (((flags) & WRITABLE) && !params->accounts[_idx].is_writable) \
        return ERROR_ACCOUNT_NOT_WRITABLE; \
    ctx->name = &params->accounts[_idx]; \
    _idx++;

/* generate validate function: packed flag checks + pointer assignment */
#define _VALIDATE_FN(prefix, accounts_table) \
    static inline uint64_t prefix##_validate( \
        Parameters *params, \
        prefix##_accounts_t *ctx \
    ) { \
        enum { _expected = 0 accounts_table(_ACCOUNT_COUNT) }; \
        if (UNLIKELY(params->accounts_len < _expected)) \
            return ERROR_NOT_ENOUGH_ACCOUNT_KEYS; \
        uint64_t _idx = 0; \
        accounts_table(_ACCOUNT_VALIDATE) \
        (void)_idx; \
        return SUCCESS; \
    }

/* internal - 3-arg (no ix args) */
#define _IX_3(disc, prefix, accounts_table) \
    DEFINE_ACCOUNTS(prefix, accounts_table) \
    _VALIDATE_FN(prefix, accounts_table) \
    typedef struct __attribute__((packed)) { } prefix##_args_t; \
    static inline uint64_t prefix##_parse_args( \
        Parameters *_p, prefix##_args_t *_o) { \
        (void)_p; (void)_o; return SUCCESS; \
    } \
    enum { _disc_##prefix = (disc) };

/* internal - 4-arg (with ix args) */
#define _IX_4(disc, prefix, accounts_table, args_type) \
    DEFINE_ACCOUNTS(prefix, accounts_table) \
    _VALIDATE_FN(prefix, accounts_table) \
    static inline uint64_t prefix##_parse_args( \
        Parameters *params, args_type *out) { \
        if (UNLIKELY(IX_DATA_LEN(params) < sizeof(*out))) \
            return ERROR_INVALID_INSTRUCTION_DATA; \
        sol_memcpy_(out, IX_DATA(params), sizeof(*out)); \
        return SUCCESS; \
    } \
    enum { _disc_##prefix = (disc) };

/* internal - select 3- or 4-arg variant */
#define _IX_SELECT(_1, _2, _3, _4, NAME, ...) NAME
#define IX(...) \
    _IX_SELECT(__VA_ARGS__, _IX_4, _IX_3, ~)(__VA_ARGS__)

#define HANDLER(prefix) \
    case _disc_##prefix: { \
        prefix##_accounts_t _ctx; \
        uint64_t _err = prefix##_validate(params, &_ctx); \
        if (_err) return _err; \
        if (UNLIKELY(IX_DATA_LEN(params) < sizeof(prefix##_args_t))) \
            return ERROR_INVALID_INSTRUCTION_DATA; \
        prefix##_args_t *_args = \
            (prefix##_args_t *)IX_DATA(params); \
        return prefix(&_ctx, _args, params); \
    }

#endif /* INSTRUCTION_H */
