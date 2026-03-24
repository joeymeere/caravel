/*
 * @brief IDL gen
 * supported annotations (as C comments):
 *   @cvl:instruction <name> <discriminator>
 *   @cvl:account     <name> <flags>        (within instruction scope)
 *   @cvl:field        <name> <type>         (within instruction scope)
 *   @cvl:state        <name>                (marks a state struct)
 */

#include "cli.h"
#include "config.h"

#define MAX_INSTRUCTIONS 64
#define MAX_ACCOUNTS     16
#define MAX_FIELDS       16
#define MAX_STATES       32

typedef struct {
    char name[CVL_MAX_NAME];
    char flags[CVL_MAX_NAME]; /* esc "mut,signer" */
} IdlAccount;

typedef struct {
    char name[CVL_MAX_NAME];
    char type[CVL_MAX_NAME];
} IdlField;

typedef struct {
    char       name[CVL_MAX_NAME];
    int        discriminator;
    IdlAccount accounts[MAX_ACCOUNTS];
    int        num_accounts;
    IdlField   args[MAX_FIELDS];
    int        num_args;
} IdlInstruction;

typedef struct {
    char     name[CVL_MAX_NAME];
    IdlField fields[MAX_FIELDS];
    int      num_fields;
} IdlState;

static IdlInstruction g_instructions[MAX_INSTRUCTIONS];
static int            g_num_instructions = 0;

static IdlState       g_states[MAX_STATES];
static int            g_num_states = 0;

/* ptr to the "current" ix being built */
static IdlInstruction *g_cur_ix = NULL;

/* ptr to the "current" state being built */
static IdlState       *g_cur_state = NULL;

/* parse single line looking for `@cvl:` annotations. */
static void parse_line(const char *raw_line) {
    char line[2048];
    strncpy(line, raw_line, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    char *at = strstr(line, "@cvl:");
    if (!at) return;

    char *p = at + 5; /* skip "@cvl:" */

    char directive[64] = {0};
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
           && i < (int)sizeof(directive) - 1) {
        directive[i++] = *p++;
    }
    directive[i] = '\0';

    while (*p == ' ' || *p == '\t') p++;

    /* @cvl:instruction <name> <discriminator> */
    if (strcmp(directive, "instruction") == 0) {
        if (g_num_instructions >= MAX_INSTRUCTIONS) return;

        IdlInstruction *ix = &g_instructions[g_num_instructions++];
        memset(ix, 0, sizeof(*ix));

        /* name */
        i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
               && i < CVL_MAX_NAME - 1) {
            ix->name[i++] = *p++;
        }
        ix->name[i] = '\0';

        /* skip whitespace */
        while (*p == ' ' || *p == '\t') p++;

        /* discriminator */
        ix->discriminator = atoi(p);

        g_cur_ix    = ix;
        g_cur_state = NULL;
        return;
    }

    /* @cvl:account <name> <flags> */
    if (strcmp(directive, "account") == 0) {
        if (!g_cur_ix) return;
        if (g_cur_ix->num_accounts >= MAX_ACCOUNTS) return;

        IdlAccount *acc = &g_cur_ix->accounts[g_cur_ix->num_accounts++];
        memset(acc, 0, sizeof(*acc));

        /* name */
        i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
               && i < CVL_MAX_NAME - 1) {
            acc->name[i++] = *p++;
        }
        acc->name[i] = '\0';

        while (*p == ' ' || *p == '\t') p++;

        /* flags */
        i = 0;
        while (*p && *p != '*' && *p != '/' && *p != '\n' && *p != '\r'
               && i < CVL_MAX_NAME - 1) {
            acc->flags[i++] = *p++;
        }
        acc->flags[i] = '\0';
        /* trim trailing whitespace from flags */
        while (i > 0 && (acc->flags[i - 1] == ' ' || acc->flags[i - 1] == '\t'))
            acc->flags[--i] = '\0';

        return;
    }

    /* @cvl:field <name> <type> */
    if (strcmp(directive, "field") == 0) {
        /* could belong to an instruction or a state? */
        if (g_cur_state) {
            if (g_cur_state->num_fields >= MAX_FIELDS) return;

            IdlField *f = &g_cur_state->fields[g_cur_state->num_fields++];
            memset(f, 0, sizeof(*f));

            i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
                   && i < CVL_MAX_NAME - 1)
                f->name[i++] = *p++;
            f->name[i] = '\0';

            while (*p == ' ' || *p == '\t') p++;

            i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
                   && *p != '\n' && *p != '\r' && i < CVL_MAX_NAME - 1)
                f->type[i++] = *p++;
            f->type[i] = '\0';
        } else if (g_cur_ix) {
            if (g_cur_ix->num_args >= MAX_FIELDS) return;

            IdlField *f = &g_cur_ix->args[g_cur_ix->num_args++];
            memset(f, 0, sizeof(*f));

            i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
                   && i < CVL_MAX_NAME - 1)
                f->name[i++] = *p++;
            f->name[i] = '\0';

            while (*p == ' ' || *p == '\t') p++;

            i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
                   && *p != '\n' && *p != '\r' && i < CVL_MAX_NAME - 1)
                f->type[i++] = *p++;
            f->type[i] = '\0';
        }
        return;
    }

    /* @cvl:state <name> */
    if (strcmp(directive, "state") == 0) {
        if (g_num_states >= MAX_STATES) return;

        IdlState *st = &g_states[g_num_states++];
        memset(st, 0, sizeof(*st));

        i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
               && *p != '\n' && *p != '\r' && i < CVL_MAX_NAME - 1)
            st->name[i++] = *p++;
        st->name[i] = '\0';

        g_cur_state = st;
        g_cur_ix    = NULL;
        return;
    }
}

/* scan a single file for annotations */
static int scan_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "warning: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        parse_line(line);
    }

    fclose(fp);
    return 0;
}

/* scan all src and header files in a directory */
static int scan_directory(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp) {
        fprintf(stderr, "err: cannot open directory '%s': %s\n",
                dir, strerror(errno));
        return -1;
    }

    struct dirent *ent;
    char path[CVL_MAX_PATH];

    while ((ent = readdir(dp)) != NULL) {
        size_t len = strlen(ent->d_name);
        bool is_c = (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0);
        bool is_h = (len > 2 && strcmp(ent->d_name + len - 2, ".h") == 0);

        if (is_c || is_h) {
            snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
            scan_file(path);
        }
    }

    closedir(dp);
    return 0;
}

/* map CVL type annotations to IDL types */
static const char *map_type(const char *cvl_type) {
    if (strcmp(cvl_type, "u8") == 0)      return "u8";
    if (strcmp(cvl_type, "u16") == 0)     return "u16";
    if (strcmp(cvl_type, "u32") == 0)     return "u32";
    if (strcmp(cvl_type, "u64") == 0)     return "u64";
    if (strcmp(cvl_type, "i8") == 0)      return "i8";
    if (strcmp(cvl_type, "i16") == 0)     return "i16";
    if (strcmp(cvl_type, "i32") == 0)     return "i32";
    if (strcmp(cvl_type, "i64") == 0)     return "i64";
    if (strcmp(cvl_type, "bool") == 0)    return "bool";
    if (strcmp(cvl_type, "pubkey") == 0)  return "publicKey";
    if (strcmp(cvl_type, "string") == 0)  return "string";
    if (strcmp(cvl_type, "bytes") == 0)   return "bytes";
    return cvl_type; /* pass thru unknowns */
}

static int emit_idl(const char *output_path, const CvlConfig *cfg) {
    FILE *fp = fopen(output_path, "w");
    if (!fp) {
        fprintf(stderr, "err: cannot create %s: %s\n",
                output_path, strerror(errno));
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": \"%s\",\n", cfg->version);
    fprintf(fp, "  \"name\": \"%s\",\n", cfg->name);

    fprintf(fp, "  \"instructions\": [\n");
    for (int i = 0; i < g_num_instructions; i++) {
        IdlInstruction *ix = &g_instructions[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", ix->name);
        fprintf(fp, "      \"discriminator\": [%d],\n", ix->discriminator);

        fprintf(fp, "      \"accounts\": [\n");
        for (int a = 0; a < ix->num_accounts; a++) {
            IdlAccount *acc = &ix->accounts[a];

            bool is_mut    = (strstr(acc->flags, "mut") != NULL);
            bool is_signer = (strstr(acc->flags, "signer") != NULL);

            fprintf(fp, "        {\n");
            fprintf(fp, "          \"name\": \"%s\",\n", acc->name);
            fprintf(fp, "          \"isMut\": %s,\n", is_mut ? "true" : "false");
            fprintf(fp, "          \"isSigner\": %s\n", is_signer ? "true" : "false");
            fprintf(fp, "        }%s\n",
                    (a < ix->num_accounts - 1) ? "," : "");
        }
        fprintf(fp, "      ],\n");

        fprintf(fp, "      \"args\": [\n");
        for (int f = 0; f < ix->num_args; f++) {
            IdlField *field = &ix->args[f];
            fprintf(fp, "        {\n");
            fprintf(fp, "          \"name\": \"%s\",\n", field->name);
            fprintf(fp, "          \"type\": \"%s\"\n", map_type(field->type));
            fprintf(fp, "        }%s\n",
                    (f < ix->num_args - 1) ? "," : "");
        }
        fprintf(fp, "      ]\n");

        fprintf(fp, "    }%s\n",
                (i < g_num_instructions - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"accounts\": [\n");
    for (int s = 0; s < g_num_states; s++) {
        IdlState *st = &g_states[s];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", st->name);
        fprintf(fp, "      \"type\": {\n");
        fprintf(fp, "        \"kind\": \"struct\",\n");
        fprintf(fp, "        \"fields\": [\n");
        for (int f = 0; f < st->num_fields; f++) {
            IdlField *field = &st->fields[f];
            fprintf(fp, "          {\n");
            fprintf(fp, "            \"name\": \"%s\",\n", field->name);
            fprintf(fp, "            \"type\": \"%s\"\n", map_type(field->type));
            fprintf(fp, "          }%s\n",
                    (f < st->num_fields - 1) ? "," : "");
        }
        fprintf(fp, "        ]\n");
        fprintf(fp, "      }\n");
        fprintf(fp, "    }%s\n",
                (s < g_num_states - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n");

    fprintf(fp, "}\n");
    fclose(fp);
    return 0;
}

int cmd_idl(int argc, char **argv) {
    (void)argc; (void)argv;

    CvlConfig cfg;
    if (cvl_config_load("Caravel.toml", &cfg) != 0) {
        fprintf(stderr, "err: Caravel.toml not found\n");
        return 1;
    }

    g_num_instructions = 0;
    g_num_states       = 0;
    g_cur_ix           = NULL;
    g_cur_state        = NULL;

    if (scan_directory(cfg.src_dir) != 0) return 1;

    cvl_mkdir_p(cfg.build_dir);

    char output[CVL_MAX_PATH];
    snprintf(output, sizeof(output), "%s/idl.json", cfg.build_dir);

    if (emit_idl(output, &cfg) != 0) return 1;

    printf("  [IDL] %s  (%d instruction(s), %d state type(s))\n",
           output, g_num_instructions, g_num_states);

    return 0;
}
