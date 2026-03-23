#include "cli.h"
#include "config.h"

static int collect_sources(const char *dir, char out[][CVL_MAX_NAME], int max) {
    DIR *dp = opendir(dir);
    if (!dp) {
        fprintf(stderr, "err: cannot open source directory '%s': %s\n",
                dir, strerror(errno));
        return -1;
    }

    int n = 0;
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL && n < max) {
        size_t len = strlen(ent->d_name);
        if (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0) {
            strncpy(out[n], ent->d_name, CVL_MAX_NAME - 1);
            out[n][CVL_MAX_NAME - 1] = '\0';
            n++;
        }
    }
    closedir(dp);
    return n;
}

static const char *find_tool(const char *env_var, const char *tool_name,
                              char *buf, size_t bufsz) {
    const char *val = getenv(env_var);
    if (val) return val;

    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, bufsz,
            "%s/.cache/solana/v1.51/platform-tools/llvm/bin/%s", home, tool_name);
        if (cvl_file_exists(buf)) return buf;
    }

    return tool_name; /* fall back to PATH */
}

int cmd_build(int argc, char **argv) {
    (void)argc; (void)argv;

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    printf("\n  Building %s v%s\n\n", cfg.name, cfg.version);

    cvl_mkdir_p(cfg.build_dir);

    char sources[128][CVL_MAX_NAME];
    int nsrc = collect_sources(cfg.src_dir, sources, 128);
    if (nsrc < 0) return 1;
    if (nsrc == 0) {
        fprintf(stderr, "err: no .c files found in '%s/'\n", cfg.src_dir);
        return 1;
    }

    printf("  Found %d source file(s)\n\n", nsrc);

    char clang_buf[CVL_MAX_PATH], lld_buf[CVL_MAX_PATH];
    const char *clang = find_tool("SOLANA_CLANG", "clang", clang_buf, sizeof(clang_buf));
    const char *lld = find_tool("SOLANA_LLD", "ld.lld", lld_buf, sizeof(lld_buf));

    const char *inc = getenv("CARAVEL_INCLUDE");
    if (!inc) inc = "../../include";

    /* compile each .c to .o */
    char cmd[CVL_MAX_PATH * 3];
    char obj_files[128][CVL_MAX_PATH];
    int nobj = 0;

    for (int i = 0; i < nsrc; i++) {
        char base[CVL_MAX_NAME];
        strncpy(base, sources[i], CVL_MAX_NAME);
        size_t len = strlen(base);
        base[len - 2] = '\0';

        snprintf(obj_files[nobj], CVL_MAX_PATH, "%s/%s.o", cfg.build_dir, base);

        snprintf(cmd, sizeof(cmd),
            "%s --target=sbf -fPIC -O2 -fno-builtin -I%s -c %s/%s -o %s",
            clang, inc, cfg.src_dir, sources[i], obj_files[nobj]);

        printf("  [CC] %s/%s\n", cfg.src_dir, sources[i]);
        int rc = cvl_run_command(cmd);
        if (rc != 0) {
            fprintf(stderr, "\nerr: compilation failed for %s (exit %d)\n",
                    sources[i], rc);
            return 1;
        }
        nobj++;
    }

    /* link .o files → program.so */
    printf("\n  [LD] %s/program.so\n", cfg.build_dir);

    char obj_list[CVL_MAX_PATH * 64] = "";
    for (int i = 0; i < nobj; i++) {
        strcat(obj_list, obj_files[i]);
        if (i < nobj - 1) strcat(obj_list, " ");
    }

    snprintf(cmd, sizeof(cmd),
        "%s -z notext -shared --Bdynamic %s/bpf.ld --entry entrypoint -o %s/program.so %s",
        lld, inc, cfg.build_dir, obj_list);
    int rc = cvl_run_command(cmd);
    if (rc != 0) {
        fprintf(stderr, "\nerr: linking failed (exit %d)\n", rc);
        return 1;
    }

    printf("\n  Build complete: %s/program.so\n", cfg.build_dir);

    /* TODO @joey IDLgen */
    printf("\n  Generating IDL...\n");
    cmd_idl(0, NULL);

    printf("\n");
    return 0;
}
