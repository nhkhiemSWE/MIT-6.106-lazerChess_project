// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>

#include "move_gen.h"
#include "simple_mutex.h"

// score_t values
#define INF 32700
#define WIN 32000
#define PAWN_VALUE 100

// Enable all optimization tables.
#define ENABLE_TABLES true

// the maximum possible value for score_t type
#define MAX_SCORE_VAL INT16_MAX

// Threefold repetition rule
#define DRAW_NUM_REPS 3

// Killer move table
//
// https://www.chessprogramming.org/Killer_Move
// https://www.chessprogramming.org/Killer_Heuristic
//
// FORMAT: killer[ply][id]
#define __KMT_dim__ [MAX_PLY_IN_SEARCH * 4]  // NOLINT(whitespace/braces)
#define KMT(ply, id) (4 * ply + id)

// Best move history table and lookup function
//
// https://www.chessprogramming.org/History_Heuristic
//
// FORMAT: best_move_history[color_t][piece_t][square_t][orientation]
#define __BMH_dim__ [2 * 6 * ARR_SIZE * NUM_ORI]  // NOLINT(whitespace/braces)
#define BMH(color, piece, square, ori) \
  (color * 6 * ARR_SIZE * NUM_ORI + piece * ARR_SIZE * NUM_ORI + square * NUM_ORI + ori)

typedef int16_t score_t;  // Search uses "low res" values

// Main search routines and helper functions
typedef enum searchType {  // different types of search
  SEARCH_ROOT,
  SEARCH_PV,
  SEARCH_SCOUT
} searchType_t;

typedef struct searchNode {
  struct searchNode* parent;
  searchType_t type;
  score_t orig_alpha;
  score_t alpha;
  score_t beta;
  int depth;
  int ply;
  color_t fake_color_to_move;
  int quiescence;
  int pov;
  int legal_move_count;
  bool abort;
  score_t best_score;
  int best_move_index;
  position_t position;
  move_t subpv[MAX_PLY_IN_SEARCH];
  move_t* killer;
  int* best_move_history;
} searchNode;

void init_tics();
void init_abort_timer(double goal_time);
double elapsed_time();
bool should_abort();
void reset_abort();
// void init_best_move_history();
bool is_draw(position_t* p);
move_t get_move(sortable_move_t sortable_mv);

typedef struct bestMove {
  int score;
  move_t move;
  bool has_been_set;
  simple_mutex_t mutex;
} bestMove;

score_t searchRoot(position_t* p, score_t alpha, score_t beta, int depth, int ply, bestMove* pv,
                   uint64_t* node_count_serial, FILE* OUT, int thread, sortable_move_t move_list[MAX_NUM_MOVES], move_t* killer, int* best_move_history);

typedef enum { MOVE_EVALUATED, MOVE_ILLEGAL, MOVE_IGNORE, MOVE_GAMEOVER } moveEvaluationResult_t;

typedef struct moveEvaluationResult {
  score_t score;
  moveEvaluationResult_t type;
  searchNode next_node;
} moveEvaluationResult;

typedef struct leafEvalResult {
  score_t score;
  moveEvaluationResult_t type;
  bool should_enter_quiescence;
  move_t hash_table_move;
} leafEvalResult;

#endif  // SEARCH_H
