#include "cli.h"
#include "config.h"

int cmd_test(int argc, char **argv) {
    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    printf("\n  Testing %s v%s\n\n", cfg.name, cfg.version);

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
