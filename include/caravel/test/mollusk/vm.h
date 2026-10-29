#ifndef CARAVEL_TEST_MOLLUSK_VM_H
#define CARAVEL_TEST_MOLLUSK_VM_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/**
 * Main runner
 *
 * A Mollusk harness with MolluskContext as an account store. 
 * Instructions load their accounts from the store and write them back on success
 */
typedef MolluskContextHandle Ctx;

#define _CVLT_VM      Ctx
#define _CVLT_VM_NAME ctx

/* the harness behind the store, for settings (compute budget, sysvars) */
_CVLT_API MolluskHandle *_cvlt_mollusk(Ctx *ctx) {
    return mollusk_context_mollusk(ctx);
}

_CVLT_API const char *_cvlt_mollusk_error(void) {
    static char err[256];
    memset(err, 0, sizeof err);
    mollusk_last_error_copy((uint8_t *)err, sizeof err - 1);
    return err;
}

_CVLT_API Ctx *_cvlt_vm_new(const char *program, const Pubkey *program_id) {
    MolluskHandle *m = mollusk_new();
    if (!m) {
        fprintf(stderr, "error: could not create Mollusk\n");
        return NULL;
    }
    if (mollusk_add_program_from_file(
            m, program_id->bytes, (const uint8_t *)program, strlen(program)
        ) != 0) {
        fprintf(
            stderr, "error: cannot load %s: %s\n", program,
            _cvlt_mollusk_error()
        );
        mollusk_free(m);
        return NULL;
    }
    if (mollusk_add_token_programs(m) != 0) {
        fprintf(
            stderr, "error: cannot load SPL programs: %s\n",
            _cvlt_mollusk_error()
        );
        mollusk_free(m);
        return NULL;
    }
    Ctx *ctx = mollusk_context_new(m);
    if (!ctx)
        fprintf(
            stderr, "error: could not create a Mollusk context: %s\n",
            _cvlt_mollusk_error()
        );
    return ctx;
}

_CVLT_API void _cvlt_vm_free(Ctx *ctx) {
    mollusk_context_free(ctx);
}

static void _cvlt_pubkey_from_seed(const uint8_t seed[32], Pubkey *out) {
    mollusk_pubkey_from_seed(seed, out->bytes);
}

static bool _cvlt_find_pda_raw(
    const uint8_t *const *seeds,
    const size_t *lens,
    size_t n,
    const Pubkey *program_id,
    Pubkey *out,
    uint8_t *bump_out
) {
    return mollusk_find_program_address(
               seeds, lens, n, program_id->bytes, out->bytes, bump_out
           ) == 0;
}

/* `logs` is the MolluskResult an IxResult came from */
static void _cvlt_print_logs(const void *logs) {
    const MolluskResult *r = logs;
    size_t n               = mollusk_result_logs_count(r);
    if (!n) return;
    _cvlt_out("        logs:\n");
    for (size_t i = 0; i < n; i++) {
        char line[512] = {0};
        mollusk_result_log_copy(r, i, (uint8_t *)line, sizeof line - 1);
        _cvlt_out("          %s\n", line);
    }
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_MOLLUSK_VM_H */
