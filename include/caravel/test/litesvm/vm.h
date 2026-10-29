#ifndef CARAVEL_TEST_LITESVM_VM_H
#define CARAVEL_TEST_LITESVM_VM_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** 
 * Main runner
 * 
 * A bank that takes signed transactions
*/
typedef LiteSvmHandle Svm;

#define _CVLT_VM      Svm
#define _CVLT_VM_NAME svm

_CVLT_API Svm *_cvlt_vm_new(const char *program, const Pubkey *program_id) {
    Svm *svm = litesvm_new();
    if (!svm) {
        fprintf(stderr, "error: could not create LiteSVM\n");
        return NULL;
    }
    if (litesvm_add_program_from_file(
            svm, program_id->bytes, (const uint8_t *)program, strlen(program)
        ) != 0) {
        char err[256] = {0};
        litesvm_last_error_copy((uint8_t *)err, sizeof err - 1);
        fprintf(stderr, "error: cannot load %s: %s\n", program, err);
        litesvm_free(svm);
        return NULL;
    }
    return svm;
}

_CVLT_API void _cvlt_vm_free(Svm *svm) {
    litesvm_free(svm);
}

static void _cvlt_pubkey_from_seed(const uint8_t seed[32], Pubkey *out) {
    litesvm_pubkey_from_seed(seed, out->bytes);
}

static bool _cvlt_find_pda_raw(
    const uint8_t *const *seeds,
    const size_t *lens,
    size_t n,
    const Pubkey *program_id,
    Pubkey *out,
    uint8_t *bump_out
) {
    return litesvm_find_program_address(
               seeds, lens, n, program_id->bytes, out->bytes, bump_out
           ) == 0;
}

/* `logs` is the LiteSvmTxOutcome a TxResult came from */
static void _cvlt_print_logs(const void *logs) {
    const LiteSvmTxOutcome *o = logs;
    size_t n                  = litesvm_tx_outcome_logs_count(o);
    if (!n) return;
    _cvlt_out("        logs:\n");
    for (size_t i = 0; i < n; i++) {
        char line[512] = {0};
        litesvm_tx_outcome_log_copy(o, i, (uint8_t *)line, sizeof line - 1);
        _cvlt_out("          %s\n", line);
    }
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_LITESVM_VM_H */
