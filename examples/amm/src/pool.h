#ifndef AMM_POOL_H
#define AMM_POOL_H

#include <caravel.h>
#include "state.h"
#include "math.h"

uint64_t handle_initialize_pool(CvlParameters *params);
uint64_t handle_add_liquidity(CvlParameters *params);
uint64_t handle_remove_liquidity(CvlParameters *params);
uint64_t handle_swap(CvlParameters *params);

#endif /* AMM_POOL_H */
