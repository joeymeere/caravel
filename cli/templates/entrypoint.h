#ifndef TPL_ENTRYPOINT_H
#define TPL_ENTRYPOINT_H

static const char *TPL_ENTRYPOINT_C =
"#include <caravel.h>\n"
"#include \"state.h\"\n"
"\n"
"#define INIT_ACCOUNTS(X) \\\n"
"    X(payer,          SIGNER | WRITABLE) \\\n"
"    X(counter,        SIGNER | WRITABLE) \\\n"
"    X(system_program, PROGRAM)\n"
"\n"
"IX(0, initialize, INIT_ACCOUNTS)\n"
"\n"
"static uint64_t initialize(\n"
"    initialize_accounts_t *ctx, initialize_args_t *args, Parameters *params\n"
") {\n"
"    (void)args;\n"
"\n"
"    Rent rent;\n"
"    get_rent(&rent);\n"
"    uint64_t lamports = minimum_balance(&rent, sizeof(CounterState));\n"
"\n"
"    TRY(system_create_account(\n"
"        ctx->payer, ctx->counter,\n"
"        lamports, sizeof(CounterState), params->program_id,\n"
"        params->accounts, (int)params->accounts_len\n"
"    ));\n"
"\n"
"    CounterState *state = ACCOUNT_STATE(ctx->counter, CounterState);\n"
"    state->count = 0;\n"
"    sol_memcpy_(state->authority.bytes, ctx->payer->key->bytes, 32);\n"
"\n"
"    log_literal(\"initialized\");\n"
"    return SUCCESS;\n"
"}\n"
"\n"
"ENTRYPOINT(\n"
"    HANDLER(initialize)\n"
")\n";

static const char *TPL_STATE_H =
"#ifndef %s_STATE_H\n"
"#define %s_STATE_H\n"
"\n"
"#include <caravel.h>\n"
"\n"
"STATE(CounterState)\n"
"typedef struct {\n"
"    uint64_t  count;\n"
"    Pubkey authority;\n"
"} CounterState;\n"
"\n"
"#endif /* %s_STATE_H */\n";

static const char *TPL_INSTRUCTIONS_H =
"#ifndef %s_INSTRUCTIONS_H\n"
"#define %s_INSTRUCTIONS_H\n"
"\n"
"/*\n"
" * Add additional instruction handlers here.\n"
" * Include this file from entrypoint.c and add to DISPATCH.\n"
" */\n"
"\n"
"#endif /* %s_INSTRUCTIONS_H */\n";

#endif /* TPL_ENTRYPOINT_H */
