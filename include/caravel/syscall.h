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

/**
 * Get remaining compute units in the current transaction
 */
extern uint64_t sol_remaining_compute_units(void);

/**
 * Blake3
 *
 * @param vals Array of {addr, len} pairs (same layout as CvlSignerSeed)
 * @param vals_len Number of pairs
 * @param result 32 byte array to hold the result
 */
extern uint64_t sol_blake3(
    const CvlSignerSeed *vals,
    uint64_t              vals_len,
    uint8_t               result[32]
);

/**
 * Poseidon hash (ZK-friendly)
 *
 * @param parameters Hash parameters (1 = Bn254X5)
 * @param endianness 0 = big-endian, 1 = little-endian
 * @param vals Array of {addr, len} pairs (same layout as CvlSignerSeed)
 * @param vals_len Number of pairs
 * @param result 32 byte array to hold the result
 */
extern uint64_t sol_poseidon(
    uint64_t              parameters,
    uint64_t              endianness,
    const CvlSignerSeed *vals,
    uint64_t              vals_len,
    uint8_t               result[32]
);

/**
 * Recover a secp256k1 public key from a signed message hash
 *
 * @param hash 32-byte message hash
 * @param recovery_id Recovery ID (0–3)
 * @param signature 64-byte signature (r, s)
 * @param result 64-byte recovered public key (x, y)
 * @return 0 on success
 */
extern uint64_t sol_secp256k1_recover(
    const uint8_t *hash,
    uint64_t       recovery_id,
    const uint8_t *signature,
    uint8_t       *result
);

/**
 * Validate a point on an elliptic curve
 *
 * @param curve_id 0 = Ed25519, 1 = Ristretto
 * @param point Compressed point bytes
 * @param result Unused (pass NULL)
 * @return 0 if valid
 */
extern uint64_t sol_curve_validate_point(
    uint64_t       curve_id,
    const uint8_t *point,
    uint8_t       *result
);

/**
 * Elliptic curve group operation (add / subtract / multiply)
 *
 * @param curve_id 0 = Ed25519, 1 = Ristretto
 * @param group_op 0 = add, 1 = subtract, 2 = multiply
 * @param left Left input point
 * @param right Right input point (or scalar for multiply)
 * @param result Result point
 */
extern uint64_t sol_curve_group_op(
    uint64_t       curve_id,
    uint64_t       group_op,
    const uint8_t *left,
    const uint8_t *right,
    uint8_t       *result
);

/**
 * Elliptic curve multi-scalar multiplication
 *
 * @param curve_id 0 = Ed25519, 1 = Ristretto
 * @param scalars Packed scalar bytes
 * @param points Packed point bytes
 * @param points_len Number of point/scalar pairs
 * @param result Result point
 */
extern uint64_t sol_curve_multiscalar_mul(
    uint64_t       curve_id,
    const uint8_t *scalars,
    const uint8_t *points,
    uint64_t       points_len,
    uint8_t       *result
);

/**
 * Alt_bn128 group operation (EVM-compatible)
 *
 * @param group_op 0 = add, 1 = multiply, 2 = pairing
 * @param input Serialized input
 * @param input_size Input size in bytes
 * @param result Result buffer (64 bytes for add/mul, 32 for pairing)
 */
extern uint64_t sol_alt_bn128_group_op(
    uint64_t       group_op,
    const uint8_t *input,
    uint64_t       input_size,
    uint8_t       *result
);

/**
 * Alt_bn128 point compression / decompression
 *
 * @param op 0 = compress G1, 1 = decompress G1, 2 = compress G2, 3 = decompress G2
 * @param input Input point bytes
 * @param input_size Input size in bytes
 * @param result Output buffer
 */
extern uint64_t sol_alt_bn128_compression(
    uint64_t       op,
    const uint8_t *input,
    uint64_t       input_size,
    uint8_t       *result
);

/**
 * Big integer modular exponentiation
 *
 * @param params Serialized {base_len(32), exp_len(32), mod_len(32), base, exp, mod}
 * @param result Output buffer (mod_len bytes)
 */
extern uint64_t sol_big_mod_exp(
    const uint8_t *params,
    uint8_t       *result
);

/**
 * Get the epoch schedule sysvar
 */
extern uint64_t sol_get_epoch_schedule_sysvar(void *epoch_schedule);

/**
 * Get the epoch rewards sysvar
 */
extern uint64_t sol_get_epoch_rewards_sysvar(void *epoch_rewards);

/**
 * Get the last restart slot
 *
 * @param slot Pointer to a uint64_t to fill
 */
extern uint64_t sol_get_last_restart_slot(void *slot);

/**
 * Generic sysvar accessor — read arbitrary byte ranges from any sysvar
 *
 * @param sysvar_id 32-byte sysvar public key
 * @param result Buffer to receive data
 * @param offset Byte offset into the sysvar
 * @param length Number of bytes to read
 */
extern uint64_t sol_get_sysvar(
    const CvlPubkey *sysvar_id,
    uint8_t         *result,
    uint64_t         offset,
    uint64_t         length
);

/**
 * Get a processed sibling instruction from the current transaction
 *
 * @param index Instruction index (0 = most recent before current)
 * @param meta Pointer to {uint64_t data_len; uint64_t accounts_len;} — filled by runtime;
 *             set data_len/accounts_len to your buffer capacities before calling
 * @param program_id Filled with the instruction's program ID
 * @param data Buffer to receive instruction data
 * @param accounts Buffer to receive account metas ({pubkey[32], is_signer, is_writable} each)
 * @return 1 if found, 0 if index out of range
 */
extern uint64_t sol_get_processed_sibling_instruction(
    uint64_t   index,
    void      *meta,
    CvlPubkey *program_id,
    uint8_t   *data,
    void      *accounts
);

/**
 * Get total active stake for a vote account (or the entire cluster)
 *
 * @param vote_address Vote account pubkey, or NULL for total cluster stake
 * @return Active stake in lamports
 */
extern uint64_t sol_get_epoch_stake(const CvlPubkey *vote_address);

/**
 * Abort the program immediately
 */
extern void abort(void);

/**
 * Panic with a message, source location
 *
 * @param message Error message string
 * @param len Message length
 * @param line Source line number
 * @param column Source column number
 */
extern void sol_panic_(const char *message, uint64_t len,
                       uint64_t line, uint64_t column);

#endif /* CVL_SYSCALL_H */
