#ifndef CVL_SYSCALL_H
#define CVL_SYSCALL_H

#include "types.h"

extern void sol_log_(const char *message, uint64_t len);
extern void sol_log_64_(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                        uint64_t arg4, uint64_t arg5);
extern void sol_log_pubkey(const CvlPubkey *pubkey);
extern void sol_log_compute_units_();
extern void sol_log_data(const uint8_t *data[], const uint64_t data_len[], uint64_t count);

extern void *sol_memcpy_(void *dest, const void *src, uint64_t n);
extern void *sol_memmove_(void *dest, const void *src, uint64_t n);
extern void *sol_memset_(void *dest, uint8_t val, uint64_t n);
extern int   sol_memcmp_(const void *a, const void *b, uint64_t n, int *result);

extern uint64_t sol_invoke_signed_c(
    const CvlInstruction   *instruction,
    const CvlAccountInfo   *account_infos,
    int                     account_infos_len,
    const CvlSignerSeeds   *signer_seeds,
    int                     signer_seeds_len
);

extern uint64_t sol_create_program_address(
    const CvlSignerSeed *seeds,
    int                  seeds_len,
    const CvlPubkey     *program_id,
    CvlPubkey           *address
);

extern uint64_t sol_try_find_program_address(
    const CvlSignerSeed *seeds,
    int                  seeds_len,
    const CvlPubkey     *program_id,
    CvlPubkey           *address,
    uint8_t             *bump_seed
);

extern uint64_t sol_sha256(
    const uint8_t *bytes[],
    const uint64_t sizes[],
    uint64_t       count,
    uint8_t        result[32]
);

extern uint64_t sol_keccak256(
    const uint8_t *bytes[],
    const uint64_t sizes[],
    uint64_t       count,
    uint8_t        result[32]
);

extern uint64_t sol_get_clock_sysvar(void *clock);
extern uint64_t sol_get_rent_sysvar(void *rent);

extern uint64_t sol_get_return_data(
    uint8_t   *data,
    uint64_t   data_len,
    CvlPubkey *program_id
);

extern void sol_set_return_data(
    const uint8_t *data,
    uint64_t       data_len
);

extern uint64_t sol_get_stack_height(void);

#endif /* CVL_SYSCALL_H */
