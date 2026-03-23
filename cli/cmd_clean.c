#include "cli.h"
#include "config.h"

int cmd_clean(int argc, char **argv) {
    (void)argc; (void)argv;

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    if (!cvl_file_exists(cfg.build_dir)) {
        printf("  Nothing to clean.\n");
        return 0;
    }

    printf("  Cleaning %s/\n", cfg.build_dir);

    char cmd[CVL_MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", cfg.build_dir);
    int rc = cvl_run_command(cmd);
    if (rc != 0) {
        fprintf(stderr, "err: clean failed (exit %d)\n", rc);
        return 1;
    }

    printf("  Done.\n");
    return 0;
}
