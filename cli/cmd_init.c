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
            "Usage: caravel init <project-name>\n"
            "\n"
            "Creates a new Caravel project directory with:\n"
            "  Caravel.toml   Project configuration\n"
            "  Makefile        Build rules for SBF target\n"
            "  src/            Program source files\n"
            "  tests/          TypeScript test suite\n"
            "  build/          Compilation output\n");
        return 1;
    }

    const char *name = argv[0];

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

    return cvl_scaffold_project(name);
}
