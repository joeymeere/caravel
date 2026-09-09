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
        fprintf(stderr,
            "Usage: caravel init <project-name> [--sbf-ver=v0|v1|v2|v3]\n"
            "\n"
            "Creates a new Caravel project directory with:\n"
            "  Caravel.toml   Project configuration\n"
            "  Makefile        Build rules for SBF target\n"
            "  src/            Program source files\n"
            "  tests/          TypeScript test suite\n"
            "  build/          Compilation output\n"
            "\n"
            "  --sbf-ver=<v>   Default SBPF version baked into the Makefile\n"
            "                  (v0 default; overridable at build with make SBF_VER=)\n");
        return 1;
    }

    const char *name = NULL;
    const char *sbf_ver = "v0";
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--sbf-ver=", 10) == 0) {
            const char *v = argv[i] + 10;
            if (v[0] == 'v' || v[0] == 'V') v++;
            if (strcmp(v, "0") && strcmp(v, "1") &&
                strcmp(v, "2") && strcmp(v, "3")) {
                fprintf(stderr, "err: unknown sbf version '%s' "
                        "(expected v0, v1, v2, or v3)\n", argv[i] + 10);
                return 1;
            }
            static char verbuf[3];
            verbuf[0] = 'v'; verbuf[1] = v[0]; verbuf[2] = '\0';
            sbf_ver = verbuf;
        } else if (argv[i][0] != '-' && !name) {
            name = argv[i];
        }
    }
    if (!name) {
        fprintf(stderr, "err: missing project name\n");
        return 1;
    }

    if (strlen(name) >= CVL_MAX_NAME) {
        fprintf(stderr, "err: project name too long (max %d characters)\n",
                CVL_MAX_NAME - 1);
        return 1;
    }

    if (!valid_name(name)) {
        fprintf(stderr,
            "err: invalid project name '%s'\n"
            "  Names must start with a letter or underscore and contain\n"
            "  only alphanumerics, underscores, or hyphens.\n", name);
        return 1;
    }

    return cvl_scaffold_project(name, sbf_ver);
}
