#ifndef ERROR_H
#define ERROR_H

#define SUCCESS                         0
#define ERROR_CUSTOM_ZERO               1  /* Deprecated: use ERROR_CUSTOM */
#define ERROR_INVALID_ARGUMENT          2
#define ERROR_INVALID_INSTRUCTION_DATA  3
#define ERROR_INVALID_ACCOUNT_DATA      4
#define ERROR_ACCOUNT_DATA_TOO_SMALL    5
#define ERROR_INSUFFICIENT_FUNDS        6
#define ERROR_INCORRECT_PROGRAM_ID      7
#define ERROR_MISSING_REQUIRED_SIGNATURE 8
#define ERROR_ACCOUNT_ALREADY_INITIALIZED 9
#define ERROR_UNINITIALIZED_ACCOUNT     10
#define ERROR_NOT_ENOUGH_ACCOUNT_KEYS   11
#define ERROR_ACCOUNT_BORROW_FAILED     12

/**
 * Custom errors start from this base. Pass your error number (0-4095)
 * to get a unique error code.
 *
 * @code return ERROR_CUSTOM(42);
 */
#define ERROR_CUSTOM_BASE 0x100
#define ERROR_CUSTOM(x)   ((uint64_t)(ERROR_CUSTOM_BASE + (x)))

#define ERROR_ACCOUNT_NOT_SIGNER     ERROR_CUSTOM(0)
#define ERROR_ACCOUNT_NOT_WRITABLE   ERROR_CUSTOM(1)
#define ERROR_ACCOUNT_WRONG_OWNER    ERROR_CUSTOM(2)
#define ERROR_ACCOUNT_WRONG_KEY      ERROR_CUSTOM(3)
#define ERROR_INVALID_PDA            ERROR_CUSTOM(4)
#define ERROR_UNKNOWN_INSTRUCTION    ERROR_CUSTOM(5)

/**
 * Early return if expression is non-zero.
 *
 * @param expr The expression to evaluate
 * @return The result of the expression
 *
 * @code TRY(some_function());
 */
#define TRY(expr) \
    do { \
        uint64_t _err = (expr); \
        if (_err != SUCCESS) return _err; \
    } while (0)

/**
 * Immediately eject from the program with a specific code
 *
 * @param code The code to exit with
 *
 * @warning This should generally be avoided in favor of returning an error code and 
 *          handling upstream. The primary use case is to prevent the compiler from
 *          hoisting error-code loads into hotpaths
 *
 * @code EXIT(ERROR_INVALID_ARGUMENT);
 */
#define EXIT(code) \
    do { \
        __asm__ volatile("lddw r0, " #code "\nexit"); \
        __builtin_unreachable(); \
    } while (0)

#endif /* ERROR_H */
