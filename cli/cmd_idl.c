#include "cli.h"
#include "config.h"

#define MAX_INSTRUCTIONS 64
#define MAX_ACCOUNTS     16
#define MAX_ARGS         16
#define MAX_FIELDS       32
#define MAX_STATES       32
#define MAX_ERRORS       64
#define MAX_PENDING      8
#define MAX_FILES        128

typedef struct {
    char name[CVL_MAX_NAME];
    bool is_writable;
    bool is_signer;
    bool is_program;
    char address[64];
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
    IdlField   args[MAX_ARGS];
    int        num_args;
} IdlInstruction;

typedef struct {
    char     name[CVL_MAX_NAME];
    IdlField fields[MAX_FIELDS];
    int      num_fields;
} IdlState;

typedef struct {
    char name[CVL_MAX_NAME];
    int  code;
} IdlError;

typedef struct {
    char     type_name[CVL_MAX_NAME];
    IdlField fields[MAX_ARGS];
    int      num_fields;
} ArgsType;

static IdlInstruction g_instructions[MAX_INSTRUCTIONS];
static int            g_num_instructions;

static IdlState       g_states[MAX_STATES];
static int            g_num_states;

static IdlError       g_errors[MAX_ERRORS];
static int            g_num_errors;

static ArgsType       g_args_types[MAX_INSTRUCTIONS];
static int            g_num_args_types;

static int  g_pending_ix[MAX_PENDING];
static int  g_num_pending;

static IdlAccount g_temp_accounts[MAX_ACCOUNTS];
static int        g_num_temp_accounts;

static bool      g_in_accounts_macro;
static bool      g_waiting_for_struct;
static IdlState *g_cur_state;

static bool      g_waiting_for_args_struct;
static ArgsType *g_cur_args_type;

static char g_files[MAX_FILES][CVL_MAX_PATH];
static int  g_num_files;

static bool is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_';
}

static void snake_to_camel(const char *snake, char *out, int max) {
    int j = 0;
    bool cap = false;
    for (int i = 0; snake[i] && j < max - 1; i++) {
        if (snake[i] == '_') {
            cap = true;
        } else {
            out[j++] = (cap && snake[i] >= 'a' && snake[i] <= 'z')
                        ? (char)(snake[i] - 32) : snake[i];
            cap = false;
        }
    }
    out[j] = '\0';
}

static void screaming_to_pascal(const char *src, char *out, int max) {
    int j = 0;
    bool cap = true;
    for (int i = 0; src[i] && j < max - 1; i++) {
        if (src[i] == '_') {
            cap = true;
        } else {
            if (cap) {
                out[j++] = src[i]; /* alr uppercase */
            } else {
                out[j++] = (src[i] >= 'A' && src[i] <= 'Z')
                            ? (char)(src[i] + 32) : src[i];
            }
            cap = false;
        }
    }
    out[j] = '\0';
}

static const char *map_c_type(const char *ct) {
    if (strcmp(ct, "uint8_t")  == 0) return "u8";
    if (strcmp(ct, "uint16_t") == 0) return "u16";
    if (strcmp(ct, "uint32_t") == 0) return "u32";
    if (strcmp(ct, "uint64_t") == 0) return "u64";
    if (strcmp(ct, "int8_t")   == 0) return "i8";
    if (strcmp(ct, "int16_t")  == 0) return "i16";
    if (strcmp(ct, "int32_t")  == 0) return "i32";
    if (strcmp(ct, "int64_t")  == 0) return "i64";
    if (strcmp(ct, "CvlPubkey") == 0) return "publicKey";
    if (strcmp(ct, "bool")     == 0) return "bool";
    return ct;
}

static const char *known_program_address(const char *name) {
    if (strcmp(name, "system_program") == 0)
        return "11111111111111111111111111111111";
    if (strcmp(name, "token_program") == 0)
        return "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
    if (strcmp(name, "associated_token_program") == 0)
        return "ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL";
    return NULL;
}

static int parse_struct_field(const char *line, IdlField *out) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '#' || *p == '/' || *p == '*') return 0;

    char ctype[CVL_MAX_NAME] = {0};
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && i < CVL_MAX_NAME - 1)
        ctype[i++] = *p++;
    ctype[i] = '\0';
    while (*p == ' ' || *p == '\t') p++;

    char fname[CVL_MAX_NAME] = {0};
    i = 0;
    while (*p && *p != ';' && *p != '[' && *p != ' ' && *p != '\t'
           && i < CVL_MAX_NAME - 1)
        fname[i++] = *p++;
    fname[i] = '\0';

    if (fname[0] == '_' || fname[0] == '\0') return 0; /* skip pad */

    int array_size = 0;
    if (*p == '[') { p++; array_size = atoi(p); }

    strncpy(out->name, fname, CVL_MAX_NAME - 1);
    if (array_size > 0) {
        snprintf(out->type, CVL_MAX_NAME, "{\"array\":[\"%s\",%d]}",
                 map_c_type(ctype), array_size);
    } else {
        strncpy(out->type, map_c_type(ctype), CVL_MAX_NAME - 1);
    }
    return 1;
}

static void parse_line(const char *raw) {
    char line[2048];
    strncpy(line, raw, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'
                       || line[len - 1] == ' '  || line[len - 1] == '\t'))
        line[--len] = '\0';
    bool continuation = (len > 0 && line[len - 1] == '\\');

    if (g_in_accounts_macro) {
        char *x = strstr(line, "X(");
        if (x && g_num_temp_accounts < MAX_ACCOUNTS) {
            char *p = x + 2;
            /* name */
            char name[CVL_MAX_NAME] = {0};
            int i = 0;
            while (*p && *p != ',' && *p != ')' && i < CVL_MAX_NAME - 1) {
                if (*p != ' ' && *p != '\t') name[i++] = *p;
                p++;
            }
            name[i] = '\0';

            /* flags */
            char flags[256] = {0};
            if (*p == ',') {
                p++;
                i = 0;
                while (*p && *p != ')' && i < 255) flags[i++] = *p++;
                flags[i] = '\0';
            }

            IdlAccount *acc = &g_temp_accounts[g_num_temp_accounts++];
            memset(acc, 0, sizeof(*acc));
            strncpy(acc->name, name, CVL_MAX_NAME - 1);
            acc->is_writable = (strstr(flags, "WRITABLE") != NULL);
            acc->is_signer   = (strstr(flags, "SIGNER")   != NULL);
            acc->is_program  = (strstr(flags, "PROGRAM")  != NULL);
            const char *addr = known_program_address(name);
            if (addr) strncpy(acc->address, addr, 63);
        }
        if (!continuation) g_in_accounts_macro = false;
        return;
    }

    if (g_waiting_for_struct) {
        if (strchr(line, '{')) g_waiting_for_struct = false;
        return;
    }

    if (g_waiting_for_args_struct) {
        if (strchr(line, '{')) g_waiting_for_args_struct = false;
        return;
    }

    if (g_cur_state) {
        if (strchr(line, '}')) { g_cur_state = NULL; return; }
        if (g_cur_state->num_fields < MAX_FIELDS) {
            IdlField f = {0};
            if (parse_struct_field(line, &f))
                g_cur_state->fields[g_cur_state->num_fields++] = f;
        }
        return;
    }

    if (g_cur_args_type) {
        char *close = strchr(line, '}');
        if (close) {
            char *p = close + 1;
            while (*p == ' ' || *p == '\t') p++;
            char tname[CVL_MAX_NAME] = {0};
            int i = 0;
            while (*p && *p != ';' && *p != ' ' && *p != '\t'
                   && i < CVL_MAX_NAME - 1)
                tname[i++] = *p++;
            tname[i] = '\0';
            strncpy(g_cur_args_type->type_name, tname, CVL_MAX_NAME - 1);
            g_cur_args_type = NULL;
            return;
        }
        if (g_cur_args_type->num_fields < MAX_ARGS) {
            IdlField f = {0};
            if (parse_struct_field(line, &f))
                g_cur_args_type->fields[g_cur_args_type->num_fields++] = f;
        }
        return;
    }

    if (strstr(line, "typedef") && strstr(line, "struct")
        && strstr(line, "packed")) {
        if (g_num_args_types < MAX_INSTRUCTIONS) {
            ArgsType *at = &g_args_types[g_num_args_types++];
            memset(at, 0, sizeof(*at));
            g_cur_args_type = at;
            if (!strchr(line, '{'))
                g_waiting_for_args_struct = true;
        }
        return;
    }

    char *state_match = strstr(line, "STATE(");
    if (state_match
        && (state_match == line || !is_ident_char(state_match[-1]))
        && !strstr(line, "#define")) {
        if (g_num_states >= MAX_STATES) return;
        char *p = state_match + 6;
        while (*p == ' ' || *p == '\t') p++;

        IdlState *st = &g_states[g_num_states++];
        memset(st, 0, sizeof(*st));

        int i = 0;
        while (*p && *p != ')' && *p != ' ' && *p != '\t'
               && i < CVL_MAX_NAME - 1)
            st->name[i++] = *p++;
        st->name[i] = '\0';

        g_cur_state = st;
        g_waiting_for_struct = true;
        return;
    }

    char *ix_match = strstr(line, "IX(");
    if (ix_match
        && (ix_match == line || !is_ident_char(ix_match[-1]))
        && !strstr(line, "#define")
        && !strstr(line, "_IX_")
        && !strstr(line, "IX_DATA")) {
        char *p = ix_match + 3;
        while (*p == ' ' || *p == '\t') p++;

        int disc = atoi(p);

        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        while (*p == ' ' || *p == '\t') p++;

        /* 2nd param: instruction name */
        char name[CVL_MAX_NAME] = {0};
        int i = 0;
        while (*p && *p != ',' && *p != ')' && i < CVL_MAX_NAME - 1) {
            if (*p != ' ' && *p != '\t') name[i++] = *p;
            p++;
        }
        name[i] = '\0';

        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        while (*p == ' ' || *p == '\t') p++;

        while (*p && *p != ',' && *p != ')') p++;

        char args_type[CVL_MAX_NAME] = {0};
        if (*p == ',') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            i = 0;
            while (*p && *p != ')' && *p != ' ' && *p != '\t'
                   && i < CVL_MAX_NAME - 1)
                args_type[i++] = *p++;
            args_type[i] = '\0';
        }

        if (g_num_instructions < MAX_INSTRUCTIONS && name[0]) {
            IdlInstruction *ix = &g_instructions[g_num_instructions];
            memset(ix, 0, sizeof(*ix));
            strncpy(ix->name, name, CVL_MAX_NAME - 1);
            ix->discriminator = disc;

            for (int a = 0; a < g_num_temp_accounts && a < MAX_ACCOUNTS; a++)
                ix->accounts[a] = g_temp_accounts[a];
            ix->num_accounts = g_num_temp_accounts;

            if (args_type[0]) {
                for (int t = 0; t < g_num_args_types; t++) {
                    if (strcmp(g_args_types[t].type_name,
                              args_type) == 0) {
                        for (int a = 0;
                             a < g_args_types[t].num_fields
                             && a < MAX_ARGS; a++)
                            ix->args[a] = g_args_types[t].fields[a];
                        ix->num_args = g_args_types[t].num_fields;
                        break;
                    }
                }
            }

            g_num_instructions++;
        }
        return;
    }

    char *at = strstr(line, "@cvl:instruction");
    if (at) {
        if (g_num_instructions >= MAX_INSTRUCTIONS) return;
        char *p = at + 16;
        while (*p == ' ' || *p == '\t') p++;

        IdlInstruction *ix = &g_instructions[g_num_instructions];
        memset(ix, 0, sizeof(*ix));

        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
               && i < CVL_MAX_NAME - 1)
            ix->name[i++] = *p++;
        ix->name[i] = '\0';

        while (*p == ' ' || *p == '\t') p++;
        ix->discriminator = atoi(p);

        if (g_num_pending < MAX_PENDING)
            g_pending_ix[g_num_pending++] = g_num_instructions;
        g_num_instructions++;
        return;
    }

    at = strstr(line, "@cvl:args");
    if (at) {
        if (g_num_instructions == 0) return;
        IdlInstruction *ix = &g_instructions[g_num_instructions - 1];
        char *p = at + 9;
        while (*p == ' ' || *p == '\t') p++;

        while (*p && *p != '*' && *p != '/') {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '*' || *p == '/' || *p == '\0') break;

            char name[CVL_MAX_NAME] = {0};
            int i = 0;
            while (*p && *p != ':' && i < CVL_MAX_NAME - 1) name[i++] = *p++;
            while (i > 0 && (name[i-1] == ' ' || name[i-1] == '\t')) i--;
            name[i] = '\0';

            if (*p != ':') break;
            p++;

            char type[CVL_MAX_NAME] = {0};
            i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
                   && i < CVL_MAX_NAME - 1)
                type[i++] = *p++;
            type[i] = '\0';

            if (name[0] && type[0] && ix->num_args < MAX_ARGS) {
                IdlField *f = &ix->args[ix->num_args++];
                strncpy(f->name, name, CVL_MAX_NAME - 1);
                strncpy(f->type, type, CVL_MAX_NAME - 1);
            }
        }
        return;
    }

    at = strstr(line, "@cvl:state");
    if (at) {
        if (g_num_states >= MAX_STATES) return;
        char *p = at + 10;
        while (*p == ' ' || *p == '\t') p++;

        IdlState *st = &g_states[g_num_states++];
        memset(st, 0, sizeof(*st));

        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '*' && *p != '/'
               && *p != '\n' && *p != '\r' && i < CVL_MAX_NAME - 1)
            st->name[i++] = *p++;
        st->name[i] = '\0';

        g_cur_state = st;
        g_waiting_for_struct = true;
        return;
    }

    if (strstr(line, "#define") && strstr(line, "ERROR_CUSTOM(")) {
        char *p = strstr(line, "#define") + 7;
        while (*p == ' ' || *p == '\t') p++;

        char macro[CVL_MAX_NAME] = {0};
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < CVL_MAX_NAME - 1)
            macro[i++] = *p++;
        macro[i] = '\0';

        char *num_p = strstr(line, "ERROR_CUSTOM(") + 13;
        int n = atoi(num_p);

        char *err_part = strstr(macro, "_ERROR_");
        if (err_part) { err_part += 7; }
        else if ((err_part = strstr(macro, "_ERR_")) != NULL) { err_part += 5; }
        else if (strncmp(macro, "ERROR_", 6) == 0) { err_part = macro + 6; }
        else if (strncmp(macro, "ERR_", 4) == 0) { err_part = macro + 4; }
        else return;

        if (g_num_errors >= MAX_ERRORS) return;
        IdlError *err = &g_errors[g_num_errors++];
        screaming_to_pascal(err_part, err->name, CVL_MAX_NAME);
        err->code = 0x100 + n;
        return;
    }

    if (strstr(line, "#define") && strstr(line, "_ACCOUNTS(X)")) {
        g_in_accounts_macro = true;
        g_num_temp_accounts = 0;
        if (!continuation) g_in_accounts_macro = false;
        return;
    }

    if (strstr(line, "DEFINE_ACCOUNTS(")) {
        for (int pi = 0; pi < g_num_pending; pi++) {
            IdlInstruction *ix = &g_instructions[g_pending_ix[pi]];
            for (int a = 0; a < g_num_temp_accounts && a < MAX_ACCOUNTS; a++)
                ix->accounts[a] = g_temp_accounts[a];
            ix->num_accounts = g_num_temp_accounts;
        }
        g_num_pending = 0;
    }
}

static int scan_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "warning: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    char line[2048];
    while (fgets(line, sizeof(line), fp))
        parse_line(line);
    fclose(fp);
    return 0;
}

static void collect_files(const char *dir, const char *ext) {
    DIR *dp = opendir(dir);
    if (!dp) return;

    struct dirent *ent;
    char path[CVL_MAX_PATH];

    while ((ent = readdir(dp)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            collect_files(path, ext);
        } else {
            size_t nlen = strlen(ent->d_name);
            size_t elen = strlen(ext);
            if (nlen > elen && strcmp(ent->d_name + nlen - elen, ext) == 0) {
                if (g_num_files < MAX_FILES)
                    strncpy(g_files[g_num_files++], path, CVL_MAX_PATH - 1);
            }
        }
    }
    closedir(dp);
}

static int compare_paths(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static int compare_ix(const void *a, const void *b) {
    return ((const IdlInstruction *)a)->discriminator
         - ((const IdlInstruction *)b)->discriminator;
}

static int scan_directory(const char *dir) {
    g_num_files = 0;
    collect_files(dir, ".h");
    qsort(g_files, g_num_files, CVL_MAX_PATH, compare_paths);
    for (int i = 0; i < g_num_files; i++) scan_file(g_files[i]);

    g_num_files = 0;
    collect_files(dir, ".c");
    qsort(g_files, g_num_files, CVL_MAX_PATH, compare_paths);
    for (int i = 0; i < g_num_files; i++) scan_file(g_files[i]);

    return 0;
}

static void emit_type(FILE *fp, const char *type) {
    if (type[0] == '{')
        fprintf(fp, "%s", type);
    else
        fprintf(fp, "\"%s\"", type);
}

static int emit_idl(const char *output_path, const CvlConfig *cfg) {
    FILE *fp = fopen(output_path, "w");
    if (!fp) {
        fprintf(stderr, "err: cannot create %s: %s\n",
                output_path, strerror(errno));
        return -1;
    }

    /* sort ixs by discriminator */
    qsort(g_instructions, g_num_instructions,
          sizeof(IdlInstruction), compare_ix);

    fprintf(fp, "{\n");
    fprintf(fp, "  \"address\": \"\",\n");
    fprintf(fp, "  \"metadata\": {\n");
    fprintf(fp, "    \"name\": \"%s\",\n", cfg->name);
    fprintf(fp, "    \"version\": \"%s\",\n", cfg->version);
    fprintf(fp, "    \"spec\": \"0.1.0\"\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"instructions\": [\n");
    for (int i = 0; i < g_num_instructions; i++) {
        IdlInstruction *ix = &g_instructions[i];
        char cname[CVL_MAX_NAME];
        snake_to_camel(ix->name, cname, CVL_MAX_NAME);

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", cname);
        fprintf(fp, "      \"discriminator\": [%d],\n", ix->discriminator);

        fprintf(fp, "      \"accounts\": [\n");
        for (int a = 0; a < ix->num_accounts; a++) {
            IdlAccount *acc = &ix->accounts[a];
            char cacc[CVL_MAX_NAME];
            snake_to_camel(acc->name, cacc, CVL_MAX_NAME);

            fprintf(fp, "        { \"name\": \"%s\"", cacc);
            if (acc->is_program && acc->address[0]) {
                fprintf(fp, ", \"address\": \"%s\"", acc->address);
            } else {
                fprintf(fp, ", \"writable\": %s",
                        acc->is_writable ? "true" : "false");
                fprintf(fp, ", \"signer\": %s",
                        acc->is_signer ? "true" : "false");
            }
            fprintf(fp, " }%s\n", (a < ix->num_accounts - 1) ? "," : "");
        }
        fprintf(fp, "      ],\n");

        fprintf(fp, "      \"args\": [\n");
        for (int f = 0; f < ix->num_args; f++) {
            char carg[CVL_MAX_NAME];
            snake_to_camel(ix->args[f].name, carg, CVL_MAX_NAME);
            fprintf(fp, "        { \"name\": \"%s\", \"type\": \"%s\" }%s\n",
                    carg, ix->args[f].type,
                    (f < ix->num_args - 1) ? "," : "");
        }
        fprintf(fp, "      ]\n");

        fprintf(fp, "    }%s\n", (i < g_num_instructions - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"accounts\": [\n");
    for (int s = 0; s < g_num_states; s++) {
        fprintf(fp, "    { \"name\": \"%s\", \"discriminator\": [] }%s\n",
                g_states[s].name, (s < g_num_states - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"types\": [\n");
    for (int s = 0; s < g_num_states; s++) {
        IdlState *st = &g_states[s];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", st->name);
        fprintf(fp, "      \"type\": {\n");
        fprintf(fp, "        \"kind\": \"struct\",\n");
        fprintf(fp, "        \"fields\": [\n");
        for (int f = 0; f < st->num_fields; f++) {
            char cf[CVL_MAX_NAME];
            snake_to_camel(st->fields[f].name, cf, CVL_MAX_NAME);
            fprintf(fp, "          { \"name\": \"%s\", \"type\": ", cf);
            emit_type(fp, st->fields[f].type);
            fprintf(fp, " }%s\n", (f < st->num_fields - 1) ? "," : "");
        }
        fprintf(fp, "        ]\n");
        fprintf(fp, "      }\n");
        fprintf(fp, "    }%s\n", (s < g_num_states - 1) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    fprintf(fp, "  \"errors\": [\n");
    for (int e = 0; e < g_num_errors; e++) {
        fprintf(fp, "    { \"code\": %d, \"name\": \"%s\" }%s\n",
                g_errors[e].code, g_errors[e].name,
                (e < g_num_errors - 1) ? "," : "");
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

    g_num_instructions       = 0;
    g_num_states             = 0;
    g_num_errors             = 0;
    g_num_args_types         = 0;
    g_num_pending            = 0;
    g_num_temp_accounts      = 0;
    g_in_accounts_macro      = false;
    g_waiting_for_struct     = false;
    g_waiting_for_args_struct = false;
    g_cur_state              = NULL;
    g_cur_args_type          = NULL;

    if (scan_directory(cfg.src_dir) != 0) return 1;

    cvl_mkdir_p(cfg.build_dir);

    char output[CVL_MAX_PATH];
    snprintf(output, sizeof(output), "%s/idl.json", cfg.build_dir);

    if (emit_idl(output, &cfg) != 0) return 1;

    printf("  [IDL] %s  (%d instruction(s), %d state(s), %d error(s))\n",
           output, g_num_instructions, g_num_states, g_num_errors);

    return 0;
}
