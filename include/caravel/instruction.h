#ifndef CVL_INSTRUCTION_H
#define CVL_INSTRUCTION_H

#include "types.h"
#include "error.h"

/**
 * Get the instruction discriminator (first byte of instruction data).
 */
#define CVL_INSTRUCTION_DISCRIMINATOR(params) ((params)->data[0])

/**
 * Define a single instruction case in a dispatch table.
 * Used inside CVL_DISPATCH.
 *
 * handler signature: uint64_t handler(CvlParameters *params)
 */
#define CVL_INSTRUCTION(disc, handler) \
    case (disc): return handler(params);

/**
 * Dispatch to instruction handlers based on the discriminator byte.
 *
 * Usage:
 *   CVL_DISPATCH(params,
 *       CVL_INSTRUCTION(0, handle_initialize)
 *       CVL_INSTRUCTION(1, handle_increment)
 *       CVL_INSTRUCTION(2, handle_decrement)
 *   );
 *
 * Returns CVL_ERROR_UNKNOWN_INSTRUCTION for unrecognized discriminators.
 */
#define CVL_DISPATCH(params, ...) \
    do { \
        if ((params)->data_len == 0) return CVL_ERROR_INVALID_INSTRUCTION_DATA; \
        uint8_t _cvl_disc = CVL_INSTRUCTION_DISCRIMINATOR(params); \
        switch (_cvl_disc) { \
            __VA_ARGS__ \
            default: return CVL_ERROR_UNKNOWN_INSTRUCTION; \
        } \
    } while (0)

/**
 * Get pointer to instruction data after the discriminator byte.
 */
#define CVL_IX_DATA(params)     ((params)->data + 1)

/**
 * Get length of instruction data after the discriminator byte.
 */
#define CVL_IX_DATA_LEN(params) ((params)->data_len - 1)

/**
 * Cast instruction data (after discriminator) to a typed struct pointer.
 * Usage: MyIxData *data = CVL_IX_DATA_AS(params, MyIxData);
 */
#define CVL_IX_DATA_AS(params, type) ((type *)CVL_IX_DATA(params))

#endif /* CVL_INSTRUCTION_H */
