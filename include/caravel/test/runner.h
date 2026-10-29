#ifndef CARAVEL_TEST_RUNNER_H
#define CARAVEL_TEST_RUNNER_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

#define CVL_TEST_MAX_TESTS 256

/** the backend provides the handle tests receive and its lifecycle */

typedef struct {
    const char *name;
    void (*fn)(_CVLT_VM *);
    int line;
} _CvlTestCase;

static struct {
    _CvlTestCase tests[CVL_TEST_MAX_TESTS];
    int count;
    void (*setup)(_CVLT_VM *);
} _cvlt_registry;

_CVLT_API void
_cvlt_register(const char *name, void (*fn)(_CVLT_VM *), int line) {
    if (_cvlt_registry.count == CVL_TEST_MAX_TESTS) {
        fprintf(stderr, "too many tests (max %d)\n", CVL_TEST_MAX_TESTS);
        exit(1);
    }
    _cvlt_registry.tests[_cvlt_registry.count++] =
        (_CvlTestCase){name, fn, line};
}

/** a test - the backend's handle (`svm` on LiteSVM, `ctx` on Mollusk, etc) is in scope. 
    The name is shown with '_' as spaces */
#define TEST(name) \
    static void _cvlt_test_##name(_CVLT_VM *_CVLT_VM_NAME); \
    __attribute__((constructor)) static void _cvlt_reg_##name(void) { \
        _cvlt_register(#name, _cvlt_test_##name, __LINE__); \
    } \
    static void _cvlt_test_##name( \
        __attribute__((unused)) _CVLT_VM *_CVLT_VM_NAME \
    )

/** runs once before the first test - the backend's handle is in scope */
#define SETUP \
    static void _cvlt_setup_fn(_CVLT_VM *_CVLT_VM_NAME); \
    __attribute__((constructor)) static void _cvlt_reg_setup(void) { \
        _cvlt_registry.setup = _cvlt_setup_fn; \
    } \
    static void _cvlt_setup_fn(__attribute__((unused)) _CVLT_VM *_CVLT_VM_NAME)

_CVLT_API int _cvlt_by_line(const void *a, const void *b) {
    return ((const _CvlTestCase *)a)->line - ((const _CvlTestCase *)b)->line;
}

_CVLT_API int _cvlt_run(void (*fn)(_CVLT_VM *), _CVLT_VM *vm) {
    jmp_buf jump;
    _cvlt.jump           = &jump;
    _cvlt.failed_in_test = 0;
    _cvlt.detail_len     = 0;
    _cvlt.detail[0]      = '\0';
    if (setjmp(jump) == 0) fn(vm);
    _cvlt.jump = NULL;
    _cvlt_free_owned();
    return _cvlt.failed_in_test;
}

#ifndef CARAVEL_TEST_NO_MAIN
int main(int argc, char **argv) {
    const char *filter  = argc > 1 ? argv[1] : NULL;
    const char *program = getenv("CARAVEL_TEST_PROGRAM");
    if (!program) program = CARAVEL_TEST_PROGRAM;
    setvbuf(stdout, NULL, _IOLBF, 0);

#ifdef CARAVEL_TEST_PROGRAM_ID
    PROGRAM_ID = PUBKEY(CARAVEL_TEST_PROGRAM_ID);
#else
    PROGRAM_ID = keypair(0xff).pubkey;
#endif

    _CVLT_VM *vm = _cvlt_vm_new(program, &PROGRAM_ID);
    if (!vm) return 1;

    qsort(
        _cvlt_registry.tests, (size_t)_cvlt_registry.count,
        sizeof *_cvlt_registry.tests, _cvlt_by_line
    );

    if (_cvlt_registry.setup && _cvlt_run(_cvlt_registry.setup, vm)) {
        printf("    \xE2\x9C\x97 SETUP\n%s\n    no tests run\n", _cvlt.detail);
        return 1;
    }

    int passed = 0, failed = 0;
    for (int i = 0; i < _cvlt_registry.count; i++) {
        const _CvlTestCase *t = &_cvlt_registry.tests[i];
        if (filter && !strstr(t->name, filter)) continue;
        int bad = _cvlt_run(t->fn, vm);
        char label[128];
        snprintf(label, sizeof label, "%s", t->name);
        for (char *p = label; *p; p++)
            if (*p == '_') *p = ' ';
        printf("    %s %s\n", bad ? "\xE2\x9C\x97" : "\xE2\x9C\x93", label);
        if (bad) fputs(_cvlt.detail, stdout);
        bad ? failed++ : passed++;
    }
    _cvlt_vm_free(vm);

    printf("\n    %d passing", passed);
    if (failed) printf(", %d failing", failed);
    printf("\n");
    return failed ? 1 : 0;
}
#endif /* CARAVEL_TEST_NO_MAIN */

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_RUNNER_H */
