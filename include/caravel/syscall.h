#ifndef CVL_SYSCALL_H
#define CVL_SYSCALL_H

#include "types.h"

/**
 * Prints a string to stdout
 */
extern void sol_log_(const char *message, uint64_t len);

/**
 * Prints 64 bit values represented in hexadecimal to stdout
 */
extern void sol_log_64_(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                        uint64_t arg4, uint64_t arg5);

/**
 * Prints the hexadecimal representation of a public key
 *
 * @param key The public key to print
 */
extern void sol_log_pubkey(const CvlPubkey *pubkey);

/**
 * Prints the current compute unit consumption to stdout
 */
extern void sol_log_compute_units_();

/**
 * Print the base64 representation of some arrays.
 */
extern void sol_log_data(const uint8_t *data[], const uint64_t data_len[], uint64_t count);

/**
 * Copies memory
 */
extern void *sol_memcpy_(void *dest, const void *src, uint64_t n);

/**
 * Moves memory
 */
extern void *sol_memmove_(void *dest, const void *src, uint64_t n);

/**
 * Fill a byte string with a byte value
 */
extern void *sol_memset_(void *dest, uint8_t val, uint64_t n);

/**
 * Compares memory
 */
extern int   sol_memcmp_(const void *a, const void *b, uint64_t n, int *result);

/**
 * Invoke another program and sign for some of the keys
 *
 * @note For `sol_invoke`, call this fn with empty seeds
 *
 * @param instruction Instruction to process
 * @param account_infos Accounts used by instruction
 * @param account_infos_len Length of account_infos array
 * @param seeds Seed bytes used to sign program accounts
 * @param seeds_len Length of the seeds array
 */
extern uint64_t sol_invoke_signed_c(
    const CvlInstruction   *instruction,
    const CvlAccountInfo   *account_infos,
    int                     account_infos_len,
    const CvlSignerSeeds   *signer_seeds,
    int                     signer_seeds_len
);

/**
 * Create a program address
 *
 * @param seeds Seed bytes used to sign program accounts
 * @param seeds_len Length of the seeds array
 * @param program_id Program id of the signer
 * @param program_address Program address created, filled on return
 */
extern uint64_t sol_create_program_address(
    const CvlSignerSeed *seeds,
    int                  seeds_len,
    const CvlPubkey     *program_id,
    CvlPubkey           *address
);

/**
 * Try to find a program address and return corresponding bump seed
 *
 * @param seeds Seed bytes used to sign program accounts
 * @param seeds_len Length of the seeds array
 * @param program_id Program id of the signer
 * @param program_address Program address created, filled on return
 * @param bump_seed Bump seed required to create a valid program address
 */
extern uint64_t sol_try_find_program_address(
    const CvlSignerSeed *seeds,
    int                  seeds_len,
    const CvlPubkey     *program_id,
    CvlPubkey           *address,
    uint8_t             *bump_seed
);

/**
 * Sha256
 *
 * @param vals Array of {addr, len} pairs (same layout as CvlSignerSeed)
 * @param vals_len Number of pairs
 * @param result 32 byte array to hold the result
 */
extern uint64_t sol_sha256(
    const CvlSignerSeed *vals,
    uint64_t              vals_len,
    uint8_t               result[32]
);

/**
 * Keccak256
 *
 * @param vals Array of {addr, len} pairs (same layout as CvlSignerSeed)
 * @param vals_len Number of pairs
 * @param result 32 byte array to hold the result
 */
extern uint64_t sol_keccak256(
    const CvlSignerSeed *vals,
    uint64_t              vals_len,
    uint8_t               result[32]
);

/**
 * Get the clock sysvar
 *
 * @param clock Pointer to a CvlClock struct to fill
 * @return CVL_SUCCESS on success
 */
extern uint64_t sol_get_clock_sysvar(void *clock);

/**
 * Get the rent sysvar
 *
 * @param rent Pointer to a CvlRent struct to fill
 * @return CVL_SUCCESS on success
 */
extern uint64_t sol_get_rent_sysvar(void *rent);

/**
 * Get the return data
 *
 * @param data Pointer to a uint8_t array to fill
 * @param data_len Length of the data array
 * @param program_id Program id of the signer
 * @return CVL_SUCCESS on success
 */
extern uint64_t sol_get_return_data(
    uint8_t   *data,
    uint64_t   data_len,
    CvlPubkey *program_id
);

/**
 * Set the return data
 *
 * @param data Pointer to a uint8_t array to set
 * @param data_len Length of the data array
 */
extern void sol_set_return_data(
    const uint8_t *data,
    uint64_t       data_len
);

/**
 * Get the stack height
 *
 * @return The stack height
 */
extern uint64_t sol_get_stack_height(void);

#endif /* CVL_SYSCALL_H */
