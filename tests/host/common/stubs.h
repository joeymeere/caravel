/**
 * Host implementations of the syscalls the headers call, plus hooks for
 * tests to inspect what a program did (logs, CPIs) and feed it sysvars.
 */
#ifndef CVL_STUBS_H
#define CVL_STUBS_H

#include <caravel.h>

/* last CPI passed to sol_invoke_signed_c, deep-copied */
#define STUB_MAX_METAS 16
#define STUB_MAX_DATA  256
#define STUB_MAX_SEEDS 4

typedef struct {
    int calls;
    Pubkey program_id;
    uint64_t metas_len;
    Pubkey meta_keys[STUB_MAX_METAS];
    bool meta_writable[STUB_MAX_METAS];
    bool meta_signer[STUB_MAX_METAS];
    uint64_t data_len;
    uint8_t data[STUB_MAX_DATA];
    uint64_t signers_len; /* number of SignerSeeds groups */
    uint64_t seeds_len[STUB_MAX_SEEDS];
    uint64_t return_value; /* what the stub returns to the program */
} StubCpi;

extern StubCpi stub_cpi;

/* last message passed to sol_log_ */
extern char stub_last_log[256];
extern int stub_log_calls;

/* values returned by sol_get_{rent,clock}_sysvar */
extern Rent stub_rent;
extern Clock stub_clock;

void stub_reset(void);

#endif /* CVL_STUBS_H */
