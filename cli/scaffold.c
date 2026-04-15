#include "scaffold.h"
#include "templates/entrypoint.h"
#include "templates/makefile.h"
#include "templates/test.h"
#include <ctype.h>
#include <stdarg.h>

static int write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "err: cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }
    fputs(content, fp);
    fclose(fp);
    return 0;
}

static int write_file_fmt(const char *path, const char *fmt, ...) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "err: cannot create %s: %s\n", path, strerror(errno));
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fclose(fp);
    return 0;
}

static void to_upper(const char *src, char *dst, size_t max) {
    size_t i;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

int cvl_scaffold_project(const char *name) {
    char path[CVL_MAX_PATH];
    char upper[CVL_MAX_NAME];

    to_upper(name, upper, sizeof(upper));

    printf("\n  Creating Caravel project: %s\n\n", name);

    snprintf(path, sizeof(path), "%s", name);
    if (cvl_file_exists(path)) {
        fprintf(stderr, "err: directory '%s' already exists\n", name);
        return -1;
    }

    cvl_mkdir_p(path);

    snprintf(path, sizeof(path), "%s/src", name);
    cvl_mkdir_p(path);

    snprintf(path, sizeof(path), "%s/tests", name);
    cvl_mkdir_p(path);

    snprintf(path, sizeof(path), "%s/build", name);
    cvl_mkdir_p(path);

    printf("  [+] Created directories\n");

    snprintf(path, sizeof(path), "%s/Caravel.toml", name);
    {
        FILE *fp = fopen(path, "w");
        if (!fp) { perror(path); return -1; }
        fprintf(fp,
            "[program]\n"
            "name    = \"%s\"\n"
            "version = \"0.1.0\"\n"
            "\n"
            "[build]\n"
            "src_dir   = \"src\"\n"
            "build_dir = \"build\"\n",
            name);
        fclose(fp);
    }
    printf("  [+] Caravel.toml\n");

    snprintf(path, sizeof(path), "%s/Makefile", name);
    write_file_fmt(path, TPL_MAKEFILE, name);
    printf("  [+] Makefile\n");

    snprintf(path, sizeof(path), "%s/src/entrypoint.c", name);
    write_file_fmt(path, TPL_ENTRYPOINT_C, name);
    printf("  [+] src/entrypoint.c\n");

    snprintf(path, sizeof(path), "%s/src/state.h", name);
    write_file_fmt(path, TPL_STATE_H, upper, upper, upper);
    printf("  [+] src/state.h\n");

    snprintf(path, sizeof(path), "%s/src/instructions.h", name);
    write_file_fmt(path, TPL_INSTRUCTIONS_H, upper, upper, upper);
    printf("  [+] src/instructions.h\n");

    snprintf(path, sizeof(path), "%s/tests/package.json", name);
    write_file_fmt(path, TPL_PACKAGE_JSON, name, name);
    printf("  [+] tests/package.json\n");

    snprintf(path, sizeof(path), "%s/tests/tsconfig.json", name);
    write_file(path, TPL_TSCONFIG_JSON);
    printf("  [+] tests/tsconfig.json\n");

    snprintf(path, sizeof(path), "%s/tests/%s.test.ts", name, name);
    write_file_fmt(path, TPL_TEST_TS, name, name);
    printf("  [+] tests/%s.test.ts\n", name);

    snprintf(path, sizeof(path), "%s/.gitignore", name);
    write_file(path,
        "build/\n"
        "tests/node_modules/\n"
        "tests/dist/\n"
        "*.bc\n"
        "*.o\n"
    );
    printf("  [+] .gitignore\n");

    printf("\n  Done! Next steps:\n");
    printf("    cd %s\n", name);
    printf("    caravel build\n");
    printf("    caravel test\n\n");

    return 0;
}
