#ifndef COUNTER_INSTRUCTIONS_H
#define COUNTER_INSTRUCTIONS_H

#include <caravel.h>

#define INIT_ACCOUNTS(X) \
    X(payer, SIGNER | WRITABLE) \
    X(counter, SIGNER | WRITABLE) \
    X(system_program, PROGRAM)

IX(0, initialize, INIT_ACCOUNTS)

#define INC_ACCOUNTS(X) \
    X(counter, WRITABLE) \
    X(authority, SIGNER)

IX(1, increment, INC_ACCOUNTS)

#define DEC_ACCOUNTS(X) \
    X(counter, WRITABLE) \
    X(authority, SIGNER)

IX(2, decrement, DEC_ACCOUNTS)

#endif
