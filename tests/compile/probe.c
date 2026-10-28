#include <stdint.h>
#include <stddef.h>
#include <caravel.h>

typedef struct __attribute__((packed)) {
    uint64_t amount;
    uint8_t bump;
} probe_args_t;

STATE(ProbeState)
typedef struct {
    uint64_t value;
    Pubkey authority;
} ProbeState;

#define WITH_ARGS_ACCOUNTS(X) \
    X(payer, SIGNER | WRITABLE) \
    X(target, WRITABLE) \
    X(mint, 0) \
    X(system_program, PROGRAM) \
    X(token_program, PROGRAM)

IX(0, with_args, WITH_ARGS_ACCOUNTS, probe_args_t)

#define NO_ARGS_ACCOUNTS(X) X(only, SIGNER)

IX(1, no_args, NO_ARGS_ACCOUNTS)

#define STANDALONE_ACCOUNTS(X) \
    X(a, WRITABLE) \
    X(b, 0)

DEFINE_ACCOUNTS(standalone, STANDALONE_ACCOUNTS)

static uint64_t
asserts(with_args_accounts_t *ctx, Parameters *params, probe_args_t *args) {
    ASSERT_SIGNER(ctx->payer);
    ASSERT_WRITABLE(ctx->target);
    ASSERT_OWNER(ctx->target, params->program_id);
    ASSERT_KEY(ctx->system_program, &SYSTEM_PROGRAM_ID);
    ASSERT_INITIALIZED(ctx->target);
    ASSERT_DATA_LEN(ctx->target, sizeof(ProbeState));
    ASSERT_EXECUTABLE(ctx->token_program);

    SignerSeed seeds[] = {
        SEED_STR("probe"),
        SEED_PUBKEY(ctx->payer->key),
    };

    uint8_t bump;
    ASSERT_PDA(ctx->target, seeds, 2, params->program_id, &bump);
    (void)args;

    return SUCCESS;
}

static uint64_t
cpis(with_args_accounts_t *ctx, Parameters *params, probe_args_t *args) {
    AccountInfo *accts = params->accounts;
    int n              = (int)params->accounts_len;

    SignerSeed seeds[] = {
        SEED_STR("probe"),
        SEED_PUBKEY(ctx->payer->key),
        SEED_U8(&args->bump),
        SEED_BYTES(&args->amount, 8),
    };
    SignerSeeds signer = {.seeds = seeds, .len = 4};

    AccountMeta metas[] = {
        meta_writable_signer(ctx->payer->key),
        meta_writable(ctx->target->key),
        meta_readonly_signer(ctx->payer->key),
        meta_readonly(ctx->mint->key),
    };
    uint8_t data[] = {1, 2, 3};

    CPI_INVOKE(&SYSTEM_PROGRAM_ID, metas, 4, data, sizeof(data), accts, n);
    CPI_INVOKE_SIGNED(
        params->program_id, metas, 2, data, 1, accts, n, &signer, 1
    );

    Instruction ix = {
        .program_id   = (Pubkey *)&SYSTEM_PROGRAM_ID,
        .accounts     = metas,
        .accounts_len = 1,
        .data         = data,
        .data_len     = 1,
    };

    TRY(invoke(&ix, accts, n));
    TRY(invoke_signed(&ix, accts, n, &signer, 1));

#ifndef NO_SYSTEM
    TRY(system_transfer(ctx->payer, ctx->target, args->amount, accts, n));
    TRY(
        system_transfer_signed(ctx->target, ctx->payer, 1, accts, n, &signer, 1)
    );
    TRY(system_create_account(
        ctx->payer, ctx->target, 1, 2, params->program_id, accts, n
    ));
    TRY(system_create_account_signed(
        ctx->payer, ctx->target, 1, 2, params->program_id, accts, n, &signer, 1
    ));
    TRY(system_allocate(ctx->target, 8, accts, n));
    TRY(system_assign(ctx->target, params->program_id, accts, n));
    TRY(system_assign_signed(
        ctx->target, params->program_id, accts, n, &signer, 1
    ));
#endif

#ifndef NO_TOKEN
    AccountInfo *t = ctx->target, *m = ctx->mint, *p = ctx->payer;

    TRY(token_initialize_mint(m, t, p->key, NULL, 6, accts, n));
    TRY(token_initialize_account(t, m, p, accts, n));
    TRY(token_initialize_account_signed(t, m, p, accts, n, &signer, 1));
    TRY(token_transfer(t, t, p, 1, accts, n));
    TRY(token_transfer_signed(t, t, p, 1, accts, n, &signer, 1));
    TRY(token_mint_to(m, t, p, 1, accts, n));
    TRY(token_mint_to_signed(m, t, p, 1, accts, n, &signer, 1));
    TRY(token_burn(t, m, p, 1, accts, n));
    TRY(token_burn_signed(t, m, p, 1, accts, n, &signer, 1));
    TRY(token_close_account(t, p, p, accts, n));
    TRY(token_close_account_signed(t, p, p, accts, n, &signer, 1));
    TRY(token_approve(t, p, p, 1, accts, n));
    TRY(token_approve_signed(t, p, p, 1, accts, n, &signer, 1));
    TRY(token_revoke(t, p, accts, n));
    TRY(token_revoke_signed(t, p, accts, n, &signer, 1));
    TRY(
        token_set_authority(t, p, TOKEN_AUTHORITY_ACCOUNT_OWNER, NULL, accts, n)
    );
    TRY(token_set_authority_signed(
        m, p, TOKEN_AUTHORITY_MINT_TOKENS, p->key, accts, n, &signer, 1
    ));
    TRY(token_freeze_account(t, m, p, accts, n));
    TRY(token_freeze_account_signed(t, m, p, accts, n, &signer, 1));
    TRY(token_thaw_account(t, m, p, accts, n));
    TRY(token_thaw_account_signed(t, m, p, accts, n, &signer, 1));
    TRY(token_sync_native(t, accts, n));
    TRY(create_associated_token_account(p, t, p, m, accts, n));
    TRY(create_associated_token_account_idempotent(p, t, p, m, accts, n));
    TRY(
        create_associated_token_account_signed(p, t, p, m, accts, n, &signer, 1)
    );
    TRY(create_associated_token_account_idempotent_signed(
        p, t, p, m, accts, n, &signer, 1
    ));
    TokenAccount *ta = TOKEN_ACCOUNT(t);
    MintAccount *ma  = MINT_ACCOUNT(m);
    if (ta->amount > ma->supply) return ERROR_CUSTOM(1);
#endif
    return SUCCESS;
}

static uint64_t
sysvars_and_pdas(with_args_accounts_t *ctx, Parameters *params) {
    Clock clock;
    TRY(get_clock(&clock));

    Rent rent;
    TRY(get_rent(&rent));

    uint64_t lamports = minimum_balance(&rent, sizeof(ProbeState));

    InstructionsSysvar sv;
    TRY(instructions_sysvar_init(&sv, ctx->mint));

    ASSERT_INSTRUCTIONS_SYSVAR(ctx->mint);
    TRY(assert_no_reentrancy(&sv, params->program_id));

    LoadedInstruction lix;
    TRY(instructions_get(&sv, instructions_current_index(&sv), &lix));

    const Pubkey *key;
    bool is_signer, is_writable;
    if (lix.accounts_len)
        loaded_ix_account(&lix, 0, &key, &is_signer, &is_writable);
    (void)instructions_count(&sv);

    SignerSeed seeds[] = {SEED_STR("probe")};
    Pubkey addr;
    uint8_t bump;
    TRY(find_program_address(seeds, 1, params->program_id, &addr, &bump));
    TRY(create_program_address(seeds, 1, params->program_id, &addr));
    derive_address(seeds, 1, params->program_id, &addr);

    ProbeState *state = ACCOUNT_STATE(ctx->target, ProbeState);
    TRY(checked_add_u64(state->value, lamports, &state->value));
    TRY(checked_sub_u64(state->value, 1, &state->value));
    TRY(checked_mul_u64(state->value, 2, &state->value));

    state->value = saturating_add_u64(state->value, saturating_sub_u64(1, 2));
    state->value = min_u64(state->value, max_u64(1, 2));

    pubkey_cpy(&state->authority, ctx->payer->key);

    if (!pubkey_eq(&state->authority, ACCOUNT_KEY(ctx->payer)))
        return ERROR_CUSTOM(2);
    if (ACCOUNT_LAMPORTS(ctx->payer) == 0 || !ACCOUNT_IS_SIGNER(ctx->payer) ||
        !ACCOUNT_IS_WRITABLE(ctx->target) ||
        !ACCOUNT_EXECUTABLE(ctx->token_program) ||
        ACCOUNT_DATA(ctx->target) == NULL ||
        ACCOUNT_DATA_LEN(ctx->target) == 0 ||
        ACCOUNT_OWNER(ctx->target) == NULL)
        return ERROR_CUSTOM(3);

    const uint8_t *p = ctx->target->data;
    uint64_t sum     = READ_U16(p) + READ_U32(p);
    sum += READ_U8(p);
    sum += READ_U64(p);

    Vec(uint64_t, 4) v = VEC_INIT;
    if (!vec_full(&v)) vec_push(&v, sum);
    if (vec_len(&v) == vec_cap(&v) - 3) sum = vec_pop(&v);
    vec_clear(&v);

    log_str("probe");
    log_literal("probe");
    log_u64(sum);
    log_64(1, 2, 3, 4, 5);
    log_pubkey(&addr);
    log_compute_units();
    debug("probe");
    debug_u64(sum);
    debug_pubkey(&addr);

#if !defined(NO_HEAP) && !defined(CUSTOM_HEAP)
    uint64_t *heap = alloc(16);
    if (!heap) return ERROR_CUSTOM(4);
    free(heap);
    heap_reset();
#endif

#if __BPF_CPU_VERSION__ != 2 /* EXIT uses lddw, which v2 removed */
    if (sum == 0xDEAD) EXIT(ERROR_INVALID_ARGUMENT);
#endif
    return SUCCESS;
}

static uint64_t
with_args(with_args_accounts_t *ctx, probe_args_t *args, Parameters *params) {
    standalone_accounts_t sa;
    TRY(standalone_parse(params, &sa));
    TRY(with_args_validate(params, ctx));
    probe_args_t copy;
    TRY(with_args_parse_args(params, &copy));
    TRY(asserts(ctx, params, args));
    TRY(cpis(ctx, params, args));
    return sysvars_and_pdas(ctx, params);
}

static uint64_t
no_args(no_args_accounts_t *ctx, no_args_args_t *args, Parameters *params) {
    (void)ctx;
    (void)args;
    if (IX_DATA_LEN(params) > 0 && *IX_DATA(params) == 9)
        return ERROR_CUSTOM(9);
    return SUCCESS;
}

#ifdef PROBE_LEGACY
static uint64_t process(Parameters *params) {
    DISPATCH(params, HANDLER(with_args) HANDLER(no_args));
    return ERROR_UNKNOWN_INSTRUCTION;
}

LEGACY_ENTRYPOINT(process);
#else
ENTRYPOINT(HANDLER(with_args) HANDLER(no_args))
#endif
