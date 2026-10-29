#ifndef CVL_CLI_H
#define CVL_CLI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>

#include "../include/caravel/version.h"

#define CVL_CLI_VERSION CARAVEL_VERSION_STRING
#define CVL_MAX_PATH    1024
#define CVL_MAX_NAME    128
/** :/ */
#define CVL_LITESVM_CXX_VERSION  "0.1.0"
#define CVL_LITESVM_CXX_REPO     "https://github.com/joeymeere/litesvm-cxx"
#define CVL_MOLLUSK_CXX_VERSION  "0.1.0"
#define CVL_MOLLUSK_CXX_REPO     "https://github.com/joeymeere/mollusk-cxx"
#define CVL_SEASHELL_CXX_VERSION "0.1.0"
#define CVL_SEASHELL_CXX_REPO    "https://github.com/joeymeere/seashell-cxx"

typedef struct {
    char name[CVL_MAX_NAME];
    char version[32];
    char src_dir[CVL_MAX_PATH];
    char build_dir[CVL_MAX_PATH];
    char runner[8];   /* [test] runner: "c", "ts", or "" to detect */
    char backend[16]; /* [test] backend: "litesvm", "mollusk", "seashell" */
} CvlConfig;

static inline void cvl_config_defaults(CvlConfig *cfg) {
    strcpy(cfg->name, "my_program");
    strcpy(cfg->version, "0.1.0");
    strcpy(cfg->src_dir, "src");
    strcpy(cfg->build_dir, "build");
    cfg->runner[0]  = '\0';
    cfg->backend[0] = '\0';
}

int cmd_init(int argc, char **argv);
int cmd_build(int argc, char **argv);
int cmd_test(int argc, char **argv);
int cmd_deploy(int argc, char **argv);
int cmd_clean(int argc, char **argv);
int cmd_idl(int argc, char **argv);
int cmd_dump(int argc, char **argv);

int cvl_config_load(const char *path, CvlConfig *cfg);

int cvl_scaffold_project(
    const char *name,
    const char *sbf_ver,
    const char *tests,
    const char *backend
);

const char *cvl_find_tool(
    const char *env_var, const char *tool_name, char *buf, size_t bufsz
);

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

int cvl_fmt(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

void cvl_copy(char *dst, size_t size, const char *src);

const char *cvl_find_include(char *buf, size_t size);

typedef struct {
    char **argv;
    int argc;
    int cap;
    int oom;
} CvlCmd;

void cvl_cmd_init(CvlCmd *c, const char *prog);
void cvl_cmd_arg(CvlCmd *c, const char *arg);
void cvl_cmd_argf(CvlCmd *c, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void cvl_cmd_args(CvlCmd *c, const char *flags);
void cvl_cmd_free(CvlCmd *c);

int cvl_cmd_run(CvlCmd *c, const char *cwd, const char *stdout_path);

#endif /* CVL_CLI_H */
