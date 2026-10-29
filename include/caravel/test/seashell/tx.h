#ifndef CARAVEL_TEST_SEASHELL_TX_H
#define CARAVEL_TEST_SEASHELL_TX_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** Outcome of a processed instruction. Valid until the current test ends */
typedef struct {
    bool ok;
    uint64_t compute_units;
    TestError error;
    const void *logs;       /* the result, for failure output */
    SeashellResult *result; /* for anything else seashell.h exposes */
} IxResult;

_CVLT_API void _cvlt_free_result(void *r) {
    seashell_result_free(r);
}

_CVLT_API IxResult
_cvlt_process(Shell *shell, const Ix *ix, const char *file, int line) {
    SeashellAccountMeta metas[CVL_TEST_MAX_ACCOUNTS];
    if (ix->error) _cvlt_fail(file, line, "instruction: %s", ix->error);
    for (size_t a = 0; a < ix->accounts_len; a++) {
        metas[a].pubkey      = ix->keys[a].bytes;
        metas[a].is_signer   = ix->is_signer[a];
        metas[a].is_writable = ix->is_writable[a];
    }
    SeashellInstruction raw = {
        ix->program_id.bytes, metas, ix->accounts_len, ix->data, ix->data_len
    };

    SeashellResult *res = seashell_process_instruction(shell, &raw);
    if (!res)
        _cvlt_fail(
            file, line, "could not process instruction: %s",
            _cvlt_seashell_error()
        );
    _cvlt_own(res, _cvlt_free_result, file, line);

    SeashellError e;
    seashell_result_error_info(res, &e);
    IxResult r;
    r.result                  = res;
    r.logs                    = res;
    r.ok                      = seashell_result_is_ok(res) == 1;
    r.compute_units           = seashell_result_compute_units(res);
    r.error.kind              = e.kind;
    r.error.code              = e.code;
    r.error.instruction_index = e.instruction_index;
    r.error.program_error     = e.program_error;
    return r;
}

/**
 * Process a single instruction
 */
#define PROCESS(shell, ix) _cvlt_process((shell), &(ix), __FILE__, __LINE__)

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_SEASHELL_TX_H */
