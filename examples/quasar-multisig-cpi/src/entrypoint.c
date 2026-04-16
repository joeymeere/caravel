#include <caravel.h>
#include "client.h"

#define PROXY_CREATE_ACCOUNTS(X) \
    X(creator,          SIGNER | WRITABLE) \
    X(config,           WRITABLE) \
    X(rent_sysvar,      0) \
    X(system_program,   PROGRAM) \
    X(multisig_program, PROGRAM)

typedef struct __attribute__((packed)) {
    uint8_t threshold;
} proxy_create_args_t;

IX(0, proxy_create, PROXY_CREATE_ACCOUNTS, proxy_create_args_t)

static uint64_t proxy_create(
    proxy_create_accounts_t *ctx, proxy_create_args_t *args, Parameters *params
) {
    quasar_multisig_create_accounts_t cpi_accts = {
        .creator       = ctx->creator->key,
        .rent          = ctx->rent_sysvar->key,
        .systemProgram = ctx->system_program->key,
    };
    quasar_multisig_create_args_t cpi_args = { .threshold = args->threshold };

    uint64_t num_signers = params->accounts_len > 5 ? params->accounts_len - 5 : 0;
    AccountMeta remaining[8];
    for (uint64_t i = 0; i < num_signers; i++)
        remaining[i] = meta_readonly_signer(params->accounts[5 + i].key);

    Instruction ix;
    AccountMeta meta[12];
    uint8_t data[16];
    if (!quasar_multisig_create_ix(
            &cpi_accts, &cpi_args, remaining, num_signers,
            &ix, meta, data, sizeof(data)))
        return ERROR_INVALID_INSTRUCTION_DATA;

    return invoke(&ix, params->accounts, (int)params->accounts_len);
}

#define PROXY_DEPOSIT_ACCOUNTS(X) \
    X(depositor,        SIGNER | WRITABLE) \
    X(config,           0) \
    X(vault,            WRITABLE) \
    X(system_program,   PROGRAM) \
    X(multisig_program, PROGRAM)

typedef struct __attribute__((packed)) {
    uint64_t amount;
} proxy_deposit_args_t;

IX(1, proxy_deposit, PROXY_DEPOSIT_ACCOUNTS, proxy_deposit_args_t)

static uint64_t proxy_deposit(
    proxy_deposit_accounts_t *ctx, proxy_deposit_args_t *args, Parameters *params
) {
    quasar_multisig_deposit_accounts_t cpi_accts = {
        .depositor     = ctx->depositor->key,
        .config        = ctx->config->key,
        .systemProgram = ctx->system_program->key,
    };
    quasar_multisig_deposit_args_t cpi_args = { .amount = args->amount };

    Instruction ix;
    AccountMeta meta[4];
    uint8_t data[16];
    if (!quasar_multisig_deposit_ix(
            &cpi_accts, &cpi_args, &ix, meta, data, sizeof(data)))
        return ERROR_INVALID_INSTRUCTION_DATA;

    return invoke(&ix, params->accounts, (int)params->accounts_len);
}

#define PROXY_EXEC_TRANSFER_ACCOUNTS(X) \
    X(creator,          SIGNER) \
    X(config,           0) \
    X(vault,            WRITABLE) \
    X(recipient,        WRITABLE) \
    X(system_program,   PROGRAM) \
    X(multisig_program, PROGRAM)

typedef struct __attribute__((packed)) {
    uint64_t amount;
} proxy_exec_transfer_args_t;

IX(2, proxy_exec_transfer, PROXY_EXEC_TRANSFER_ACCOUNTS, proxy_exec_transfer_args_t)

static uint64_t proxy_exec_transfer(
    proxy_exec_transfer_accounts_t *ctx, proxy_exec_transfer_args_t *args, Parameters *params
) {
    quasar_multisig_execute_transfer_accounts_t cpi_accts = {
        .creator       = ctx->creator->key,
        .recipient     = ctx->recipient->key,
        .systemProgram = ctx->system_program->key,
    };
    quasar_multisig_execute_transfer_args_t cpi_args = { .amount = args->amount };

    uint64_t num_signers = params->accounts_len > 6 ? params->accounts_len - 6 : 0;
    AccountMeta remaining[8];
    for (uint64_t i = 0; i < num_signers; i++)
        remaining[i] = meta_readonly_signer(params->accounts[6 + i].key);

    Instruction ix;
    AccountMeta meta[13];
    uint8_t data[16];
    if (!quasar_multisig_execute_transfer_ix(
            &cpi_accts, &cpi_args, remaining, num_signers,
            &ix, meta, data, sizeof(data)))
        return ERROR_INVALID_INSTRUCTION_DATA;

    return invoke(&ix, params->accounts, (int)params->accounts_len);
}

ENTRYPOINT(
    HANDLER(proxy_create)
    HANDLER(proxy_deposit)
    HANDLER(proxy_exec_transfer)
)
