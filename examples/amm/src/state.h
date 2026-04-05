#ifndef AMM_STATE_H
#define AMM_STATE_H

#include <caravel.h>

STATE(PoolState)
typedef struct {
    uint8_t   is_initialized;  /* 0      */
    uint8_t   bump;            /* 1      */
    uint8_t   _padding[6];     /* 2..7   */
    Pubkey mint_a;          /* 8..39  */
    Pubkey mint_b;          /* 40..71 */
    Pubkey vault_a;         /* 72..103 */
    Pubkey vault_b;         /* 104..135 */
    Pubkey lp_mint;         /* 136..167 */
} PoolState;

#define AMM_ERROR_ALREADY_INITIALIZED  ERROR_CUSTOM(10)
#define AMM_ERROR_NOT_INITIALIZED      ERROR_CUSTOM(11)
#define AMM_ERROR_INVALID_VAULT_OWNER  ERROR_CUSTOM(12)
#define AMM_ERROR_INVALID_LP_AUTHORITY ERROR_CUSTOM(13)
#define AMM_ERROR_SLIPPAGE_EXCEEDED    ERROR_CUSTOM(14)
#define AMM_ERROR_ZERO_AMOUNT          ERROR_CUSTOM(15)
#define AMM_ERROR_INVALID_DIRECTION    ERROR_CUSTOM(16)
#define AMM_ERROR_INSUFFICIENT_LP      ERROR_CUSTOM(17)

#endif /* AMM_STATE_H */
