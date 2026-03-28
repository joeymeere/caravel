#ifndef CVL_ACCOUNT_H
#define CVL_ACCOUNT_H

#include "types.h"
#include "error.h"
#include "log.h"

#define CVL_ACCOUNT_KEY(acc)        ((acc)->key)
#define CVL_ACCOUNT_LAMPORTS(acc)   (*(acc)->lamports)
#define CVL_ACCOUNT_DATA(acc)       ((acc)->data)
#define CVL_ACCOUNT_DATA_LEN(acc)   ((acc)->data_len)
#define CVL_ACCOUNT_OWNER(acc)      ((acc)->owner)
#define CVL_ACCOUNT_IS_SIGNER(acc)  ((acc)->is_signer)
#define CVL_ACCOUNT_IS_WRITABLE(acc)((acc)->is_writable)
#define CVL_ACCOUNT_EXECUTABLE(acc) ((acc)->executable)

/**
 * Cast account data to a typed struct pointer (zero-copy).
 *
 * @param acc The account to cast
 * @param type The type to cast to (e.g. MyState)
 * @return The pointer to the casted data
 *
 * @code: MyState *state = CVL_ACCOUNT_STATE(acc, MyState);
 */
#define CVL_ACCOUNT_STATE(acc, type) ((type *)((acc)->data))

#define CVL_SIGNER   (1 << 0)
#define CVL_WRITABLE (1 << 1)
#define CVL_PROGRAM  (1 << 2)

#define CVL_ASSERT_SIGNER(acc) \
    do { \
        if (!(acc)->is_signer) { \
            cvl_log_literal("err: account not signer"); \
            return CVL_ERROR_MISSING_REQUIRED_SIGNATURE; \
        } \
    } while (0)

#define CVL_ASSERT_WRITABLE(acc) \
    do { \
        if (!(acc)->is_writable) { \
            cvl_log_literal("err: account not writable"); \
            return CVL_ERROR_ACCOUNT_NOT_WRITABLE; \
        } \
    } while (0)

#define CVL_ASSERT_OWNER(acc, expected_owner) \
    do { \
        if (!cvl_pubkey_eq((acc)->owner, (expected_owner))) { \
            cvl_log_literal("err: wrong account owner"); \
            return CVL_ERROR_ACCOUNT_WRONG_OWNER; \
        } \
    } while (0)

#define CVL_ASSERT_KEY(acc, expected_key) \
    do { \
        if (!cvl_pubkey_eq((acc)->key, (expected_key))) { \
            cvl_log_literal("err: wrong account key"); \
            return CVL_ERROR_ACCOUNT_WRONG_KEY; \
        } \
    } while (0)

#define CVL_ASSERT_INITIALIZED(acc) \
    do { \
        if ((acc)->data_len == 0) { \
            cvl_log_literal("err: account not initialized"); \
            return CVL_ERROR_UNINITIALIZED_ACCOUNT; \
        } \
    } while (0)

#define CVL_ASSERT_DATA_LEN(acc, min_len) \
    do { \
        if ((acc)->data_len < (min_len)) { \
            cvl_log_literal("err: account data too small"); \
            return CVL_ERROR_ACCOUNT_DATA_TOO_SMALL; \
        } \
    } while (0)

/**
 * X-macro system for defining instruction account contexts.
 * Accounts are assigned indices automatically based on declaration order.
 *
 * 1. Define your accounts table:
 * @code:
 *    #define MY_ACCOUNTS(X) \
 *        X(payer,          CVL_SIGNER | CVL_WRITABLE) \
 *        X(counter,        CVL_WRITABLE) \
 *        X(system_program, CVL_PROGRAM)
 *
 * 2. Generate the struct + parser:
 * @code: CVL_DEFINE_ACCOUNTS(my_instruction, MY_ACCOUNTS)
 *
 * This produces:
 * @code:
 *    typedef struct { CvlAccountInfo *payer, *counter, *system_program; }
 *      my_instruction_accounts_t;
 *    static inline uint64_t my_instruction_parse(CvlParameters *params,
 *      my_instruction_accounts_t *ctx) which validates flags and populates ctx.
 */

#define _CVL_ACCOUNT_FIELD(name, flags) \
    CvlAccountInfo *name;

#define _CVL_ACCOUNT_COUNT(name, flags) + 1

/* generate validation + assignment with auto-incrementing index */
#define _CVL_ACCOUNT_VALIDATE(name, flags) \
    ctx->name = &params->accounts[_cvl_idx]; \
    if (((flags) & CVL_SIGNER) && !ctx->name->is_signer) { \
        cvl_log_literal("err: " #name " must be signer"); \
        return CVL_ERROR_MISSING_REQUIRED_SIGNATURE; \
    } \
    if (((flags) & CVL_WRITABLE) && !ctx->name->is_writable) { \
        cvl_log_literal("err: " #name " must be writable"); \
        return CVL_ERROR_ACCOUNT_NOT_WRITABLE; \
    } \
    _cvl_idx++;

/**
 * Main macro: generates account context struct + parse function.
 */
#define CVL_DEFINE_ACCOUNTS(prefix, accounts_table) \
    typedef struct { \
        accounts_table(_CVL_ACCOUNT_FIELD) \
    } prefix##_accounts_t; \
    \
    static inline uint64_t prefix##_parse( \
        CvlParameters *params, \
        prefix##_accounts_t *ctx \
    ) { \
        enum { _cvl_expected = 0 accounts_table(_CVL_ACCOUNT_COUNT) }; \
        if (params->accounts_len < _cvl_expected) { \
            cvl_log_literal("err: not enough accounts"); \
            return CVL_ERROR_NOT_ENOUGH_ACCOUNT_KEYS; \
        } \
        uint64_t _cvl_idx = 0; \
        accounts_table(_CVL_ACCOUNT_VALIDATE) \
        (void)_cvl_idx; \
        return CVL_SUCCESS; \
    }

#endif /* CVL_ACCOUNT_H */
