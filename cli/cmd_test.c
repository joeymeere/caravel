#include "cli.h"
#include "config.h"

/* Check if any .test.c files exist in tests/ */
static int has_c_tests(void) {
    DIR *dp = opendir("tests");
    if (!dp) return 0;
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 7 && strcmp(ent->d_name + len - 7, ".test.c") == 0) {
            closedir(dp);
            return 1;
        }
    }
    closedir(dp);
    return 0;
}

/* Find directory containing libquasar_svm.{dylib,so} */
static const char *find_quasar_lib(char *buf, size_t bufsz) {
    const char *env = getenv("QUASAR_LIB");
    if (env) return env;

    static const char *search[] = { "./lib", "/usr/local/lib" };
    for (int i = 0; i < 2; i++) {
        char path[CVL_MAX_PATH];
        snprintf(path, sizeof(path), "%s/libquasar_svm.dylib", search[i]);
        if (cvl_file_exists(path)) return search[i];
        snprintf(path, sizeof(path), "%s/libquasar_svm.so", search[i]);
        if (cvl_file_exists(path)) return search[i];
    }

    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, bufsz, "%s/.local/lib", home);
        char path[CVL_MAX_PATH];
        snprintf(path, sizeof(path), "%s/libquasar_svm.dylib", buf);
        if (cvl_file_exists(path)) return buf;
        snprintf(path, sizeof(path), "%s/libquasar_svm.so", buf);
        if (cvl_file_exists(path)) return buf;
    }

    return NULL;
}

static int run_c_tests(int argc, char **argv, CvlConfig *cfg) {
    printf("  [1/3] Building program...\n\n");
    int rc = cmd_build(argc, argv);
    if (rc != 0) {
        fprintf(stderr, "\nerr: build failed — aborting tests\n");
        return rc;
    }

    char lib_buf[CVL_MAX_PATH];
    const char *quasar_lib = find_quasar_lib(lib_buf, sizeof(lib_buf));
    if (!quasar_lib) {
        fprintf(stderr,
            "\nerr: libquasar_svm not found\n"
            "     Set QUASAR_LIB to the directory containing libquasar_svm.{dylib,so}\n"
            "     Build from source: https://github.com/blueshift-gg/quasar-svm\n");
        return 1;
    }

    const char *inc = getenv("CARAVEL_INCLUDE");
    if (!inc) inc = "../../include";

    /* collect test sources */
    char sources[32][CVL_MAX_PATH];
    char bins[32][CVL_MAX_PATH];
    int nsrc = 0;

    DIR *dp = opendir("tests");
    if (dp) {
        struct dirent *ent;
        while ((ent = readdir(dp)) != NULL && nsrc < 32) {
            size_t len = strlen(ent->d_name);
            if (len > 7 && strcmp(ent->d_name + len - 7, ".test.c") == 0) {
                snprintf(sources[nsrc], CVL_MAX_PATH, "tests/%s", ent->d_name);
                /* binary: build/<name>.test (strip .c extension) */
                char base[CVL_MAX_PATH];
                strncpy(base, ent->d_name, CVL_MAX_PATH - 1);
                base[CVL_MAX_PATH - 1] = '\0';
                base[len - 2] = '\0';
                snprintf(bins[nsrc], CVL_MAX_PATH, "%s/%s", cfg->build_dir, base);
                nsrc++;
            }
        }
        closedir(dp);
    }

    printf("\n  [2/3] Compiling tests...\n\n");
    char cmd[CVL_MAX_PATH * 4];
    for (int i = 0; i < nsrc; i++) {
        printf("  [CC] %s\n", sources[i]);
        snprintf(cmd, sizeof(cmd),
            "cc -I%s %s -L%s -lquasar_svm -Wl,-rpath,%s -o %s",
            inc, sources[i], quasar_lib, quasar_lib, bins[i]);
        rc = cvl_run_command(cmd);
        if (rc != 0) {
            fprintf(stderr, "\nerr: test compilation failed (exit %d)\n", rc);
            return 1;
        }
    }

    printf("\n  [3/3] Running tests...\n\n");
    int any_failed = 0;
    for (int i = 0; i < nsrc; i++) {
        snprintf(cmd, sizeof(cmd), "./%s", bins[i]);
        rc = cvl_run_command(cmd);
        if (rc != 0) any_failed = 1;
    }

    if (any_failed) {
        fprintf(stderr, "\n  Some tests failed.\n\n");
        return 1;
    }

    printf("\n  All tests passed.\n\n");
    return 0;
}

int cmd_test(int argc, char **argv) {
    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    printf("\n  Testing %s v%s\n\n", cfg.name, cfg.version);

    if (has_c_tests()) {
        return run_c_tests(argc, argv, &cfg);
    }

    if (cvl_file_exists("tests/package.json")) {
        printf("  [1/3] Building program...\n\n");
        int rc = cmd_build(argc, argv);
        if (rc != 0) {
            fprintf(stderr, "\nerr: build failed — aborting tests\n");
            return rc;
        }

        printf("  [2/3] Installing test dependencies...\n\n");
        rc = cvl_run_command("cd tests && npm install");
        if (rc != 0) {
            fprintf(stderr, "\nerr: npm install failed (exit %d)\n", rc);
            return 1;
        }

        printf("\n  [3/3] Running tests...\n\n");
        rc = cvl_run_command("cd tests && npm test");
        if (rc != 0) {
            fprintf(stderr, "\n  Tests failed (exit %d)\n", rc);
            return 1;
        }

        printf("\n  All tests passed.\n\n");
        return 0;
    }

    fprintf(stderr, "err: no tests found\n"
            "     Create tests/*.test.c for C tests or tests/package.json for TypeScript tests\n");
    return 1;
}
