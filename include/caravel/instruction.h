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
#define IX_DATA(params) ((params)->data + 1)

/**
 * Get length of instruction data after the discriminator byte.
 */
#define IX_DATA_LEN(params) ((params)->data_len - 1)

/**
 * Cast instruction data (after discriminator) to a typed struct pointer.
 */
#define IX_DATA_AS(params, type) ((type *)IX_DATA(params))

/*
 * generate validate function
 */
#define _VALIDATE_FN(prefix, accounts_table) \
    static inline __attribute__((unused)) uint64_t prefix##_validate( \
        Parameters *params, prefix##_accounts_t *ctx \
    ) { \
        return prefix##_parse(params, ctx); \
    }

/*
 * internal - a function rather than an inline comparison so a zero-size args
 * struct doesn't trip -Wtype-limits ("unsigned < 0 is always false")
 */
static inline bool _cvl_args_fit(const Parameters *params, uint64_t size) {
    return IX_DATA_LEN(params) >= size;
}

/*
 * internal - zero-copy view of the args (after the discriminator) and their
 * size, so HANDLER works whatever the args struct is called
 */
#define _IX_ARGS_FN(prefix, args_type) \
    enum { _args_size_##prefix = sizeof(args_type) }; \
    static inline __attribute__((unused)) args_type *_cvl_args_##prefix( \
        Parameters *params \
    ) { \
        return (args_type *)IX_DATA(params); \
    }

/* internal - 3-arg (no ix args) */
#define _IX_3(disc, prefix, accounts_table) \
    DEFINE_ACCOUNTS(prefix, accounts_table) \
    _VALIDATE_FN(prefix, accounts_table) \
    typedef struct __attribute__((packed)) { \
    } prefix##_args_t; \
    static inline __attribute__((unused)) uint64_t prefix##_parse_args( \
        Parameters *_p, prefix##_args_t *_o \
    ) { \
        (void)_p; \
        (void)_o; \
        return SUCCESS; \
    } \
    _IX_ARGS_FN(prefix, prefix##_args_t) \
    enum { _disc_##prefix = (disc) };

/* internal - 4-arg (with ix args) */
#define _IX_4(disc, prefix, accounts_table, args_type) \
    DEFINE_ACCOUNTS(prefix, accounts_table) \
    _VALIDATE_FN(prefix, accounts_table) \
    static inline __attribute__((unused)) uint64_t prefix##_parse_args( \
        Parameters *params, args_type *out \
    ) { \
        if (UNLIKELY(IX_DATA_LEN(params) < sizeof(*out))) \
            return ERROR_INVALID_INSTRUCTION_DATA; \
        sol_memcpy_(out, IX_DATA(params), sizeof(*out)); \
        return SUCCESS; \
    } \
    _IX_ARGS_FN(prefix, args_type) \
    enum { _disc_##prefix = (disc) };

/* internal - select 3- or 4-arg variant */
#define _IX_SELECT(_1, _2, _3, _4, NAME, ...) NAME
#define IX(...) _IX_SELECT(__VA_ARGS__, _IX_4, _IX_3, ~)(__VA_ARGS__)

#define HANDLER(prefix) \
    case _disc_##prefix: { \
        prefix##_accounts_t _ctx; \
        uint64_t _err = prefix##_validate(params, &_ctx); \
        if (_err) return _err; \
        if (UNLIKELY(!_cvl_args_fit(params, _args_size_##prefix))) \
            return ERROR_INVALID_INSTRUCTION_DATA; \
        return prefix(&_ctx, _cvl_args_##prefix(params), params); \
    }

#endif /* INSTRUCTION_H */
