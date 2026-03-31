#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "types.h"
#include "error.h"
#include "log.h"

#define ACCOUNT_KEY(acc)        ((acc)->key)
#define ACCOUNT_LAMPORTS(acc)   (*(acc)->lamports)
#define ACCOUNT_DATA(acc)       ((acc)->data)
#define ACCOUNT_DATA_LEN(acc)   ((acc)->data_len)
#define ACCOUNT_OWNER(acc)      ((acc)->owner)
#define ACCOUNT_IS_SIGNER(acc)  ((acc)->is_signer)
#define ACCOUNT_IS_WRITABLE(acc)((acc)->is_writable)
#define ACCOUNT_EXECUTABLE(acc) ((acc)->executable)

/**
 * Cast account data to a typed struct pointer (zero-copy).
 *
 * @param acc The account to cast
 * @param type The type to cast to (e.g. MyState)
 * @return The pointer to the casted data
 *
 * @code: MyState *state = ACCOUNT_STATE(acc, MyState);
 */
#define ACCOUNT_STATE(acc, type) ((type *)((acc)->data))

#define SIGNER   (1 << 0)
#define WRITABLE (1 << 1)
#define PROGRAM  (1 << 2)

#define ASSERT_SIGNER(acc) \
    do { \
        if (!(acc)->is_signer) { \
            log_literal("err: account not signer"); \
            return ERROR_MISSING_REQUIRED_SIGNATURE; \
        } \
    } while (0)

#define ASSERT_WRITABLE(acc) \
    do { \
        if (!(acc)->is_writable) { \
            log_literal("err: account not writable"); \
            return ERROR_ACCOUNT_NOT_WRITABLE; \
        } \
    } while (0)

#define ASSERT_OWNER(acc, expected_owner) \
    do { \
        if (!pubkey_eq((acc)->owner, (expected_owner))) { \
            log_literal("err: wrong account owner"); \
            return ERROR_ACCOUNT_WRONG_OWNER; \
        } \
    } while (0)

#define ASSERT_KEY(acc, expected_key) \
    do { \
        if (!pubkey_eq((acc)->key, (expected_key))) { \
            log_literal("err: wrong account key"); \
            return ERROR_ACCOUNT_WRONG_KEY; \
        } \
    } while (0)

#define ASSERT_INITIALIZED(acc) \
    do { \
        if ((acc)->data_len == 0) { \
            log_literal("err: account not initialized"); \
            return ERROR_UNINITIALIZED_ACCOUNT; \
        } \
    } while (0)

#define ASSERT_DATA_LEN(acc, min_len) \
    do { \
        if ((acc)->data_len < (min_len)) { \
            log_literal("err: account data too small"); \
            return ERROR_ACCOUNT_DATA_TOO_SMALL; \
        } \
    } while (0)

/**
 * X-macro system for defining instruction account contexts.
 * Accounts are assigned indices automatically based on declaration order.
 *
 * 1. Define your accounts table:
 * @code:
 *    #define MY_ACCOUNTS(X) \
 *        X(payer,          SIGNER | WRITABLE) \
 *        X(counter,        WRITABLE) \
 *        X(system_program, PROGRAM)
 *
 * 2. Generate the struct + parser:
 * @code: DEFINE_ACCOUNTS(my_instruction, MY_ACCOUNTS)
 *
 * This produces:
 * @code:
 *    typedef struct { AccountInfo *payer, *counter, *system_program; }
 *      my_instruction_accounts_t;
 *    static inline uint64_t my_instruction_parse(Parameters *params,
 *      my_instruction_accounts_t *ctx) which validates flags and populates ctx.
 */

#define _ACCOUNT_FIELD(name, flags) \
    AccountInfo *name;

#define _ACCOUNT_COUNT(name, flags) + 1

/* generate validation + assignment with auto-incrementing index */
#undef _ACCOUNT_VALIDATE
#define _ACCOUNT_VALIDATE(name, flags) \
    ctx->name = &params->accounts[_idx]; \
    if (((flags) & SIGNER) && !ctx->name->is_signer) { \
        log_literal("err: " #name " must be signer"); \
        return ERROR_MISSING_REQUIRED_SIGNATURE; \
    } \
    if (((flags) & WRITABLE) && !ctx->name->is_writable) { \
        log_literal("err: " #name " must be writable"); \
        return ERROR_ACCOUNT_NOT_WRITABLE; \
    } \
    _idx++;

/**
 * Mark a struct as a state account for IDL generation.
 * Place immediately before the typedef struct.
 *
 * @code:
 *    STATE(CounterState)
 *    typedef struct { uint64_t count; } CounterState;
 */
#define STATE(name)

#define DEFINE_ACCOUNTS(prefix, accounts_table) \
    typedef struct { \
        accounts_table(_ACCOUNT_FIELD) \
    } prefix##_accounts_t; \
    \
    static inline uint64_t prefix##_parse( \
        Parameters *params, \
        prefix##_accounts_t *ctx \
    ) { \
        enum { _expected = 0 accounts_table(_ACCOUNT_COUNT) }; \
        if (params->accounts_len < _expected) { \
            log_literal("err: not enough accounts"); \
            return ERROR_NOT_ENOUGH_ACCOUNT_KEYS; \
        } \
        uint64_t _idx = 0; \
        accounts_table(_ACCOUNT_VALIDATE) \
        (void)_idx; \
        return SUCCESS; \
    }

#endif /* ACCOUNT_H */
