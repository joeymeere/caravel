/**
 * @brief Caravel SDK
 *
 * Compilation:
 *   - Define DEBUG before including to enable debug logging
 *   - Define NO_TOKEN to exclude SPL Token helpers
 *   - Define NO_SYSTEM to exclude System program helpers
 */

#ifndef CARAVEL_H
#define CARAVEL_H

#include "caravel/types.h"
#include "caravel/error.h"

#include "caravel/syscall.h"
#include "caravel/sysvar.h"

#include "caravel/log.h"

#include "caravel/math.h"
#include "caravel/util.h"

#include "caravel/entrypoint.h"
#include "caravel/instruction.h"

#include "caravel/account.h"
#include "caravel/cpi.h"
#include "caravel/pda.h"

#ifndef NO_HEAP
#include "caravel/heap.h"
#endif

#ifndef NO_SYSTEM
#include "caravel/system.h"
#endif

#ifndef NO_TOKEN
#include "caravel/token.h"
#endif

/**
 * Embed a `.s` file containing sBPF assembly into the current translation unit
 *
 * The file must define its own `.globl` / `.type` directives. Declare matching
 * `extern` prototypes in C so the linker can resolve calls to the symbols.
 *
 * @note platform-tools toolchain only
 *
 * @code
 *   INCLUDE_ASM("ops.s");
 *   extern uint64_t mul_div_u64(uint64_t a, uint64_t b, uint64_t c);
 */
#define INCLUDE_ASM(path) __asm__(".include \"" path "\"")

#endif /* CARAVEL_H */
