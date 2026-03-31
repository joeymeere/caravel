#ifndef COUNTER_STATE_H
#define COUNTER_STATE_H

#include <caravel.h>

STATE(CounterState)
typedef struct {
    uint64_t count;
    Pubkey authority;
} CounterState;

#endif
