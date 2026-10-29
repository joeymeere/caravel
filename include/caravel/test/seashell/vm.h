#ifndef CARAVEL_TEST_SEASHELL_VM_H
#define CARAVEL_TEST_SEASHELL_VM_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/**
 * Main runner
 *
 * Seashell is slightly different from the other frameworks, in the sense that it is scenario-driven.
 * Instructions run against local accounts. With a scenario loaded, real mainnet accounts are recorded from RPC.
 * Results are memoized and accounts nobody have start out empty, so tests can create them
 */
typedef SeashellHandle Shell;

#define _CVLT_VM      Shell
#define _CVLT_VM_NAME shell

/* where SCENARIO("name") keeps its recording, relative to the project root */
#ifndef CARAVEL_TEST_SCENARIO_DIR
#define CARAVEL_TEST_SCENARIO_DIR "tests/scenarios"
#endif

_CVLT_API const char *_cvlt_seashell_error(void) {
    static char err[256];
    memset(err, 0, sizeof err);
    seashell_last_error_copy((uint8_t *)err, sizeof err - 1);
    return err;
}

_CVLT_API Shell *_cvlt_vm_new(const char *program, const Pubkey *program_id) {
    Shell *shell = seashell_new();
    if (!shell) {
        fprintf(stderr, "error: could not create Seashell\n");
        return NULL;
    }
    if (seashell_add_program_from_file(
            shell, program_id->bytes, (const uint8_t *)program, strlen(program)
        ) != 0) {
        fprintf(
            stderr, "error: cannot load %s: %s\n", program,
            _cvlt_seashell_error()
        );
        seashell_free(shell);
        return NULL;
    }
    seashell_set_memoize(shell, true);
    seashell_set_allow_uninitialized(shell, true, false);
    return shell;
}

/* this will also write back a scenario that recorded anything */
_CVLT_API void _cvlt_vm_free(Shell *shell) {
    seashell_free(shell);
}

static void _cvlt_pubkey_from_seed(const uint8_t seed[32], Pubkey *out) {
    seashell_pubkey_from_seed(seed, out->bytes);
}

static bool _cvlt_find_pda_raw(
    const uint8_t *const *seeds,
    const size_t *lens,
    size_t n,
    const Pubkey *program_id,
    Pubkey *out,
    uint8_t *bump_out
) {
    return seashell_find_program_address(
               seeds, lens, n, program_id->bytes, out->bytes, bump_out
           ) == 0;
}

/* `logs` is the SeashellResult an IxResult came from */
static void _cvlt_print_logs(const void *logs) {
    const SeashellResult *r = logs;
    size_t n                = seashell_result_logs_count(r);
    if (!n) return;
    _cvlt_out("        logs:\n");
    for (size_t i = 0; i < n; i++) {
        char line[512] = {0};
        seashell_result_log_copy(r, i, (uint8_t *)line, sizeof line - 1);
        _cvlt_out("          %s\n", line);
    }
}

_CVLT_API void
_cvlt_scenario(Shell *shell, const char *name, const char *file, int line) {
    char path[512];
    int n = snprintf(
        path, sizeof path, "%s/%s.json.gz", CARAVEL_TEST_SCENARIO_DIR, name
    );
    if (n < 0 || (size_t)n >= sizeof path)
        _cvlt_fail(file, line, "SCENARIO: name too long");
    if (seashell_load_scenario(
            shell, (const uint8_t *)path, strlen(path), NULL, 0
        ) != 0)
        _cvlt_fail(
            file, line, "SCENARIO(%s): %s", name, _cvlt_seashell_error()
        );
}

/**
 * Run against mainnet state recorded in `tests/scenarios/<name>.json.gz`
 *
 * With `RPC_URL` set, accounts nobody has are fetched from it and recorded
 * there when the tests finish; without it the file is replayed offline, so
 * commit it. Recorded accounts take precedence over ones set locally.
 * Call in `SETUP`, before touching any accounts
 */
#define SCENARIO(name) _cvlt_scenario(shell, (name), __FILE__, __LINE__)

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_SEASHELL_VM_H */
