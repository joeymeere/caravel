#include "cli.h"
#include "config.h"

typedef enum {
    BACKEND_SOLANA,
    BACKEND_SBPF_LINKER,
} BuildBackend;

static int collect_sources(const char *base, const char *rel,
                           char out[][CVL_MAX_PATH], int max, int n) {
    char dir[CVL_MAX_PATH];
    if (rel[0])
        snprintf(dir, sizeof(dir), "%s/%s", base, rel);
    else
        snprintf(dir, sizeof(dir), "%s", base);

    DIR *dp = opendir(dir);
    if (!dp) {
        fprintf(stderr, "err: cannot open source directory '%s': %s\n",
                dir, strerror(errno));
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL && n < max) {
        if (ent->d_name[0] == '.') continue;

        char full[CVL_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            char subrel[CVL_MAX_PATH];
            if (rel[0])
                snprintf(subrel, sizeof(subrel), "%s/%s", rel, ent->d_name);
            else
                snprintf(subrel, sizeof(subrel), "%s", ent->d_name);
            n = collect_sources(base, subrel, out, max, n);
            if (n < 0) { closedir(dp); return -1; }
        } else if (S_ISREG(st.st_mode)) {
            size_t len = strlen(ent->d_name);
            if (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0) {
                if (rel[0])
                    snprintf(out[n], CVL_MAX_PATH, "%s/%s", rel, ent->d_name);
                else
                    snprintf(out[n], CVL_MAX_PATH, "%s", ent->d_name);
                n++;
            }
        }
    }
    closedir(dp);
    return n;
}

static int version_cmp(const char *a, const char *b) {
    if (*a == 'v') a++;
    if (*b == 'v') b++;

    while (*a && *b) {
        long va = strtol(a, (char **)&a, 10);
        long vb = strtol(b, (char **)&b, 10);
        if (va != vb) return (va > vb) ? 1 : -1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    if (*a) return 1;
    if (*b) return -1;
    return 0;
}

static const char *find_tool(const char *env_var, const char *tool_name,
                              char *buf, size_t bufsz) {
    const char *val = getenv(env_var);
    if (val) return val;

    const char *home = getenv("HOME");
    if (!home) return tool_name;

    char cache_dir[CVL_MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache/solana", home);

    DIR *dp = opendir(cache_dir);
    if (!dp) return tool_name;

    char best_ver[64] = "";
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (ent->d_name[0] != 'v') continue;

        char candidate[CVL_MAX_PATH];
        snprintf(candidate, sizeof(candidate),
            "%s/%s/platform-tools/llvm/bin/%s", cache_dir, ent->d_name, tool_name);

        if (cvl_file_exists(candidate) && version_cmp(ent->d_name, best_ver) > 0) {
            strncpy(best_ver, ent->d_name, sizeof(best_ver) - 1);
        }
    }
    closedir(dp);

    if (best_ver[0]) {
        snprintf(buf, bufsz,
            "%s/%s/platform-tools/llvm/bin/%s", cache_dir, best_ver, tool_name);
        return buf;
    }

    return tool_name; /* fall back to PATH */
}

/* Find llc from the sbpf-linker gallery LLVM build. */
static const char *find_gallery_llc(char *buf, size_t bufsz) {
    const char *val = getenv("CVL_LLC");
    if (val) return val;

    const char *home = getenv("HOME");
    if (!home) return "llc";

#ifdef __APPLE__
    const char *cache_base = "Library/Caches";
#else
    const char *cache_base = ".cache";
#endif

    char cache_dir[CVL_MAX_PATH];
    snprintf(cache_dir, sizeof(cache_dir), "%s/%s", home, cache_base);

    DIR *dp = opendir(cache_dir);
    if (!dp) return "llc";

    char best[CVL_MAX_PATH] = "";
    struct dirent *ent;
    while ((ent = readdir(dp)) != NULL) {
        if (strncmp(ent->d_name, "sbpf-linker-upstream-gallery-", 29) != 0)
            continue;
        char candidate[CVL_MAX_PATH];
        snprintf(candidate, sizeof(candidate),
            "%s/%s/llvm-install/bin/llc", cache_dir, ent->d_name);
        if (cvl_file_exists(candidate))
            strncpy(best, candidate, sizeof(best) - 1);
    }
    closedir(dp);

    if (best[0]) {
        snprintf(buf, bufsz, "%s", best);
        return buf;
    }

    return "llc";
}

int cmd_build(int argc, char **argv) {
    /* parse build flags */
    const char *opt_level = "-Oz";
    const char *debug_flags = "";
    const char *mode_label = "release";
    BuildBackend backend = BACKEND_SOLANA;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            opt_level = "-O1";
            debug_flags = " -g";
            mode_label = "debug";
        } else if (strcmp(argv[i], "--fast") == 0) {
            opt_level = "-O1";
            debug_flags = "";
            mode_label = "fast";
        } else if (strncmp(argv[i], "--backend=", 10) == 0) {
            const char *name = argv[i] + 10;
            if (strcmp(name, "solana") == 0)
                backend = BACKEND_SOLANA;
            else if (strcmp(name, "sbpf-linker") == 0)
                backend = BACKEND_SBPF_LINKER;
            else {
                fprintf(stderr, "err: unknown backend '%s'\n", name);
                return 1;
            }
        }
    }

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    const char *backend_label = (backend == BACKEND_SBPF_LINKER)
        ? "sbpf-linker" : "solana";
    printf("\n  Building %s v%s (%s, %s)\n\n",
        cfg.name, cfg.version, mode_label, backend_label);

    cvl_mkdir_p(cfg.build_dir);

    char sources[128][CVL_MAX_PATH];
    int nsrc = collect_sources(cfg.src_dir, "", sources, 128, 0);
    if (nsrc < 0) return 1;
    if (nsrc == 0) {
        fprintf(stderr, "err: no .c files found in '%s/'\n", cfg.src_dir);
        return 1;
    }

    printf("  Found %d source file(s)\n\n", nsrc);

    char clang_buf[CVL_MAX_PATH], lld_buf[CVL_MAX_PATH], llc_buf[CVL_MAX_PATH];
    const char *clang;
    const char *lld;
    const char *llc = NULL;

    if (backend == BACKEND_SBPF_LINKER) {
        clang = getenv("CVL_CLANG");
        if (!clang) clang = "clang";
        llc = find_gallery_llc(llc_buf, sizeof(llc_buf));
    } else {
        clang = find_tool("SOLANA_CLANG", "clang", clang_buf, sizeof(clang_buf));
    }
    lld = find_tool("SOLANA_LLD", "ld.lld", lld_buf, sizeof(lld_buf));

    const char *inc = getenv("CARAVEL_INCLUDE");
    if (!inc) inc = "../../include";

    /* compile each .c source */
    char cmd[CVL_MAX_PATH * 3];
    char obj_files[128][CVL_MAX_PATH];
    int nobj = 0;

    for (int i = 0; i < nsrc; i++) {
        char base[CVL_MAX_PATH];
        strncpy(base, sources[i], CVL_MAX_PATH);
        size_t len = strlen(base);
        base[len - 2] = '\0';

        snprintf(obj_files[nobj], CVL_MAX_PATH, "%s/%s.o", cfg.build_dir, base);

        char obj_dir[CVL_MAX_PATH];
        strncpy(obj_dir, obj_files[nobj], CVL_MAX_PATH);
        char *last_slash = strrchr(obj_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            cvl_mkdir_p(obj_dir);
        }

        if (backend == BACKEND_SBPF_LINKER) {
            /* .c → .ll  (LLVM IR via upstream clang) */
            char ll_path[CVL_MAX_PATH];
            snprintf(ll_path, sizeof(ll_path), "%s/%s.ll", cfg.build_dir, base);

            snprintf(cmd, sizeof(cmd),
                "%s -target bpfel %s -fno-builtin -fdata-sections"
                "%s -emit-llvm -S -I%s -o %s %s/%s",
                clang, opt_level, debug_flags, inc,
                ll_path, cfg.src_dir, sources[i]);

            printf("  [CC] %s/%s\n", cfg.src_dir, sources[i]);
            int rc = cvl_run_command(cmd);
            if (rc != 0) {
                fprintf(stderr, "\nerr: compilation failed for %s (exit %d)\n",
                        sources[i], rc);
                return 1;
            }

            /* .ll → .o  (gallery llc — SBF-compatible codegen, no JMP32) */
            snprintf(cmd, sizeof(cmd),
                "%s -march=bpfel -mcpu=generic -filetype=obj "
                "-bpf-stack-size=4096 -o %s %s",
                llc, obj_files[nobj], ll_path);

            printf("  [LLC] %s\n", obj_files[nobj]);
            rc = cvl_run_command(cmd);
            if (rc != 0) {
                fprintf(stderr, "\nerr: llc failed for %s (exit %d)\n",
                        ll_path, rc);
                return 1;
            }

            /* fix merge-string section so ld.lld preserves all strings */
            snprintf(cmd, sizeof(cmd),
                "llvm-objcopy --set-section-flags "
                ".rodata.str1.1=alloc,readonly %s",
                obj_files[nobj]);
            cvl_run_command(cmd);
        } else {
            /* .c → .o  (Solana clang — SBF target) */
            snprintf(cmd, sizeof(cmd),
                "%s --target=sbf -fPIC %s -fno-builtin -fdata-sections"
                "%s -I%s -c %s/%s -o %s",
                clang, opt_level, debug_flags, inc,
                cfg.src_dir, sources[i], obj_files[nobj]);

            printf("  [CC] %s/%s\n", cfg.src_dir, sources[i]);
            int rc = cvl_run_command(cmd);
            if (rc != 0) {
                fprintf(stderr, "\nerr: compilation failed for %s (exit %d)\n",
                        sources[i], rc);
                return 1;
            }
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
        "%s -z notext -shared --Bdynamic --gc-sections "
        "%s/bpf.ld --entry entrypoint -o %s/program.so %s",
        lld, inc, cfg.build_dir, obj_list);
    int rc = cvl_run_command(cmd);
    if (rc != 0) {
        fprintf(stderr, "\nerr: linking failed (exit %d)\n", rc);
        return 1;
    }

    /* gallery llc emits EM_BPF (0xF7); patch to SBF (0x0107) */
    if (backend == BACKEND_SBPF_LINKER) {
        char so_path[CVL_MAX_PATH];
        snprintf(so_path, sizeof(so_path), "%s/program.so", cfg.build_dir);
        FILE *fp = fopen(so_path, "r+b");
        if (fp) {
            fseek(fp, 18, SEEK_SET);
            unsigned char sbf[2] = {0x07, 0x01};
            fwrite(sbf, 1, 2, fp);
            fclose(fp);
        }
    }

    printf("\n  Build complete: %s/program.so\n", cfg.build_dir);

    /* TODO @joey IDLgen */
    printf("\n  Generating IDL...\n");
    cmd_idl(0, NULL);

    printf("\n");
    return 0;
}
