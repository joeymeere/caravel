#ifndef AMM_POOL_H
#define AMM_POOL_H

#include <caravel.h>
#include "state.h"
#include "math.h"

#define INIT_POOL_ACCOUNTS(X) \
    X(payer,          SIGNER | WRITABLE) \
    X(pool_state,     WRITABLE) \
    X(vault_a,        0) \
    X(vault_b,        0) \
    X(lp_mint,        0) \
    X(mint_a,         0) \
    X(mint_b,         0) \
    X(system_program, PROGRAM)

IX(0, initialize_pool, INIT_POOL_ACCOUNTS)

typedef struct __attribute__((packed)) {
    uint64_t amount_a;
    uint64_t amount_b;
    uint64_t min_lp;
} add_liquidity_args_t;

#define LIQUIDITY_ACCOUNTS(X) \
    X(user,          SIGNER) \
    X(pool_state,    0) \
    X(vault_a,       WRITABLE) \
    X(vault_b,       WRITABLE) \
    X(user_token_a,  WRITABLE) \
    X(user_token_b,  WRITABLE) \
    X(lp_mint,       WRITABLE) \
    X(user_lp,       WRITABLE) \
    X(token_program, PROGRAM)

IX(1, add_liquidity, LIQUIDITY_ACCOUNTS, add_liquidity_args_t)

typedef struct __attribute__((packed)) {
    uint64_t lp_amount;
    uint64_t min_a;
    uint64_t min_b;
} remove_liquidity_args_t;

IX(2, remove_liquidity, LIQUIDITY_ACCOUNTS, remove_liquidity_args_t)

typedef struct __attribute__((packed)) {
    uint64_t amount_in;
    uint64_t min_amount_out;
    uint8_t  direction;
} swap_args_t;

#define SWAP_ACCOUNTS(X) \
    X(user,          SIGNER) \
    X(pool_state,    0) \
    X(vault_a,       WRITABLE) \
    X(vault_b,       WRITABLE) \
    X(user_token_a,  WRITABLE) \
    X(user_token_b,  WRITABLE) \
    X(token_program, PROGRAM)

IX(3, swap, SWAP_ACCOUNTS, swap_args_t)

uint64_t initialize_pool(initialize_pool_accounts_t *, initialize_pool_args_t *, Parameters *);
uint64_t add_liquidity(add_liquidity_accounts_t *, add_liquidity_args_t *, Parameters *);
uint64_t remove_liquidity(remove_liquidity_accounts_t *, remove_liquidity_args_t *, Parameters *);
uint64_t swap(swap_accounts_t *, swap_args_t *, Parameters *);

#endif /* AMM_POOL_H */
