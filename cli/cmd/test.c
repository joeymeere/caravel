#include "cli.h"
#include "config.h"
#include <sys/utsname.h>
#include <sys/wait.h>

#define MAX_TESTS 64

typedef struct {
    const char *name;   /* [test] backend value; also <name>-cxx, <name>.h */
    const char *define; /* selects it in caravel/test.h */
    const char *env;    /* overrides the download with a local checkout */
    const char *version;
    const char *repo; /* NULL: not available yet */
} Backend;

static const Backend BACKENDS[] = {
    {"litesvm", "CARAVEL_TEST_LITESVM", "CARAVEL_LITESVM_DIR",
     CVL_LITESVM_CXX_VERSION, CVL_LITESVM_CXX_REPO},
    {"mollusk", "CARAVEL_TEST_MOLLUSK", "CARAVEL_MOLLUSK_DIR",
     CVL_MOLLUSK_CXX_VERSION, CVL_MOLLUSK_CXX_REPO},
    {"seashell", "CARAVEL_TEST_SEASHELL", "CARAVEL_SEASHELL_DIR",
     CVL_SEASHELL_CXX_VERSION, CVL_SEASHELL_CXX_REPO},
};
#define NBACKENDS (int)(sizeof BACKENDS / sizeof *BACKENDS)

static int run_ts(void) {
    printf("  [2/3] Installing test dependencies...\n\n");
    CvlCmd cmd;
    cvl_cmd_init(&cmd, "npm");
    cvl_cmd_arg(&cmd, "install");
    int rc = cvl_cmd_run(&cmd, "tests", NULL);
    if (rc != 0) {
        fprintf(stderr, "\nerr: npm install failed (exit %d)\n", rc);
        return 1;
    }

    printf("\n  [3/3] Running tests...\n\n");
    cvl_cmd_init(&cmd, "npm");
    cvl_cmd_arg(&cmd, "test");
    rc = cvl_cmd_run(&cmd, "tests", NULL);
    if (rc != 0) {
        fprintf(stderr, "\n  Tests failed (exit %d)\n", rc);
        return 1;
    }
    return 0;
}

static const char *host_platform(void) {
    struct utsname u;
    if (uname(&u) != 0) return NULL;
    int arm =
        strcmp(u.machine, "arm64") == 0 || strcmp(u.machine, "aarch64") == 0;
    int x64 =
        strcmp(u.machine, "x86_64") == 0 || strcmp(u.machine, "amd64") == 0;
    if (strcmp(u.sysname, "Darwin") == 0)
        return arm ? "darwin-arm64" : x64 ? "darwin-x64" : NULL;
    if (strcmp(u.sysname, "Linux") == 0)
        return arm ? "linux-arm64" : x64 ? "linux-x64" : NULL;
    return NULL;
}

static int lib_from_dir(
    const Backend *b,
    const char *dir,
    const char *platform,
    char *inc,
    char *lib
) {
    char header[CVL_MAX_PATH];
    if (cvl_fmt(header, CVL_MAX_PATH, "%s/include/%s.h", dir, b->name))
        return -1;
    if (!cvl_file_exists(header)) return -1;
    cvl_fmt(inc, CVL_MAX_PATH, "%s/include", dir);
    cvl_fmt(lib, CVL_MAX_PATH, "%s/lib/lib%s.a", dir, b->name);
    if (cvl_file_exists(lib)) return 0;
    cvl_fmt(lib, CVL_MAX_PATH, "%s/libs/%s/lib%s.a", dir, platform, b->name);
    return cvl_file_exists(lib) ? 0 : -1;
}

static int filter_sums(const char *in, const char *out, const char *name) {
    FILE *src = fopen(in, "r");
    if (!src) return -1;
    FILE *dst = fopen(out, "w");
    if (!dst) {
        fclose(src);
        return -1;
    }
    char line[512];
    int found   = 0;
    size_t nlen = strlen(name);
    while (fgets(line, sizeof line, src)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len > nlen && strcmp(line + len - nlen, name) == 0 &&
            line[len - nlen - 1] == ' ') {
            fprintf(dst, "%s\n", line);
            found = 1;
        }
    }
    fclose(src);
    fclose(dst);
    return found ? 0 : -1;
}

static int verify_sha256(const char *dir, const char *sums) {
    CvlCmd cmd;
    cvl_cmd_init(&cmd, "sha256sum");
    cvl_cmd_arg(&cmd, "-c");
    cvl_cmd_arg(&cmd, sums);
    if (cvl_cmd_run(&cmd, dir, NULL) == 0) return 0;
    cvl_cmd_init(&cmd, "shasum");
    cvl_cmd_arg(&cmd, "-a");
    cvl_cmd_arg(&cmd, "256");
    cvl_cmd_arg(&cmd, "-c");
    cvl_cmd_arg(&cmd, sums);
    return cvl_cmd_run(&cmd, dir, NULL);
}

static int download(const char *url, const char *dest) {
    CvlCmd cmd;
    cvl_cmd_init(&cmd, "curl");
    cvl_cmd_arg(&cmd, "-fsSL");
    cvl_cmd_arg(&cmd, "-o");
    cvl_cmd_arg(&cmd, dest);
    cvl_cmd_arg(&cmd, url);
    return cvl_cmd_run(&cmd, NULL, NULL);
}

static int
find_lib(const Backend *b, const char *platform, char *inc, char *lib) {
    const char *env = getenv(b->env);
    if (env) {
        if (lib_from_dir(b, env, platform, inc, lib) == 0) return 0;
        fprintf(
            stderr,
            "err: %s=%s has no include/%s.h with lib/lib%s.a or "
            "libs/%s/lib%s.a\n",
            b->env, env, b->name, b->name, platform, b->name
        );
        return -1;
    }

    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "err: HOME is not set\n");
        return -1;
    }
    char cache[CVL_MAX_PATH], name[CVL_MAX_NAME], dir[CVL_MAX_PATH];
    cvl_fmt(cache, sizeof cache, "%s/.cache/caravel/%s-cxx", home, b->name);
    cvl_fmt(name, sizeof name, "%s-cxx-v%s-%s", b->name, b->version, platform);
    if (cvl_fmt(dir, sizeof dir, "%s/%s", cache, name)) return -1;
    if (lib_from_dir(b, dir, platform, inc, lib) == 0) return 0;

    printf("  Fetching %s-cxx v%s (%s)...\n\n", b->name, b->version, platform);
    cvl_mkdir_p(cache);
    char tarball[CVL_MAX_NAME + 8], url[CVL_MAX_PATH], path[CVL_MAX_PATH];
    cvl_fmt(tarball, sizeof tarball, "%s.tar.gz", name);
    cvl_fmt(
        url, sizeof url, "%s/releases/download/v%s/%s", b->repo, b->version,
        tarball
    );
    cvl_fmt(path, sizeof path, "%s/%s", cache, tarball);
    if (download(url, path) != 0) goto fail;

    char sums[CVL_MAX_PATH], one[CVL_MAX_PATH];
    cvl_fmt(
        url, sizeof url, "%s/releases/download/v%s/SHA256SUMS", b->repo,
        b->version
    );
    cvl_fmt(sums, sizeof sums, "%s/SHA256SUMS", cache);
    cvl_fmt(one, sizeof one, "%s/%s.sha256", cache, name);
    if (download(url, sums) != 0) goto fail;
    if (filter_sums(sums, one, tarball) != 0) {
        fprintf(stderr, "err: %s is not listed in SHA256SUMS\n", tarball);
        goto fail;
    }
    char one_name[CVL_MAX_NAME + 8];
    cvl_fmt(one_name, sizeof one_name, "%s.sha256", name);
    if (verify_sha256(cache, one_name) != 0) {
        fprintf(stderr, "err: checksum mismatch for %s\n", tarball);
        goto fail;
    }

    CvlCmd cmd;
    cvl_cmd_init(&cmd, "tar");
    cvl_cmd_arg(&cmd, "-xzf");
    cvl_cmd_arg(&cmd, tarball);
    if (cvl_cmd_run(&cmd, cache, NULL) != 0) goto fail;
    remove(path);
    remove(sums);
    remove(one);
    printf("\n");
    if (lib_from_dir(b, dir, platform, inc, lib) == 0) return 0;

fail:
    remove(path);
    fprintf(
        stderr,
        "err: could not install %s-cxx v%s for %s.\n"
        "     Set %s to a %s-cxx checkout or release,\n"
        "     or use TypeScript tests ([test] runner = \"ts\").\n",
        b->name, b->version, platform, b->env, b->name
    );
    return -1;
}

static const Backend *backend_named(const char *name) {
    for (int i = 0; i < NBACKENDS; i++)
        if (strcmp(BACKENDS[i].name, name) == 0) return &BACKENDS[i];
    return NULL;
}

static const Backend *file_backend(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    const Backend *found = NULL;
    char line[512];
    while (!found && fgets(line, sizeof line, f)) {
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p++ != '#') continue;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "define", 6) != 0) continue;
        p += 6;
        if (*p != ' ' && *p != '\t') continue;
        while (*p == ' ' || *p == '\t') p++;
        for (int i = 0; i < NBACKENDS; i++) {
            size_t n = strlen(BACKENDS[i].define);
            if (strncmp(p, BACKENDS[i].define, n) == 0 &&
                (p[n] == '\0' || p[n] == '\n' || p[n] == '\r' || p[n] == ' ' ||
                 p[n] == '\t' || p[n] == '/'))
                found = &BACKENDS[i];
        }
    }
    fclose(f);
    return found;
}

static int is_c_file(const char *name) {
    size_t n = strlen(name);
    return n > 2 && strcmp(name + n - 2, ".c") == 0 && name[0] != '.';
}

static int cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static int list_c_tests(char names[][CVL_MAX_NAME]) {
    DIR *d = opendir("tests");
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < MAX_TESTS) {
        if (!is_c_file(e->d_name)) continue;
        if (strlen(e->d_name) >= CVL_MAX_NAME) continue;
        cvl_copy(names[n++], CVL_MAX_NAME, e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)n, CVL_MAX_NAME, cmp_str);
    return n;
}

static int run_c(const CvlConfig *cfg, const char *filter) {
    char tests[MAX_TESTS][CVL_MAX_NAME];
    int ntests = list_c_tests(tests);
    if (ntests == 0) {
        fprintf(stderr, "err: no tests/*.c found\n");
        return 1;
    }

    const Backend *fallback =
        backend_named(cfg->backend[0] ? cfg->backend : "litesvm");
    const char *platform = host_platform();
    if (!platform) {
        fprintf(
            stderr, "err: no prebuilt test backends for this platform; use "
                    "TypeScript tests ([test] runner = \"ts\")\n"
        );
        return 1;
    }

    const char *caravel_inc = getenv("CARAVEL_INCLUDE");
    char inc_buf[CVL_MAX_PATH];
    if (!caravel_inc) caravel_inc = cvl_find_include(inc_buf, sizeof inc_buf);
    if (!caravel_inc) {
        fprintf(
            stderr, "err: cannot locate SDK headers; set CARAVEL_INCLUDE\n"
        );
        return 1;
    }

    printf("  [2/3] Compiling tests...\n\n");
    char inc[NBACKENDS][CVL_MAX_PATH], lib[NBACKENDS][CVL_MAX_PATH];
    int located[NBACKENDS] = {0};

    const char *cc = getenv("CC");
    if (!cc || !*cc) cc = "cc";
    int darwin = strncmp(platform, "darwin", 6) == 0;

    char out_dir[CVL_MAX_PATH];
    cvl_fmt(out_dir, sizeof out_dir, "%s/tests", cfg->build_dir);
    cvl_mkdir_p(out_dir);

    char bins[MAX_TESTS][CVL_MAX_PATH];
    for (int i = 0; i < ntests; i++) {
        size_t stem = strlen(tests[i]) - 2;
        if (cvl_fmt(
                bins[i], CVL_MAX_PATH, "%s/%.*s", out_dir, (int)stem, tests[i]
            ))
            return 1;

        char src[CVL_MAX_PATH];
        cvl_fmt(src, sizeof src, "tests/%s", tests[i]);
        const Backend *own = file_backend(src);
        const Backend *b   = own ? own : fallback;
        if (!b->repo) {
            fprintf(
                stderr, "err: tests/%s: the %s backend is not available yet\n",
                tests[i], b->name
            );
            return 1;
        }
        int bi = (int)(b - BACKENDS);
        if (!located[bi]) {
            if (find_lib(b, platform, inc[bi], lib[bi]) != 0) return 1;
            located[bi] = 1;
        }

        CvlCmd cmd;
        cvl_cmd_init(&cmd, cc);
        cvl_cmd_args(
            &cmd, "-std=c11 -O1 -g -Wall -Wextra -Wno-unused-function"
        );
        cvl_cmd_argf(&cmd, "-I%s", caravel_inc);
        cvl_cmd_argf(&cmd, "-I%s", inc[bi]);
        cvl_cmd_arg(&cmd, "-iquote");
        cvl_cmd_arg(&cmd, cfg->src_dir);
        cvl_cmd_args(&cmd, "-DNO_HEAP -DCARAVEL_TEST");
        if (!own) cvl_cmd_argf(&cmd, "-D%s", b->define);
        cvl_cmd_argf(
            &cmd, "-DCARAVEL_TEST_PROGRAM=\"%s/program.so\"", cfg->build_dir
        );
        cvl_cmd_arg(&cmd, src);
        cvl_cmd_arg(&cmd, lib[bi]);
        cvl_cmd_args(
            &cmd,
            darwin ? "-liconv -lm" : "-lgcc_s -lutil -lrt -lpthread -lm -ldl"
        );
        cvl_cmd_arg(&cmd, "-o");
        cvl_cmd_arg(&cmd, bins[i]);
        int rc = cvl_cmd_run(&cmd, NULL, NULL);
        if (rc != 0) {
            fprintf(stderr, "\nerr: failed to compile tests/%s\n", tests[i]);
            return 1;
        }
    }

    printf("\n  [3/3] Running tests...\n");
    int failed = 0;
    for (int i = 0; i < ntests; i++) {
        printf("\n  tests/%s\n", tests[i]);
        fflush(stdout);
        CvlCmd cmd;
        cvl_cmd_init(&cmd, bins[i]);
        if (filter) cvl_cmd_arg(&cmd, filter);
        /* the binary prints its own results; keep the command line quiet */
        char **argv = cmd.argv;
        pid_t pid   = fork();
        if (pid == 0) {
            execv(argv[0], argv);
            fprintf(
                stderr, "err: cannot run %s: %s\n", argv[0], strerror(errno)
            );
            _exit(127);
        }
        int status = 0;
        if (pid < 0 || waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) ||
            WEXITSTATUS(status) != 0)
            failed++;
        cvl_cmd_free(&cmd);
    }

    if (failed) {
        fprintf(stderr, "\n  %d test file(s) failed\n", failed);
        return 1;
    }
    return 0;
}

int cmd_test(int argc, char **argv) {
    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    const char *filter = NULL;
    for (int i = 0; i < argc; i++)
        if (argv[i][0] != '-') filter = argv[i];

    const char *runner = cfg.runner;
    if (!runner[0]) {
        DIR *d     = opendir("tests");
        int have_c = 0;
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)))
                if (is_c_file(e->d_name)) have_c = 1;
            closedir(d);
        }
        runner = have_c                                  ? "c"
                 : cvl_file_exists("tests/package.json") ? "ts"
                                                         : "c";
    }
    if (strcmp(runner, "c") != 0 && strcmp(runner, "ts") != 0) {
        fprintf(
            stderr, "err: [test] runner must be \"c\" or \"ts\", not \"%s\"\n",
            runner
        );
        return 1;
    }

    const Backend *backend =
        backend_named(cfg.backend[0] ? cfg.backend : "litesvm");
    if (!backend || !backend->repo) {
        fprintf(
            stderr,
            "err: [test] backend must be \"litesvm\", \"mollusk\" or "
            "\"seashell\", not "
            "\"%s\"\n",
            cfg.backend
        );
        return 1;
    }

    printf("\n  Testing %s v%s\n\n", cfg.name, cfg.version);

    printf("  [1/3] Building program...\n\n");
    int rc = cmd_build(argc, argv);
    if (rc != 0) {
        fprintf(stderr, "\nerr: build failed — aborting tests\n");
        return rc;
    }

    rc = strcmp(runner, "ts") == 0 ? run_ts() : run_c(&cfg, filter);
    if (rc != 0) return rc;

    printf("\n  All tests passed.\n\n");
    return 0;
}
