#include <caravel.h>

/*
 * Instruction data layout:
 *   deposit:  disc(1) + amount(8) + bump(1) = 10 bytes
 *   withdraw: disc(1) + amount(8) + bump(1) = 10 bytes
 */

/* @cvl:instruction deposit 0 */
/* @cvl:args amount:u64 bump:u8 */

#define DEPOSIT_ACCOUNTS(X) \
    X(user,           CVL_SIGNER | CVL_WRITABLE) \
    X(vault,          CVL_WRITABLE) \
    X(system_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(deposit, DEPOSIT_ACCOUNTS)

static uint64_t handle_deposit(CvlParameters *params) {
    deposit_accounts_t ctx;
    CVL_TRY(deposit_parse(params, &ctx));

    if (CVL_UNLIKELY(params->data_len < 10)) {
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount = *(uint64_t *)(params->data + 1);
    uint8_t bump = params->data[9];

    CvlSignerSeed seeds[] = {
        CVL_SEED_STR("vault"),
        CVL_SEED_PUBKEY(ctx.user->key),
        CVL_SEED_U8(&bump),
    };
    CvlPubkey expected;
    cvl_derive_address(seeds, 3, params->program_id, &expected);
    if (CVL_UNLIKELY(!cvl_pubkey_eq(ctx.vault->key, &expected))) {
        return CVL_ERROR_INVALID_PDA;
    }

    CVL_TRY(cvl_system_transfer(
        ctx.user, ctx.vault, amount,
        params->accounts, (int)params->accounts_len
    ));

    return CVL_SUCCESS;
}

/* @cvl:instruction withdraw 1 */
/* @cvl:args amount:u64 bump:u8 */

#define WITHDRAW_ACCOUNTS(X) \
    X(user,  CVL_SIGNER | CVL_WRITABLE) \
    X(vault, CVL_WRITABLE)

CVL_DEFINE_ACCOUNTS(withdraw, WITHDRAW_ACCOUNTS)

static uint64_t handle_withdraw(CvlParameters *params) {
    withdraw_accounts_t ctx;
    CVL_TRY(withdraw_parse(params, &ctx));

    if (CVL_UNLIKELY(params->data_len < 10)) {
        return CVL_ERROR_INVALID_INSTRUCTION_DATA;
    }
    uint64_t amount = *(uint64_t *)(params->data + 1);
    uint8_t bump = params->data[9];

    CvlSignerSeed seeds[] = {
        CVL_SEED_STR("vault"),
        CVL_SEED_PUBKEY(ctx.user->key),
        CVL_SEED_U8(&bump),
    };
    CvlPubkey expected;
    cvl_derive_address(seeds, 3, params->program_id, &expected);
    if (CVL_UNLIKELY(!cvl_pubkey_eq(ctx.vault->key, &expected))) {
        return CVL_ERROR_INVALID_PDA;
    }

    *ctx.vault->lamports -= amount;
    *ctx.user->lamports += amount;

    return CVL_SUCCESS;
}

static uint64_t process(CvlParameters *params) {
    CVL_DISPATCH(params,
        CVL_INSTRUCTION(0, handle_deposit)
        CVL_INSTRUCTION(1, handle_withdraw)
    );
    return CVL_ERROR_UNKNOWN_INSTRUCTION;
}

CVL_ENTRYPOINT(process);
