#ifndef VAULT_STATE_H
#define VAULT_STATE_H

#include <caravel.h>

typedef struct {
    CvlPubkey authority;
    uint8_t   bump;
} VaultState;

#endif
