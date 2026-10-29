#ifndef CARAVEL_TEST_SPL_H
#define CARAVEL_TEST_SPL_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

#define _CVLT_MINT_LEN 82

_CVLT_API Pubkey ata_address(const Pubkey *owner, const Pubkey *mint) {
    return FIND_PDA(
        &ASSOCIATED_TOKEN_PROGRAM_ID, NULL, SEED_PUBKEY(owner),
        SEED_PUBKEY(&TOKEN_PROGRAM_ID), SEED_PUBKEY(mint)
    );
}

_CVLT_API void _cvlt_create_mint_ixs(
    Ix out[2],
    const Pubkey *payer,
    const Pubkey *mint,
    const Pubkey *authority,
    uint8_t decimals,
    uint64_t lamports
) {
    out[0] = ix_new(&SYSTEM_PROGRAM_ID);
    ix_account(&out[0], payer, true, true);
    ix_account(&out[0], mint, true, true);
    uint8_t d[52]  = {0}; /* CreateAccount: u32 tag, lamports, space, owner */
    uint64_t space = _CVLT_MINT_LEN;
    memcpy(d + 4, &lamports, 8);
    memcpy(d + 12, &space, 8);
    memcpy(d + 20, TOKEN_PROGRAM_ID.bytes, 32);
    ix_data(&out[0], d, sizeof d);

    out[1] = ix_new(&TOKEN_PROGRAM_ID);
    ix_account(&out[1], mint, false, true);
    ix_account(&out[1], &RENT_SYSVAR_ID, false, false);
    uint8_t i[67] = {0}; /* InitializeMint: tag 0, decimals, authority */
    i[1]          = decimals;
    memcpy(i + 2, authority->bytes, 32);
    ix_data(&out[1], i, sizeof i);
}

_CVLT_API Ix _cvlt_create_ata_ix(
    const Pubkey *payer,
    const Pubkey *ata,
    const Pubkey *owner,
    const Pubkey *mint
) {
    Ix ix = ix_new(&ASSOCIATED_TOKEN_PROGRAM_ID);
    ix_account(&ix, payer, true, true);
    ix_account(&ix, ata, false, true);
    ix_account(&ix, owner, false, false);
    ix_account(&ix, mint, false, false);
    ix_account(&ix, &SYSTEM_PROGRAM_ID, false, false);
    ix_account(&ix, &TOKEN_PROGRAM_ID, false, false);
    return ix;
}

_CVLT_API Ix _cvlt_mint_to_ix(
    const Pubkey *mint,
    const Pubkey *destination,
    const Pubkey *authority,
    uint64_t amount
) {
    Ix ix = ix_new(&TOKEN_PROGRAM_ID);
    ix_account(&ix, mint, false, true);
    ix_account(&ix, destination, false, true);
    ix_account(&ix, authority, true, false);
    ix_u8(&ix, 7); /* MintTo */
    ix_u64(&ix, amount);
    return ix;
}

#define TOKEN_BALANCE(vm, account) \
    (READ_ACCOUNT((vm), (account), TokenAccount).amount)

#define MINT_SUPPLY(vm, mint) (READ_ACCOUNT((vm), (mint), MintAccount).supply)

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_SPL_H */
