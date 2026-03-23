#ifndef TOKEN_VAULT_STATE_H
#define TOKEN_VAULT_STATE_H

#include <caravel.h>

typedef struct {
    CvlPubkey authority;
    CvlPubkey mint;
    uint8_t   bump;
} TokenVaultState;

#endif
