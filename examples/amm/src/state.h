#ifndef AMM_STATE_H
#define AMM_STATE_H

#include <caravel.h>

typedef struct {
    uint8_t   is_initialized;  /* 0      */
    uint8_t   bump;            /* 1      */
    uint8_t   _padding[6];     /* 2..7   */
    CvlPubkey mint_a;          /* 8..39  */
    CvlPubkey mint_b;          /* 40..71 */
    CvlPubkey vault_a;         /* 72..103 */
    CvlPubkey vault_b;         /* 104..135 */
    CvlPubkey lp_mint;         /* 136..167 */
} PoolState;

#define AMM_ERROR_ALREADY_INITIALIZED  CVL_ERROR_CUSTOM(10)
#define AMM_ERROR_NOT_INITIALIZED      CVL_ERROR_CUSTOM(11)
#define AMM_ERROR_INVALID_VAULT_OWNER  CVL_ERROR_CUSTOM(12)
#define AMM_ERROR_INVALID_LP_AUTHORITY CVL_ERROR_CUSTOM(13)
#define AMM_ERROR_SLIPPAGE_EXCEEDED    CVL_ERROR_CUSTOM(14)
#define AMM_ERROR_ZERO_AMOUNT          CVL_ERROR_CUSTOM(15)
#define AMM_ERROR_INVALID_DIRECTION    CVL_ERROR_CUSTOM(16)
#define AMM_ERROR_INSUFFICIENT_LP      CVL_ERROR_CUSTOM(17)

#endif /* AMM_STATE_H */
