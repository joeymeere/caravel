#define _GNU_SOURCE /* vasprintf, strndup, realpath on glibc with -std=c11 */
#include "cli.h"

#include <fcntl.h>
#include <stdarg.h>
#include <sys/wait.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

int cvl_fmt(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= size) {
        fprintf(
            stderr, "err: path or argument too long (limit %zu): %.60s...\n",
            size - 1, buf
        );
        return -1;
    }
    return 0;
}

void cvl_copy(char *dst, size_t size, const char *src) {
    if (size == 0) return;
    size_t n = strlen(src);
    if (n >= size) n = size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int exe_dir(char *buf, size_t size) {
    char path[CVL_MAX_PATH];
#ifdef __APPLE__
    uint32_t len = sizeof(path);
    if (_NSGetExecutablePath(path, &len) != 0) return -1;
#else
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) return -1;
    path[len] = '\0';
#endif
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) return -1;
    char *slash = strrchr(resolved, '/');
    if (!slash) return -1;
    *slash = '\0';
    cvl_copy(buf, size, resolved);
    return 0;
}

const char *cvl_find_include(char *buf, size_t size) {
    char dir[CVL_MAX_PATH];
    if (exe_dir(dir, sizeof(dir)) == 0) {
        snprintf(buf, size, "%s/../share/caravel/include/caravel.h", dir);
        if (cvl_file_exists(buf)) {
            snprintf(buf, size, "%s/../share/caravel/include", dir);
            return buf;
        }
    }

    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, size, "%s/.local/share/caravel/include", home);
        if (cvl_file_exists(buf)) return buf;
    }
    if (cvl_file_exists("/usr/local/share/caravel/include"))
        return "/usr/local/share/caravel/include";
    return NULL;
}

static void cmd_push(CvlCmd *c, char *owned) {
    if (!owned) {
        c->oom = 1;
        return;
    }
    if (c->argc + 2 > c->cap) {
        int cap     = c->cap ? c->cap * 2 : 16;
        char **argv = realloc(c->argv, (size_t)cap * sizeof(char *));
        if (!argv) {
            free(owned);
            c->oom = 1;
            return;
        }
        c->argv = argv;
        c->cap  = cap;
    }
    c->argv[c->argc++] = owned;
    c->argv[c->argc]   = NULL;
}

void cvl_cmd_init(CvlCmd *c, const char *prog) {
    memset(c, 0, sizeof(*c));
    cvl_cmd_arg(c, prog);
}

void cvl_cmd_arg(CvlCmd *c, const char *arg) {
    cmd_push(c, strdup(arg));
}

void cvl_cmd_argf(CvlCmd *c, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = NULL;
    if (vasprintf(&s, fmt, ap) < 0) s = NULL;
    va_end(ap);
    cmd_push(c, s);
}

void cvl_cmd_args(CvlCmd *c, const char *flags) {
    const char *p = flags;
    while (*p) {
        while (*p == ' ') p++;
        const char *start = p;
        while (*p && *p != ' ') p++;
        if (p > start) cmd_push(c, strndup(start, (size_t)(p - start)));
    }
}

void cvl_cmd_free(CvlCmd *c) {
    for (int i = 0; i < c->argc; i++) free(c->argv[i]);
    free(c->argv);
    memset(c, 0, sizeof(*c));
}

static void print_arg(const char *a) {
    int plain = *a != '\0';
    for (const char *p = a; *p; p++)
        if (!(isalnum((unsigned char)*p) || strchr("-_./=+,:@%", *p)))
            plain = 0;
    if (plain) {
        fputs(a, stdout);
        return;
    }
    putchar('\'');
    for (const char *p = a; *p; p++) {
        if (*p == '\'')
            fputs("'\\''", stdout);
        else
            putchar(*p);
    }
    putchar('\'');
}

int cvl_cmd_run(CvlCmd *c, const char *cwd, const char *stdout_path) {
    if (c->oom || c->argc == 0) {
        fprintf(stderr, "err: out of memory building command\n");
        cvl_cmd_free(c);
        return -1;
    }

    printf("  > ");
    if (cwd) printf("(cd %s) ", cwd);
    for (int i = 0; i < c->argc; i++) {
        if (i) putchar(' ');
        print_arg(c->argv[i]);
    }
    if (stdout_path) {
        printf(" > ");
        print_arg(stdout_path);
    }
    putchar('\n');
    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "err: fork failed: %s\n", strerror(errno));
        cvl_cmd_free(c);
        return -1;
    }
    if (pid == 0) {
        if (cwd && chdir(cwd) != 0) {
            fprintf(
                stderr, "err: cannot enter '%s': %s\n", cwd, strerror(errno)
            );
            _exit(127);
        }
        if (stdout_path) {
            int fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0) {
                fprintf(
                    stderr, "err: cannot write '%s': %s\n", stdout_path,
                    strerror(errno)
                );
                _exit(127);
            }
            close(fd);
        }
        execvp(c->argv[0], c->argv);
        fprintf(
            stderr, "err: cannot run '%s': %s\n", c->argv[0], strerror(errno)
        );
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            status = -1;
            break;
        }
    }
    cvl_cmd_free(c);

    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}
