/**
 * @brief Test harness for Caravel programs
 *
 * @code
 * #include <caravel/test.h>
 * #include "../src/instructions.h"
 *
 * static Keypair payer, counter;
 *
 * SETUP {
 *     payer   = keypair(1);
 *     counter = keypair(2);
 *     svm_airdrop(svm, &payer.pubkey, SOL(10));
 * }
 *
 * TEST(initializes_a_counter) {
 *     Ix ix = MAKE_IX(initialize, .payer = payer.pubkey,
 *                     .counter = counter.pubkey,
 *                     .system_program = SYSTEM_PROGRAM_ID);
 *     TxResult r = SEND(svm, ix, &payer, &counter);
 *     ASSERT_OK(r);
 *     ASSERT_EQ(READ_ACCOUNT(svm, &counter.pubkey, CounterState).count, 0);
 * }
 * @endcode
 *
 * Tests in a file share a single VM, created with the program deployed at
 * PROGRAM_ID, and run in file order after SETUP
 *
 * Backends: define one of these before including (or pass -D);
 * `[test] backend` in Caravel.toml sets a project's default. Each
 * keeps its own library's model, so one must be cognizant that the calls that touch the VM differ
 * in behavior:
 *   CARAVEL_TEST_LITESVM   uses a bank, requires signing + fees and executes with `SEND` or `SIMULATE`
 *   CARAVEL_TEST_MOLLUSK   bankless, no signing or fees and executes with `PROCESS`, `PROCESS_CHAIN`, `PROCESS_TX`
 *   CARAVEL_TEST_SEASHELL  uses local and recorded mainnet accounts, no signing or fees and executes with `PROCESS`, `SCENARIO`
 * Ix building, ASSERT_*, READ_ACCOUNT and report_cu() work on any of them.
 */

#ifndef CARAVEL_TEST_H
#define CARAVEL_TEST_H

#ifndef CARAVEL_TEST
#define CARAVEL_TEST
#endif
#ifndef NO_HEAP
#define NO_HEAP /* alloc/free from heap.h collides with libc */
#endif

#if defined(CARAVEL_TEST_LITESVM) + defined(CARAVEL_TEST_MOLLUSK) + \
        defined(CARAVEL_TEST_SEASHELL) > \
    1
#error "define at most one CARAVEL_TEST_<backend>"
#else
#if !defined(CARAVEL_TEST_LITESVM) && !defined(CARAVEL_TEST_MOLLUSK) && \
    !defined(CARAVEL_TEST_SEASHELL)
#define CARAVEL_TEST_LITESVM
#endif
#define _CVLT_BACKEND_SELECTED
#endif

#include "../caravel.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CARAVEL_TEST_PROGRAM
#define CARAVEL_TEST_PROGRAM "build/program.so"
#endif

#ifdef _CVLT_BACKEND_SELECTED

#include "test/tutils.h"
#include "test/ix.h"
#include "test/terr.h"
#include "test/spl.h"

#if defined(CARAVEL_TEST_LITESVM)
#include <litesvm.h>
#include "test/litesvm/vm.h"
#include "test/litesvm/tx.h"
#include "test/litesvm/svm.h"
#include "test/litesvm/spl.h"
#elif defined(CARAVEL_TEST_MOLLUSK)
#include <mollusk.h>
#include "test/mollusk/vm.h"
#include "test/mollusk/tx.h"
#include "test/mollusk/ctx.h"
#include "test/mollusk/spl.h"
#elif defined(CARAVEL_TEST_SEASHELL)
#include <seashell.h>
#include "test/seashell/vm.h"
#include "test/seashell/tx.h"
#include "test/seashell/shell.h"
#include "test/seashell/spl.h"
#endif

#include "test/runner.h"

#endif /* _CVLT_BACKEND_SELECTED */

#endif /* CARAVEL_TEST_H */
