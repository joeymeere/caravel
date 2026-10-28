#ifndef ERROR_H
#define ERROR_H

#include <stdint.h>

#define TO_BUILTIN(error) ((uint64_t)(error) << 32)

#define SUCCESS                                   0
#define ERROR_CUSTOM_ZERO                         TO_BUILTIN(1)  /* deprecated: use ERROR_CUSTOM */
#define ERROR_INVALID_ARGUMENT                    TO_BUILTIN(2)
#define ERROR_INVALID_INSTRUCTION_DATA            TO_BUILTIN(3)
#define ERROR_INVALID_ACCOUNT_DATA                TO_BUILTIN(4)
#define ERROR_ACCOUNT_DATA_TOO_SMALL              TO_BUILTIN(5)
#define ERROR_INSUFFICIENT_FUNDS                  TO_BUILTIN(6)
#define ERROR_INCORRECT_PROGRAM_ID                TO_BUILTIN(7)
#define ERROR_MISSING_REQUIRED_SIGNATURE          TO_BUILTIN(8)
#define ERROR_ACCOUNT_ALREADY_INITIALIZED         TO_BUILTIN(9)
#define ERROR_UNINITIALIZED_ACCOUNT               TO_BUILTIN(10)
#define ERROR_NOT_ENOUGH_ACCOUNT_KEYS             TO_BUILTIN(11)
#define ERROR_ACCOUNT_BORROW_FAILED               TO_BUILTIN(12)
#define ERROR_MAX_SEED_LENGTH_EXCEEDED            TO_BUILTIN(13)
#define ERROR_INVALID_SEEDS                       TO_BUILTIN(14)
#define ERROR_BORSH_IO_ERROR                      TO_BUILTIN(15)
#define ERROR_ACCOUNT_NOT_RENT_EXEMPT             TO_BUILTIN(16)
#define ERROR_UNSUPPORTED_SYSVAR                  TO_BUILTIN(17)
#define ERROR_ILLEGAL_OWNER                       TO_BUILTIN(18)
#define ERROR_MAX_ACCOUNTS_DATA_ALLOCATIONS_EXCEEDED TO_BUILTIN(19)
#define ERROR_INVALID_REALLOC                     TO_BUILTIN(20)
#define ERROR_MAX_INSTRUCTION_TRACE_LENGTH_EXCEEDED TO_BUILTIN(21)
#define ERROR_BUILTIN_PROGRAMS_MUST_CONSUME_COMPUTE_UNITS TO_BUILTIN(22)
#define ERROR_INVALID_ACCOUNT_OWNER               TO_BUILTIN(23)
#define ERROR_ARITHMETIC_OVERFLOW                 TO_BUILTIN(24)
#define ERROR_IMMUTABLE                           TO_BUILTIN(25)
#define ERROR_INCORRECT_AUTHORITY                 TO_BUILTIN(26)

/**
 * Custom errors surface as `Custom(ERROR_CUSTOM_BASE + x)`. Caravel's own
 * errors (below) use ERROR_CUSTOM(0)..ERROR_CUSTOM(7); pick numbers outside
 * that range for your program's errors.
 *
 * @code return ERROR_CUSTOM(42);   // -> Custom(298)
 */
#define ERROR_CUSTOM_BASE 0x100
#define ERROR_CUSTOM(x)   ((uint64_t)(ERROR_CUSTOM_BASE + (x)))

#define ERROR_ACCOUNT_NOT_SIGNER     ERROR_CUSTOM(0)
#define ERROR_ACCOUNT_NOT_WRITABLE   ERROR_CUSTOM(1)
#define ERROR_ACCOUNT_WRONG_OWNER    ERROR_CUSTOM(2)
#define ERROR_ACCOUNT_WRONG_KEY      ERROR_CUSTOM(3)
#define ERROR_INVALID_PDA            ERROR_CUSTOM(4)
#define ERROR_UNKNOWN_INSTRUCTION    ERROR_CUSTOM(5)
#define ERROR_ACCOUNT_NOT_EXECUTABLE ERROR_CUSTOM(6)
#define ERROR_DUPLICATE_ACCOUNT      ERROR_CUSTOM(7)

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
 * @note `code` must be a compile-time constant. Not supported on SBPF v2
 *
 * @code EXIT(ERROR_INVALID_ARGUMENT);
 */
#define EXIT(code) \
    do { \
        __asm__ volatile("lddw r0, %0\n\texit" :: "i"((uint64_t)(code))); \
        __builtin_unreachable(); \
    } while (0)

#endif /* ERROR_H */
