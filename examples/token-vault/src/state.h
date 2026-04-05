#ifndef TOKEN_VAULT_STATE_H
#define TOKEN_VAULT_STATE_H

#include <caravel.h>

STATE(TokenVaultState)
typedef struct {
    Pubkey authority;
    Pubkey mint;
    uint8_t   bump;
} TokenVaultState;

#endif
