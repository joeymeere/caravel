#ifndef CARAVEL_TEST_MOLLUSK_SPL_H
#define CARAVEL_TEST_MOLLUSK_SPL_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** Create and initialize a mint at `mint` */
_CVLT_API void ctx_create_mint(
    Ctx *ctx,
    const Pubkey *payer,
    const Pubkey *mint,
    const Pubkey *authority,
    uint8_t decimals
) {
    Ix both[2];
    _cvlt_create_mint_ixs(
        both, payer, mint, authority, decimals,
        ctx_rent_exempt(ctx, _CVLT_MINT_LEN)
    );
    IxResult r = PROCESS_TX(ctx, both, payer);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("ctx_create_mint failed");
    }
}

/** Create the associated token account for (owner, mint) */
_CVLT_API Pubkey ctx_create_ata(
    Ctx *ctx, const Pubkey *payer, const Pubkey *owner, const Pubkey *mint
) {
    Pubkey ata = ata_address(owner, mint);
    Ix ix      = _cvlt_create_ata_ix(payer, &ata, owner, mint);
    IxResult r = PROCESS(ctx, ix);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("ctx_create_ata failed");
    }
    return ata;
}

/** Mint `amount` to `destination` as `authority` */
_CVLT_API void ctx_mint_to(
    Ctx *ctx,
    const Pubkey *mint,
    const Pubkey *destination,
    const Pubkey *authority,
    uint64_t amount
) {
    Ix ix      = _cvlt_mint_to_ix(mint, destination, authority, amount);
    IxResult r = PROCESS(ctx, ix);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("ctx_mint_to failed");
    }
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_MOLLUSK_SPL_H */
