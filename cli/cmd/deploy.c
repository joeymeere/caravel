#include "cli.h"
#include "config.h"

int cmd_deploy(int argc, char **argv) {
    (void)argc;
    (void)argv;

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    char so_path[CVL_MAX_PATH];
    if (cvl_fmt(so_path, sizeof(so_path), "%s/program.so", cfg.build_dir))
        return 1;

    if (!cvl_file_exists(so_path)) {
        fprintf(
            stderr, "err: %s not found — run 'caravel build' first\n", so_path
        );
        return 1;
    }

    printf("\n  Deploying %s v%s\n\n", cfg.name, cfg.version);

    CvlCmd cmd;
    cvl_cmd_init(&cmd, "solana");
    cvl_cmd_arg(&cmd, "program");
    cvl_cmd_arg(&cmd, "deploy");
    cvl_cmd_arg(&cmd, so_path);
    int rc = cvl_cmd_run(&cmd, NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "\nerr: deploy failed (exit %d)\n", rc);
        return 1;
    }

    printf("\n  Deploy complete.\n\n");
    return 0;
}
