#ifndef CARAVEL_TEST_LITESVM_TX_H
#define CARAVEL_TEST_LITESVM_TX_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** Outcome of a sent transaction. Valid until the current test ends */
typedef struct {
    bool ok;
    uint64_t compute_units;
    TestError error;
    const void *logs;          /* the outcome, for failure output */
    LiteSvmTxOutcome *outcome; /* for anything else litesvm.h exposes */
} TxResult;

_CVLT_API void _cvlt_free_outcome(void *o) {
    litesvm_tx_outcome_free(o);
}

_CVLT_API TxResult _cvlt_send(
    Svm *svm,
    const Ix *ixs,
    size_t n,
    const Keypair *const *signers,
    size_t nsigners,
    bool simulate,
    const char *file,
    int line
) {
    LiteSvmInstruction raw[16];
    LiteSvmAccountMeta metas[16][CVL_TEST_MAX_ACCOUNTS];
    uint8_t seeds[16 * 32];
    if (n > 16) _cvlt_fail(file, line, "at most 16 instructions per tx");
    if (nsigners > 16) _cvlt_fail(file, line, "at most 16 signers");
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
    for (size_t i = 0; i < nsigners; i++)
        memcpy(seeds + 32 * i, signers[i]->seed, 32);

    LiteSvmTxOutcome *o =
        simulate ? litesvm_simulate_instructions(svm, raw, n, seeds, nsigners)
                 : litesvm_send_instructions(svm, raw, n, seeds, nsigners);
    if (!o) {
        char err[256] = {0};
        litesvm_last_error_copy((uint8_t *)err, sizeof err - 1);
        _cvlt_fail(file, line, "could not build transaction: %s", err);
    }
    /* identical later transactions would otherwise be AlreadyProcessed */
    if (!simulate) litesvm_expire_blockhash(svm);

    _cvlt_own(o, _cvlt_free_outcome, file, line);

    LiteSvmTxError e;
    litesvm_tx_outcome_error_info(o, &e);
    TxResult r;
    r.outcome                 = o;
    r.logs                    = o;
    r.ok                      = litesvm_tx_outcome_is_ok(o) == 1;
    r.compute_units           = litesvm_tx_outcome_compute_units(o);
    r.error.kind              = e.kind;
    r.error.code              = e.code;
    r.error.instruction_index = e.instruction_index;
    r.error.program_error     = e.program_error;
    return r;
}

#define _CVLT_SIGNERS(...) \
    (const Keypair *const[]){__VA_ARGS__}, \
        sizeof((const Keypair *const[]){__VA_ARGS__}) / sizeof(Keypair *)

/** Send one instruction - followed by signers, where the first pays fees */
#define SEND(svm, ix, ...) \
    _cvlt_send( \
        (svm), &(ix), 1, _CVLT_SIGNERS(__VA_ARGS__), false, __FILE__, __LINE__ \
    )

/** Send several instructions (an Ix array) in one transaction */
#define SEND_ALL(svm, ixs, ...) \
    _cvlt_send( \
        (svm), (ixs), sizeof(ixs) / sizeof(Ix), _CVLT_SIGNERS(__VA_ARGS__), \
        false, __FILE__, __LINE__ \
    )

/** `SEND`, but without committing state */
#define SIMULATE(svm, ix, ...) \
    _cvlt_send( \
        (svm), &(ix), 1, _CVLT_SIGNERS(__VA_ARGS__), true, __FILE__, __LINE__ \
    )

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_LITESVM_TX_H */
