// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

// This file is #included in search.c and is not compiled separately

#include "search.h"
#include "tt.h"

// Killer move table
//
// https://www.chessprogramming.org/Killer_Move
// https://www.chessprogramming.org/Killer_Heuristic
//
// FORMAT: killer[ply][id]
static move_t killer __KMT_dim__;  // up to 4 killers

// Best move history table and lookup function
//
// https://www.chessprogramming.org/History_Heuristic
//
// FORMAT: best_move_history[color_t][piece_t][square_t][orientation]

// static int best_move_history __BMH_dim__;

// void init_best_move_history() {
//   memset(best_move_history, 0, sizeof(best_move_history));
// }

#define MBS_SET 32768
#define MBS_WAY 4

const uint64_t entry_mask = MBS_SET-1;

volatile uint64_t moves_being_searched[MBS_SET][MBS_WAY];

static void finish_search(uint64_t pos_hash, move_t mv) {
  uint32_t move_hash = *(uint32_t*) (&mv);
  uint64_t index = (pos_hash ^ move_hash) & entry_mask;
  for (int i = 0; i < MBS_WAY; i++) {
    if (moves_being_searched[index][i] == move_hash) {
      moves_being_searched[index][i] =0;
    }
  }
}

static bool is_move_searching(uint64_t pos_hash, move_t mv) {
  uint32_t move_hash = *(uint32_t*) (&mv);
  uint64_t index = (pos_hash ^ move_hash) & entry_mask;
  for (int i = 0; i< MBS_WAY; i ++) {
    if (moves_being_searched[index][i] == move_hash) {
      return true;
    }
  }
  return false;
}

static void set_search_move(uint64_t pos_hash, move_t mv) {
  uint32_t move_hash = *(uint32_t*) (&mv);
  uint64_t index = (pos_hash ^ move_hash) & entry_mask;
  for (int i = 0; i < MBS_WAY; i ++) {
    if (moves_being_searched[index][i] == 0){
      moves_being_searched[index][i] = move_hash;
      return;
    }
  }
  moves_being_searched[index][0] = move_hash;
}

static void update_best_move_history(position_t* p, int index_of_best,
                                     sortable_move_t* lst, int count, int* best_move_history) {
  tbassert(ENABLE_TABLES, "Tables weren't enabled.\n");

  int color_to_move = color_to_move_of(p);

  for (int i = 0; i < count; i++) {
    move_t mv = get_move(lst[i]);
    ptype_t pce = mv.typ;
    rot_t ro = mv.rot;
    square_t fs = mv.from_sq;
    square_t ts = mv.to_sq;
    int ot = (ori_of(p->board[fs]) + ro) % NUM_ORI;

    int s = best_move_history[BMH(color_to_move, pce, ts, ot)];

    if (index_of_best == i) {
      s = s + 11200;  // number will never exceed 1017
    }
    s = s * 0.90;  // decay score over time

    tbassert(s < 102000, "s = %d\n", s);  // or else sorting will fail

    best_move_history[BMH(color_to_move, pce, ts, ot)] = s;
  }
}

static void update_transposition_table(searchNode* node) {
  if (node->type == SEARCH_SCOUT) {
    if (node->best_score < node->beta) {
      tt_hashtable_put(
          node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), (uint8_t) UPPER,
          NULL_MOVE);
    } else {
      tt_hashtable_put(
          node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), (uint8_t) LOWER,
          node->subpv[0]);
    }
  } else if (node->type == SEARCH_PV) {
    if (node->best_score <= node->orig_alpha) {
      tt_hashtable_put(
          node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), (uint8_t) UPPER,
          NULL_MOVE);
    } else if (node->best_score >= node->beta) {
      tt_hashtable_put(
          node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), (uint8_t) LOWER,
          node->subpv[0]);
    } else {
      tt_hashtable_put(
          node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), (uint8_t) EXACT,
          node->subpv[0]);
    }
  }
}
