#include "config.h"

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                       s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
    return s;
}

static int split_pair(char *line, char **key, char **val) {
    char *eq = strchr(line, '=');
    if (!eq) return -1;
    *eq  = '\0';
    *key = trim(line);
    if (**key == '\0') return -1;

    char *p = eq + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return -1;
    p++;

    char *end = strchr(p, '"');
    if (!end) return -1;
    *end = '\0';

    char *rest = trim(end + 1);
    if (*rest != '\0' && *rest != '#') return -1;

    *val = p;
    return 0;
}

typedef struct {
    const char *section;
    const char *key;
    size_t offset;
    size_t size;
} ConfigKey;

#define KEY(sec, field) \
    {sec, #field, offsetof(CvlConfig, field), sizeof(((CvlConfig *)0)->field)}

static const ConfigKey KEYS[] = {
    KEY("program", name),
    KEY("program", version),
    KEY("build", src_dir),
    KEY("build", build_dir),
};

int cvl_config_load(const char *path, CvlConfig *cfg) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "err: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    cvl_config_defaults(cfg);

    char section[32] = "";
    char line[CVL_MAX_PATH + 64];
    int lineno = 0;
    int rc     = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *l = trim(line);

        /* blank / comment */
        if (l[0] == '\0' || l[0] == '#') continue;

        /* section header */
        if (l[0] == '[') {
            char *close = strchr(l, ']');
            if (!close) {
                fprintf(
                    stderr, "warn: %s:%d: malformed section header\n", path,
                    lineno
                );
                section[0] = '\0';
                continue;
            }
            *close = '\0';
            cvl_copy(section, sizeof(section), trim(l + 1));
            int known = 0;
            for (size_t k = 0; k < sizeof(KEYS) / sizeof(*KEYS); k++)
                if (strcmp(KEYS[k].section, section) == 0) known = 1;
            if (!known)
                fprintf(
                    stderr, "warn: %s:%d: unknown section [%s]\n", path, lineno,
                    section
                );
            continue;
        }

        char *key, *val;
        if (split_pair(l, &key, &val) != 0) {
            fprintf(
                stderr, "warn: %s:%d: expected key = \"value\"\n", path, lineno
            );
            continue;
        }

        const ConfigKey *match = NULL;
        for (size_t k = 0; k < sizeof(KEYS) / sizeof(*KEYS); k++)
            if (strcmp(KEYS[k].section, section) == 0 &&
                strcmp(KEYS[k].key, key) == 0)
                match = &KEYS[k];

        if (!match) {
            fprintf(
                stderr, "warn: %s:%d: unknown key '%s' in [%s]\n", path, lineno,
                key, section
            );
            continue;
        }
        if (strlen(val) >= match->size) {
            fprintf(
                stderr,
                "err: %s:%d: value for '%s' is too long "
                "(max %zu characters)\n",
                path, lineno, key, match->size - 1
            );
            rc = -1;
            continue;
        }
        cvl_copy((char *)cfg + match->offset, match->size, val);
    }

    fclose(fp);
    return rc;
}
