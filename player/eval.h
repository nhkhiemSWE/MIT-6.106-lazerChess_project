// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#ifndef EVAL_H
#define EVAL_H

#include <stdbool.h>

#include "move_gen.h"
#include "search.h"

#define EV_SCORE_RATIO 100  // Ratio of ev_score_t values to score_t values

// ev_score_t values
#define PAWN_EV_VALUE (PAWN_VALUE * EV_SCORE_RATIO)

#define MAX_COVERAGE (uint8_t)255

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

// shorter version of PAWN_EV_VALUE to fit in table
#define P_EV_VAL PAWN_EV_VALUE

score_t eval(position_t* p, bool verbose);

// test routine for p_touch()
void test_ptouch(position_t* p);

#endif  // EVAL_H
