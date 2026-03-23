#include "config.h"

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' '  || s[len - 1] == '\t' ||
                       s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
    return s;
}

/* Extract the value between double quotes: key = "value"
 * Returns pointer into `line` (past the opening quote), with the
 * closing quote replaced by '\0'.  Returns NULL on parse failure. */
static char *extract_quoted(char *line) {
    char *eq = strchr(line, '=');
    if (!eq) return NULL;

    char *p = eq + 1;
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '"') return NULL;
    p++;                            /* skip opening quote */

    char *end = strchr(p, '"');
    if (!end) return NULL;
    *end = '\0';
    return p;
}

int cvl_config_load(const char *path, CvlConfig *cfg) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "err: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    cvl_config_defaults(cfg);

    enum { SEC_NONE, SEC_PROGRAM, SEC_BUILD } section = SEC_NONE;
    char line[CVL_MAX_PATH + 64];

    while (fgets(line, sizeof(line), fp)) {
        char *l = trim(line);

        /* blank / comment */
        if (l[0] == '\0' || l[0] == '#') continue;

        /* section header */
        if (l[0] == '[') {
            if (strncmp(l, "[program]", 9) == 0)      section = SEC_PROGRAM;
            else if (strncmp(l, "[build]", 7) == 0)    section = SEC_BUILD;
            else                                        section = SEC_NONE;
            continue;
        }

        /* key = "value" */
        char *val = NULL;

        if (section == SEC_PROGRAM) {
            if (strncmp(l, "name", 4) == 0) {
                val = extract_quoted(l);
                if (val) snprintf(cfg->name, sizeof(cfg->name), "%s", val);
            } else if (strncmp(l, "version", 7) == 0) {
                val = extract_quoted(l);
                if (val) snprintf(cfg->version, sizeof(cfg->version), "%s", val);
            }
        } else if (section == SEC_BUILD) {
            if (strncmp(l, "src_dir", 7) == 0) {
                val = extract_quoted(l);
                if (val) snprintf(cfg->src_dir, sizeof(cfg->src_dir), "%s", val);
            } else if (strncmp(l, "build_dir", 9) == 0) {
                val = extract_quoted(l);
                if (val) snprintf(cfg->build_dir, sizeof(cfg->build_dir), "%s", val);
            }
        }
        /* ignore unknown keys (maybe err here?) */
    }

    fclose(fp);
    return 0;
}
