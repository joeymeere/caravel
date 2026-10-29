#ifndef CARAVEL_TEST_LITESVM_SPL_H
#define CARAVEL_TEST_LITESVM_SPL_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

/** Create and initialize a mint at `mint`'s address */
_CVLT_API void svm_create_mint(
    Svm *svm,
    const Keypair *payer,
    const Keypair *mint,
    const Pubkey *authority,
    uint8_t decimals
) {
    Ix both[2];
    _cvlt_create_mint_ixs(
        both, &payer->pubkey, &mint->pubkey, authority, decimals,
        svm_rent_exempt(svm, _CVLT_MINT_LEN)
    );
    TxResult r = SEND_ALL(svm, both, payer, mint);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("svm_create_mint failed");
    }
}

/** Create the associated token account for (owner, mint) */
_CVLT_API Pubkey svm_create_ata(
    Svm *svm, const Keypair *payer, const Pubkey *owner, const Pubkey *mint
) {
    Pubkey ata = ata_address(owner, mint);
    Ix ix      = _cvlt_create_ata_ix(&payer->pubkey, &ata, owner, mint);
    TxResult r = SEND(svm, ix, payer);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("svm_create_ata failed");
    }
    return ata;
}

/** Mint `amount` to `destination` */
_CVLT_API void svm_mint_to(
    Svm *svm,
    const Pubkey *mint,
    const Pubkey *destination,
    const Keypair *authority,
    uint64_t amount
) {
    Ix ix = _cvlt_mint_to_ix(mint, destination, &authority->pubkey, amount);
    TxResult r = SEND(svm, ix, authority);
    if (!r.ok) {
        _cvlt_print_logs(r.logs);
        FAIL("svm_mint_to failed");
    }
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_LITESVM_SPL_H */
