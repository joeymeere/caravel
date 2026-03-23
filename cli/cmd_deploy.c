#include "cli.h"
#include "config.h"

int cmd_deploy(int argc, char **argv) {
    (void)argc; (void)argv;

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    char so_path[CVL_MAX_PATH];
    snprintf(so_path, sizeof(so_path), "%s/program.so", cfg.build_dir);

    if (!cvl_file_exists(so_path)) {
        fprintf(stderr,
            "err: %s not found — run 'caravel build' first\n", so_path);
        return 1;
    }

    printf("\n  Deploying %s v%s\n\n", cfg.name, cfg.version);

    char cmd[CVL_MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "solana program deploy %s", so_path);
    int rc = cvl_run_command(cmd);
    if (rc != 0) {
        fprintf(stderr, "\nerr: deploy failed (exit %d)\n", rc);
        return 1;
    }

    printf("\n  Deploy complete.\n\n");
    return 0;
}
