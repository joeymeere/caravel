#ifndef VAULT_STATE_H
#define VAULT_STATE_H

#include <caravel.h>

typedef struct {
    Pubkey authority;
    uint8_t   bump;
} VaultState;

#endif
