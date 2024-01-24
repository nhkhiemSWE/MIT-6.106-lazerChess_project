// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

// Configurable options for passing via UCI interface.
// These options are used to tune the AI and decide whether or not
// your AI will use some of the builtin techniques we implemented.
// Refer to the Google Doc mentioned in the handout for understanding
// the terminology.

#ifndef OPTIONS_H
#define OPTIONS_H

#include "eval.h"
#define MAX_HASH 4096  // 4 GB

// Options for UCI interface
// flag whether to use opening book or not (defined in lookup.h)
int USE_OB;

// defined in search.c
extern int DRAW;
extern int LMR_R1;
extern int LMR_R2;
extern int HMB;
extern int USE_NMM;
extern int FUT_DEPTH;
extern int TRACE_MOVES;
extern int DETECT_DRAWS;
extern int NMOVES_DRAW;

// defined in eval.c
extern int RANDOMIZE;
extern int PTOUCH_weight;
extern int PPROX_weight;
extern int MFACE_weight;
extern int MCEDE_weight;
extern int LCOVERAGE_weight;
extern int MMID_weight;
extern int PMID_weight;
extern int RELQI_weight;
extern int ABSQI_weight;

// defined in tt.c
extern int USE_TT;
extern int HASH;

// flag that can be set via uci setoption command that will reset the rng to
// default
//   seeds. This is useful for running benchmarks for changes that only impact
//   performance.
extern int RESET_RNG;

// struct for manipulating options below
typedef struct {
  char name[MAX_CHARS_IN_TOKEN];  // name of options
  int* var;    // pointer to an int variable holding its value
  int dfault;  // default value
  int min;     // lower bound on what we want it to be
  int max;     // upper bound
} int_options;

#define DEFAULT_MIN -5 * P_EV_VAL
#define DEFAULT_MAX 5 * P_EV_VAL

// tensor([ 0.1029,  0.2231,  0.4186,  0.1204,  0.0175, -0.1234,
// -0.1227,  1.0000, 1.2006,  0.0106], grad_fn=<DivBackward0>)
static int_options iopts[] = {
    // {name, variable, default, min, max}
    // --------------------------------------------------------------------------
    {"ptouch", &PTOUCH_weight, (int)(0.1029 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"pprox", &PPROX_weight, (int)(0.2231 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"mface", &MFACE_weight, (int)(0.4186 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"mcede", &MCEDE_weight, (int)(0.1204 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"lcoverage", &LCOVERAGE_weight, (int)(0.0175 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"pmid", &PMID_weight, (int)(-0.1234 * P_EV_VAL), DEFAULT_MIN, DEFAULT_MAX},
    {"mmid", &MMID_weight, (int)(-0.1227 * P_EV_VAL), DEFAULT_MIN, DEFAULT_MAX},
    {"relqi", &RELQI_weight, (int)(1.2006 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"absqi", &ABSQI_weight, (int)(0.0106 * P_EV_VAL), DEFAULT_MIN,
     DEFAULT_MAX},
    {"hash", &HASH, 1040, 1, MAX_HASH},
    {"draw", &DRAW, (int)(-0.0016 * PAWN_VALUE), -PAWN_VALUE, PAWN_VALUE},
    {"randomize", &RANDOMIZE, 0, 0, P_EV_VAL},
    {"reset_rng", &RESET_RNG, 0, 0, 1},
    {"lmr_r1", &LMR_R1, 10, 1, MAX_NUM_MOVES},
    {"lmr_r2", &LMR_R2, 20, 1, MAX_NUM_MOVES},
    {"hmb", &HMB, (int)(0.0027 * PAWN_VALUE), 0, PAWN_VALUE},
    {"fut_depth", &FUT_DEPTH, 3, 0, 5},
    // debug options
    {"use_nmm", &USE_NMM, 1, 0, 1},
    {"detect_draws", &DETECT_DRAWS, 1, 0, 1},
    {"use_tt", &USE_TT, 1, 0, 1},
    {"use_ob", &USE_OB, 1, 0, 1},
    {"trace_moves", &TRACE_MOVES, 0, 0, 1},
    {"nmoves_draw", &NMOVES_DRAW, 100, 1, 1000 * 1000},
    {"", NULL, 0, 0, 0}};

#endif  // OPTIONS_H
