#ifndef CARAVEL_TEST_SEASHELL_SPL_H
#define CARAVEL_TEST_SEASHELL_SPL_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

_CVLT_API void _cvlt_spl_step(Shell *shell, Ix *ix, const char *what) {
    IxResult r = PROCESS(shell, *ix);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("%s failed", what);
    }
}

/** Create and initialize a mint at `mint` */
_CVLT_API void shell_create_mint(
    Shell *shell,
    const Pubkey *payer,
    const Pubkey *mint,
    const Pubkey *authority,
    uint8_t decimals
) {
    Ix ixs[2];
    _cvlt_create_mint_ixs(
        ixs, payer, mint, authority, decimals,
        shell_rent_exempt(shell, _CVLT_MINT_LEN)
    );
    _cvlt_spl_step(shell, &ixs[0], "shell_create_mint");
    _cvlt_spl_step(shell, &ixs[1], "shell_create_mint");
}

/** Create the associated token account for (owner, mint) */
_CVLT_API Pubkey shell_create_ata(
    Shell *shell, const Pubkey *payer, const Pubkey *owner, const Pubkey *mint
) {
    Pubkey ata = ata_address(owner, mint);
    Ix ix      = _cvlt_create_ata_ix(payer, &ata, owner, mint);
    _cvlt_spl_step(shell, &ix, "shell_create_ata");
    return ata;
}

/** Mint `amount` to `destination` as `authority` */
_CVLT_API void shell_mint_to(
    Shell *shell,
    const Pubkey *mint,
    const Pubkey *destination,
    const Pubkey *authority,
    uint64_t amount
) {
    Ix ix = _cvlt_mint_to_ix(mint, destination, authority, amount);
    _cvlt_spl_step(shell, &ix, "shell_mint_to");
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_SEASHELL_SPL_H */
