#include "cli.h"
#include "config.h"

static void print_dump_usage(void) {
    printf(
        "\n"
        "  Usage: caravel dump [options]\n"
        "\n"
        "  Options:\n"
        "    -S, --source        Interleave C source (requires --debug build)\n"
        "    --symbol=NAME       Disassemble only this function (comma-separated)\n"
        "    -o, --output=PATH   Write to file instead of stdout\n"
        "    --headers           Include section and symbol tables\n"
        "    --raw               Pass-through llvm-objdump -d with no extras\n"
        "    -h, --help          Show this message\n"
        "\n"
        "  Env:\n"
        "    CVL_OBJDUMP         Override path to llvm-objdump\n"
        "\n"
    );
}

int cmd_dump(int argc, char **argv) {
    int with_source = 0;
    int with_headers = 0;
    int raw = 0;
    const char *symbol = NULL;
    const char *out_path = NULL;

    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_dump_usage();
            return 0;
        } else if (strcmp(a, "-S") == 0 || strcmp(a, "--source") == 0) {
            with_source = 1;
        } else if (strcmp(a, "--headers") == 0) {
            with_headers = 1;
        } else if (strcmp(a, "--raw") == 0) {
            raw = 1;
        } else if (strncmp(a, "--symbol=", 9) == 0) {
            symbol = a + 9;
        } else if (strncmp(a, "--output=", 9) == 0) {
            out_path = a + 9;
        } else if (strcmp(a, "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else {
            fprintf(stderr, "err: unknown dump option '%s'\n", a);
            print_dump_usage();
            return 1;
        }
    }

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    char so_path[CVL_MAX_PATH];
    snprintf(so_path, sizeof(so_path), "%s/program.so", cfg.build_dir);
    if (!cvl_file_exists(so_path)) {
        fprintf(stderr, "err: %s not found; run 'caravel build' first\n", so_path);
        return 1;
    }

    char objdump_buf[CVL_MAX_PATH];
    const char *objdump = cvl_find_tool("CVL_OBJDUMP", "llvm-objdump",
                                         objdump_buf, sizeof(objdump_buf));

    char cmd[CVL_MAX_PATH * 4];
    int off = 0;
    off += snprintf(cmd + off, sizeof(cmd) - off, "%s -d", objdump);

    if (!raw) {
        off += snprintf(cmd + off, sizeof(cmd) - off,
                        " --no-show-raw-insn --print-imm-hex");
        if (with_source)
            off += snprintf(cmd + off, sizeof(cmd) - off, " --source");
        if (with_headers)
            off += snprintf(cmd + off, sizeof(cmd) - off,
                            " --section-headers --syms");
        if (symbol)
            off += snprintf(cmd + off, sizeof(cmd) - off,
                            " --disassemble-symbols=%s", symbol);
    }

    off += snprintf(cmd + off, sizeof(cmd) - off, " %s", so_path);
    if (out_path)
        snprintf(cmd + off, sizeof(cmd) - off, " > %s", out_path);

    if (out_path)
        printf("  Dumping %s -> %s\n", so_path, out_path);

    int rc = cvl_run_command(cmd);
    if (rc != 0) {
        fprintf(stderr, "\nerr: dump failed (exit %d)\n", rc);
        return 1;
    }
    return 0;
}
