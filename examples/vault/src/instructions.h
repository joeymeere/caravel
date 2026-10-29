#ifndef VAULT_INSTRUCTIONS_H
#define VAULT_INSTRUCTIONS_H

#include <caravel.h>

typedef struct __attribute__((packed)) {
    uint64_t amount;
} deposit_args_t;

#define DEPOSIT_ACCOUNTS(X) \
    X(user, SIGNER | WRITABLE) \
    X(vault, WRITABLE) \
    X(system_program, PROGRAM)

IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)

typedef struct __attribute__((packed)) {
    uint64_t amount;
} withdraw_args_t;

#define WITHDRAW_ACCOUNTS(X) \
    X(user, SIGNER | WRITABLE) \
    X(vault, WRITABLE)

IX(1, withdraw, WITHDRAW_ACCOUNTS, withdraw_args_t)

#endif
