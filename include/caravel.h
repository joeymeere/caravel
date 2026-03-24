/**
 * @brief Caravel SDK
 *
 * Compilation:
 *   - Define CVL_DEBUG before including to enable debug logging
 *   - Define CVL_NO_TOKEN to exclude SPL Token helpers
 *   - Define CVL_NO_SYSTEM to exclude System program helpers
 */

#ifndef CARAVEL_H
#define CARAVEL_H

#include "caravel/types.h"
#include "caravel/error.h"

#include "caravel/syscall.h"
#include "caravel/sysvar.h"

#include "caravel/log.h"

#include "caravel/math.h"

#include "caravel/entrypoint.h"
#include "caravel/instruction.h"

#include "caravel/account.h"
#include "caravel/cpi.h"
#include "caravel/pda.h"

#ifndef CVL_NO_HEAP
#include "caravel/heap.h"
#endif

#ifndef CVL_NO_SYSTEM
#include "caravel/system.h"
#endif

#ifndef CVL_NO_TOKEN
#include "caravel/token.h"
#endif

#endif /* CARAVEL_H */
