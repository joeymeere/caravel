#ifndef CVL_CLI_H
#define CVL_CLI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#define CVL_CLI_VERSION "0.3.2"

#ifdef __APPLE__
#define CVL_INSTALL_LLVM_HINT "  Install LLVM: brew install llvm\n"
#elif defined(__linux__)
#define CVL_INSTALL_LLVM_HINT "  Install LLVM: sudo apt install llvm clang  (or your distro equivalent)\n"
#else
#define CVL_INSTALL_LLVM_HINT "  Install LLVM: https://releases.llvm.org/\n"
#endif
#define CVL_MAX_PATH 1024
#define CVL_MAX_NAME 128

typedef struct {
    char name[CVL_MAX_NAME];
    char version[32];
    char src_dir[CVL_MAX_PATH];
    char build_dir[CVL_MAX_PATH];
} CvlConfig;

static inline void cvl_config_defaults(CvlConfig *cfg) {
    strcpy(cfg->name, "my_program");
    strcpy(cfg->version, "0.1.0");
    strcpy(cfg->src_dir, "src");
    strcpy(cfg->build_dir, "build");
}

int cmd_init(int argc, char **argv);
int cmd_build(int argc, char **argv);
int cmd_test(int argc, char **argv);
int cmd_deploy(int argc, char **argv);
int cmd_clean(int argc, char **argv);
int cmd_idl(int argc, char **argv);

int cvl_config_load(const char *path, CvlConfig *cfg);

int cvl_scaffold_project(const char *name);

static inline int cvl_mkdir_p(const char *path) {
    char tmp[CVL_MAX_PATH];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

static inline int cvl_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static inline int cvl_path_is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;
    return access(path, X_OK) == 0;
}

static inline int cvl_has_command(const char *name) {
    if (!name || !name[0]) return 0;

    if (strchr(name, '/')) return cvl_path_is_executable(name);

    const char *path_env = getenv("PATH");
    if (!path_env || !path_env[0]) return 0;

    char buf[CVL_MAX_PATH];
    const char *p = path_env;
    for (;;) {
        const char *end = strchr(p, ':');
        size_t dirlen = end ? (size_t)(end - p) : strlen(p);

        int n;
        if (dirlen == 0)
            n = snprintf(buf, sizeof(buf), "%s", name);
        else
            n = snprintf(buf, sizeof(buf), "%.*s/%s", (int)dirlen, p, name);

        if (n > 0 && (size_t)n < sizeof(buf) && cvl_path_is_executable(buf))
            return 1;

        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static inline const char *cvl_resolve_include(char *buf, size_t bufsz) {
    const char *env = getenv("CARAVEL_INCLUDE");
    if (env && cvl_file_exists(env)) return env;

    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, bufsz, "%s/.caravel/include", home);
        if (cvl_file_exists(buf)) return buf;
    }

    if (cvl_file_exists("../../include")) return "../../include";

    return NULL;
}

static inline int cvl_run_command(const char *cmd) {
    printf("  > %s\n", cmd);
    return system(cmd);
}

#endif /* CVL_CLI_H */
