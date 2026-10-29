#include "cli.h"
#include "scaffold.h"
#include <ctype.h>

static int valid_name(const char *name) {
    if (!name || !name[0]) return 0;
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return 0;
    for (const char *p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

int cmd_init(int argc, char **argv) {
    if (argc < 1) {
        fprintf(
            stderr,
            "Usage: caravel init <project-name> [--sbf-ver=v0|v1|v2|v3] "
            "[--tests=c|ts]\n"
            "                                    [--backend=litesvm|mollusk|seashell]\n"
            "\n"
            "Creates a new Caravel project directory with:\n"
            "  Caravel.toml   Project configuration\n"
            "  Makefile        Build rules for SBF target\n"
            "  src/            Program source files\n"
            "  tests/          Test suite (C by default)\n"
            "  build/          Compilation output\n"
            "\n"
            "  --sbf-ver=<v>   Default SBPF version baked into the Makefile\n"
            "                  (v0 default; overridable at build with make SBF_VER=)\n"
            "  --tests=<lang>  c (default): native C tests\n"
            "                  ts: TypeScript tests with litesvm + @solana/kit\n"
            "  --backend=<vm>  what C tests run on:\n"
            "                  litesvm (default): signed transactions, a bank\n"
            "                  mollusk: instructions against an account store\n"
            "                  seashell: instructions against recorded mainnet\n"
            "                            accounts (scenarios)\n"
        );
        return 1;
    }

    const char *name    = NULL;
    const char *sbf_ver = "v0";
    const char *tests   = "c";
    const char *backend = NULL;
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--tests=", 8) == 0) {
            tests = argv[i] + 8;
            if (strcmp(tests, "c") != 0 && strcmp(tests, "ts") != 0) {
                fprintf(
                    stderr,
                    "err: unknown test language '%s' (expected c or ts)\n",
                    tests
                );
                return 1;
            }
            continue;
        }
        if (strncmp(argv[i], "--backend=", 10) == 0) {
            backend = argv[i] + 10;
            if (strcmp(backend, "litesvm") != 0 &&
                strcmp(backend, "mollusk") != 0 &&
                strcmp(backend, "seashell") != 0) {
                fprintf(
                    stderr,
                    "err: unknown test backend '%s' (expected litesvm, "
                    "mollusk or seashell)\n",
                    backend
                );
                return 1;
            }
            continue;
        }
        if (strncmp(argv[i], "--sbf-ver=", 10) == 0) {
            const char *v = argv[i] + 10;
            if (v[0] == 'v' || v[0] == 'V') v++;
            if (strcmp(v, "0") && strcmp(v, "1") && strcmp(v, "2") &&
                strcmp(v, "3")) {
                fprintf(
                    stderr,
                    "err: unknown sbf version '%s' "
                    "(expected v0, v1, v2, or v3)\n",
                    argv[i] + 10
                );
                return 1;
            }
            static char verbuf[3];
            verbuf[0] = 'v';
            verbuf[1] = v[0];
            verbuf[2] = '\0';
            sbf_ver   = verbuf;
        } else if (argv[i][0] != '-' && !name) {
            name = argv[i];
        }
    }
    if (!name) {
        fprintf(stderr, "err: missing project name\n");
        return 1;
    }
    if (backend && strcmp(tests, "ts") == 0) {
        fprintf(stderr, "err: --backend applies to C tests, not --tests=ts\n");
        return 1;
    }

    if (strlen(name) >= CVL_MAX_NAME) {
        fprintf(
            stderr, "err: project name too long (max %d characters)\n",
            CVL_MAX_NAME - 1
        );
        return 1;
    }

    if (!valid_name(name)) {
        fprintf(
            stderr,
            "err: invalid project name '%s'\n"
            "  Names must start with a letter or underscore and contain\n"
            "  only alphanumerics, underscores, or hyphens.\n",
            name
        );
        return 1;
    }

    return cvl_scaffold_project(
        name, sbf_ver, tests, backend ? backend : "litesvm"
    );
}
