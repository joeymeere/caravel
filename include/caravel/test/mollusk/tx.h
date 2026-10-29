#ifndef CARAVEL_TEST_MOLLUSK_TX_H
#define CARAVEL_TEST_MOLLUSK_TX_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** Outcome of processed instructions. Valid until the current test ends */
typedef struct {
    bool ok;
    uint64_t compute_units;
    TestError error;
    const void *logs;      /* the result, for failure output */
    MolluskResult *result; /* for anything else mollusk.h exposes */
} IxResult;

enum { _CVLT_SINGLE, _CVLT_CHAIN, _CVLT_TX };

_CVLT_API void _cvlt_free_result(void *r) {
    mollusk_result_free(r);
}

_CVLT_API IxResult _cvlt_process(
    Ctx *ctx,
    const Ix *ixs,
    size_t n,
    int mode,
    const Pubkey *payer,
    const char *file,
    int line
) {
    MolluskInstruction raw[16];
    MolluskAccountMeta metas[16][CVL_TEST_MAX_ACCOUNTS];
    if (n > 16) _cvlt_fail(file, line, "at most 16 instructions at once");
    for (size_t i = 0; i < n; i++) {
        if (ixs[i].error)
            _cvlt_fail(file, line, "instruction %zu: %s", i, ixs[i].error);
        for (size_t a = 0; a < ixs[i].accounts_len; a++) {
            metas[i][a].pubkey      = ixs[i].keys[a].bytes;
            metas[i][a].is_signer   = ixs[i].is_signer[a];
            metas[i][a].is_writable = ixs[i].is_writable[a];
        }
        raw[i].program_id   = ixs[i].program_id.bytes;
        raw[i].accounts     = metas[i];
        raw[i].accounts_len = ixs[i].accounts_len;
        raw[i].data         = ixs[i].data;
        raw[i].data_len     = ixs[i].data_len;
    }

    MolluskResult *res =
        mode == _CVLT_SINGLE ? mollusk_context_process_instruction(ctx, raw)
        : mode == _CVLT_CHAIN
            ? mollusk_context_process_instruction_chain(ctx, raw, n)
            : mollusk_context_process_transaction_instructions(
                  ctx, raw, n, payer ? payer->bytes : NULL
              );
    if (!res)
        _cvlt_fail(
            file, line, "could not process instruction: %s",
            _cvlt_mollusk_error()
        );
    _cvlt_own(res, _cvlt_free_result, file, line);

    MolluskError e;
    mollusk_result_error_info(res, &e);
    IxResult r;
    r.result                  = res;
    r.logs                    = res;
    r.ok                      = mollusk_result_is_ok(res) == 1;
    r.compute_units           = mollusk_result_compute_units(res);
    r.error.kind              = e.kind;
    r.error.code              = e.code;
    r.error.instruction_index = e.instruction_index;
    r.error.program_error     = e.program_error;
    return r;
}

/**
 * Process one instruction against the account store
 */
#define PROCESS(ctx, ix) \
    _cvlt_process((ctx), &(ix), 1, _CVLT_SINGLE, NULL, __FILE__, __LINE__)

/**
 * Process an Ix array one instruction at a time, each seeing the last one's accounts
 */
#define PROCESS_CHAIN(ctx, ixs) \
    _cvlt_process( \
        (ctx), (ixs), sizeof(ixs) / sizeof(Ix), _CVLT_CHAIN, NULL, __FILE__, \
        __LINE__ \
    )

/**
 * Process an Ix array as one atomic transaction
 */
#define PROCESS_TX(ctx, ixs, payer) \
    _cvlt_process( \
        (ctx), (ixs), sizeof(ixs) / sizeof(Ix), _CVLT_TX, (payer), __FILE__, \
        __LINE__ \
    )

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_MOLLUSK_TX_H */
