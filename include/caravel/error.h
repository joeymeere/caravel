#ifndef CVL_ERROR_H
#define CVL_ERROR_H

#define CVL_SUCCESS                         0
#define CVL_ERROR_CUSTOM_ZERO               1  /* Deprecated: use CVL_ERROR_CUSTOM */
#define CVL_ERROR_INVALID_ARGUMENT          2
#define CVL_ERROR_INVALID_INSTRUCTION_DATA  3
#define CVL_ERROR_INVALID_ACCOUNT_DATA      4
#define CVL_ERROR_ACCOUNT_DATA_TOO_SMALL    5
#define CVL_ERROR_INSUFFICIENT_FUNDS        6
#define CVL_ERROR_INCORRECT_PROGRAM_ID      7
#define CVL_ERROR_MISSING_REQUIRED_SIGNATURE 8
#define CVL_ERROR_ACCOUNT_ALREADY_INITIALIZED 9
#define CVL_ERROR_UNINITIALIZED_ACCOUNT     10
#define CVL_ERROR_NOT_ENOUGH_ACCOUNT_KEYS   11
#define CVL_ERROR_ACCOUNT_BORROW_FAILED     12

/**
 * Custom errors start from this base. Pass your error number (0-4095)
 * to get a unique error code.
 *
 * @usage: return CVL_ERROR_CUSTOM(42);
 */
#define CVL_ERROR_CUSTOM_BASE 0x100
#define CVL_ERROR_CUSTOM(x)   ((uint64_t)(CVL_ERROR_CUSTOM_BASE + (x)))

#define CVL_ERROR_ACCOUNT_NOT_SIGNER     CVL_ERROR_CUSTOM(0)
#define CVL_ERROR_ACCOUNT_NOT_WRITABLE   CVL_ERROR_CUSTOM(1)
#define CVL_ERROR_ACCOUNT_WRONG_OWNER    CVL_ERROR_CUSTOM(2)
#define CVL_ERROR_ACCOUNT_WRONG_KEY      CVL_ERROR_CUSTOM(3)
#define CVL_ERROR_INVALID_PDA            CVL_ERROR_CUSTOM(4)
#define CVL_ERROR_UNKNOWN_INSTRUCTION    CVL_ERROR_CUSTOM(5)

/**
 * Early return if expression is non-zero.
 *
 * @param expr The expression to evaluate
 * @return The result of the expression
 *
 * @usage: CVL_TRY(some_function());
 */
#define CVL_TRY(expr) \
    do { \
        uint64_t _cvl_err = (expr); \
        if (_cvl_err != CVL_SUCCESS) return _cvl_err; \
    } while (0)

#endif /* CVL_ERROR_H */
