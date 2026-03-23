#include "pool.h"

static uint64_t process(CvlParameters *params) {
    CVL_DISPATCH(params,
        CVL_INSTRUCTION(0, handle_initialize_pool)
        CVL_INSTRUCTION(1, handle_add_liquidity)
        CVL_INSTRUCTION(2, handle_remove_liquidity)
        CVL_INSTRUCTION(3, handle_swap)
    );
    return CVL_ERROR_UNKNOWN_INSTRUCTION;
}

CVL_ENTRYPOINT(process);
