#ifndef COUNTER_STATE_H
#define COUNTER_STATE_H

#include <caravel.h>

typedef struct {
    uint64_t count;
    CvlPubkey authority;
} CounterState;

#endif
