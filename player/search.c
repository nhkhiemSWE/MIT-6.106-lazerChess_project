// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

/*
The search module contains the alpha-beta search utilities for Leiserchess,
consisting of three main functions:

  1. searchRoot() is called from outside the module.
  2. scout_search() performs a null-window search.
  3. searchPV() performs the normal alpha-beta search.

searchRoot() calls scout_search() to order the moves.  searchRoot() then
calls searchPV() to perform the full search.
*/

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "end_game.h"
#include "eval.h"
#include "fen.h"
#include "search.h"
#include "tbassert.h"
#include "tt.h"
#include "util.h"

// -----------------------------------------------------------------------------
// Preprocessor
// -----------------------------------------------------------------------------

#define ABORT_CHECK_PERIOD 0xfff

// -----------------------------------------------------------------------------
// READ ONLY settings (see iopt in leiserchess.c)
// -----------------------------------------------------------------------------

// eval score of a board state that is draw for both players.
int DRAW;

// POSITIONAL WEIGHTS evaluation terms
int HMB;  // having-the-move bonus

// Late-move reduction
int LMR_R1;  // Look at this number of moves full width before reducing 1 ply
int LMR_R2;  // After this number of moves, reduce 2 ply

int USE_NMM;       // Null move margin
int TRACE_MOVES;   // Print moves
int DETECT_DRAWS;  // Detect draws by repetition
int NMOVES_DRAW;   // Number of contiguous non-capture moves after which a draw
                   // is declared (1 move = 2 ply)

// do not set more than 5 ply
int FUT_DEPTH;  // set to zero for no futilty

// From search_scout.c
static score_t searchPV(searchNode* node, int depth,
                        uint64_t* node_count_serial);
static score_t scout_search(searchNode* node, int depth,
                            uint64_t* node_count_serial);

// From search_globals.c
// static move_t killer[];
// static int best_move_history[];
static void update_transposition_table(searchNode* node);

static void update_best_move_history(position_t* p, int index_of_best,
                                     sortable_move_t* lst, int count, int* best_move_history);
static void finish_search(uint64_t pos_hash, move_t mv);
static bool is_move_searching(uint64_t pos_hash, move_t mv);
static void set_search_move(uint64_t pos_hash, move_t mv);

// From search_common.c
static score_t get_draw_score(position_t* p, int ply);
static void getPV(bestMove* pv, char* buf, size_t bufsize);
static void print_move_info(move_t mv, int ply, position_t* pos);
static leafEvalResult evaluate_as_leaf(searchNode* node, searchType_t type);
static moveEvaluationResult evaluateMove(searchNode* node, move_t mv,
                                         move_t killer_a, move_t killer_b,
                                         searchType_t type,
                                         uint64_t* node_count_serial);
static void sort_insertion(sortable_move_t* move_list, int num_of_moves);
static bool search_process_score(searchNode* node, move_t mv, int mv_index,
                                 moveEvaluationResult* result,
                                 searchType_t type);
static bool should_abort_check();
static int get_sortable_move_list(searchNode* node, sortable_move_t* move_list, move_t hash_table_move);
static int get_sortable_move_list_partial(searchNode* node, sortable_move_t* move_list, move_t hash_table_move);
// Include common search functions
#include "./search_common.c"
#include "./search_globals.c"
#include "./search_scout.c"

// Initializes a PV (principal variation node)
// https://www.chessprogramming.org/Node_Types#PV-Nodes
static void initialize_pv_node(searchNode* node, int depth) {
  node->type = SEARCH_PV;
  node->alpha = -node->parent->beta;
  node->orig_alpha = node->alpha;  // Save original alpha.
  node->beta = -node->parent->alpha;
  node->subpv[0] = NULL_MOVE;
  node->depth = depth;
  node->legal_move_count = 0;
  node->ply = node->parent->ply + 1;
  node->fake_color_to_move = color_to_move_of(&(node->position));
  // point of view = 1 for White, -1 for Black
  node->pov = 1 - node->fake_color_to_move * 2;
  node->quiescence = (depth <= 0);
  node->best_move_index = 0;
  node->best_score = -INF;
  node->abort = false;
  node->killer = node->parent->killer;
  node->best_move_history = node->parent->best_move_history;
}

// Perform a Principal Variation Search
//
// https://www.chessprogramming.org/Principal_Variation_Search
static score_t searchPV(searchNode* node, int depth,
                        uint64_t* node_count_serial) {
  // Initialize the searchNode data structure.
  initialize_pv_node(node, depth);

  // Pre-evaluate the node to determine if we need to search further.
  leafEvalResult pre_evaluation_result = evaluate_as_leaf(node, SEARCH_PV);

  // use some information from the pre-evaluation
  move_t hash_table_move = pre_evaluation_result.hash_table_move;

  if (pre_evaluation_result.type == MOVE_EVALUATED) {
    return pre_evaluation_result.score;
  }

  if (pre_evaluation_result.score > node->best_score) {
    node->best_score = pre_evaluation_result.score;
    if (node->best_score > node->alpha) {
      node->alpha = node->best_score;
    }
  }

  // Get the killer moves at this node.
  move_t killer_a = node->killer[KMT(node->ply, 0)];
  move_t killer_b = node->killer[KMT(node->ply, 1)];

  // sortable_move_t move_list
  //
  // Contains a list of possible moves at this node. These moves are "sortable"
  //   and can be compared as integers. This is accomplished by using high-order
  //   bits to store a sort key. To get a better idea of what these sort keys
  //   mean, look at the get_sortable_move_list function in search_common.c.
  //
  // Keep track of the number of moves that we have considered at this node.
  //   After we finish searching moves at this node the move_list array will
  //   be organized in the following way:
  //
  //   m0, m1, ... , m_k-1, m_k, ... , m_N-1
  //
  //  where k = num_moves_tried, and N = num_of_moves
  //
  //  This will allow us to update the best_move_history table easily by
  //  scanning move_list from index 0 to k such that we update the table
  //  only for moves that we actually considered at this node.

  sortable_move_t move_list_with_sentinel[MAX_NUM_MOVES];
  move_list_with_sentinel[0].key = MAX_SORT_KEY;
  sortable_move_t* move_list = move_list_with_sentinel + 1;
  int num_of_moves = get_sortable_move_list(node, move_list, hash_table_move);
  int num_moves_tried = 0;

  // Insertion sort the move list
  sort_insertion(move_list_with_sentinel, num_of_moves + 1);

  // Start searching moves
  sortable_move_t deferred[MAX_NUM_MOVES];
  sortable_move_t tried[MAX_NUM_MOVES];
  int deferred_count = 0;
  bool isFirst = true;
  sortable_move_t* moves = move_list;

  for (int abd_pass = 0; abd_pass <2; abd_pass++) {
    int move_count = (abd_pass == 0) ? num_of_moves : deferred_count;
    for (int mv_index = 0; mv_index < move_count; mv_index++) {
      move_t mv = get_move(moves[mv_index]);
      if (abd_pass == 0 && is_move_searching(node->position.key, mv) && !isFirst) {
        deferred[deferred_count++].mv =mv;
        isFirst = false;
        continue;
      }
      tried[num_moves_tried++].mv = mv;
      set_search_move(node->position.key, mv);
      isFirst = false;

      (*node_count_serial) ++;

      if (TRACE_MOVES) {
        print_move_info(mv, node->ply, &node->position);
      }

      // printf("calling from searchPV \n");
      moveEvaluationResult result = evaluateMove(node, mv, killer_a, killer_b,
                                                SEARCH_PV, node_count_serial);
      finish_search(node->position.key, mv);

      if (result.type == MOVE_ILLEGAL || result.type == MOVE_IGNORE) {
        continue;
      }

      // A legal move, but when we are in quiescence
      // we only want to count moves that has a capture.
      if (result.type == MOVE_EVALUATED) {
        node->legal_move_count++;
      }

      // Check if we should abort due to time control.
      if (abortf) {
        return 0;
      }

      bool cutoff = search_process_score(node, mv, mv_index, &result, SEARCH_PV);
      if (cutoff) {
        break;
      }
    }
    moves  = deferred;
  }

  if (node->quiescence == false) {
    update_best_move_history(&(node->position), node->best_move_index,
                            //  move_list, num_moves_tried);
                            tried, num_moves_tried, node->best_move_history);
  }

  tbassert(abs(node->best_score) != -INF, "best_score = %d\n",
           node->best_score);

  // Update the transposition table.
  //
  // Note: This function reads node->best_score, node->orig_alpha,
  //   node->position.key, node->depth, node->ply, node->beta,
  //   node->alpha, node->subpv
  update_transposition_table(node);
  return node->best_score;
}

// -----------------------------------------------------------------------------
// searchRoot
//
// This handles scout search logic for the first level of the search tree
// -----------------------------------------------------------------------------
static void initialize_root_node(searchNode* node, score_t alpha, score_t beta,
                                 int depth, int ply, position_t* p, move_t* killer, int* best_move_history) {
  node->type = SEARCH_ROOT;
  node->alpha = alpha;
  node->beta = beta;
  node->depth = depth;
  node->ply = ply;
  node->position = *p;
  node->fake_color_to_move = color_to_move_of(&(node->position));
  node->best_score = -INF;
  node->pov =
      1 - node->fake_color_to_move * 2;  // pov = 1 for White, -1 for Black
  node->abort = false;
  node->killer =  killer;
  node->best_move_history = best_move_history;
}

score_t searchRoot(position_t* p, score_t alpha, score_t beta, int depth,
                   int ply, bestMove* pv, uint64_t* node_count_serial,
                   FILE* OUT, int thread, sortable_move_t move_list[MAX_NUM_MOVES], move_t* killer, int* best_move_history) {
  static int num_of_moves = 0;  // number of moves in list
  // hopefully, more than we will need
  // static sortable_move_t move_list[MAX_NUM_MOVES];

  if (depth == 1) {
    // we are at depth 1; generate all possible moves
    num_of_moves = generate_all(p, move_list);
    // shuffle the list of moves
    for (int i = 0; i < num_of_moves; i++) {
      int r = myrand() % num_of_moves;
      sortable_move_t tmp = move_list[i];
      move_list[i] = move_list[r];
      move_list[r] = tmp;
    }
  }

  searchNode rootNode;
  rootNode.parent = NULL;
  initialize_root_node(&rootNode, alpha, beta, depth, ply, p, killer, best_move_history);

  tbassert(rootNode.best_score == alpha, "root best score not equal alpha");

  searchNode next_node;
  next_node.subpv[0] = NULL_MOVE;
  next_node.parent = &rootNode;

  score_t score;

  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    move_t mv = get_move(move_list[mv_index]);

    if (TRACE_MOVES) {
      print_move_info(mv, ply, &rootNode.position);
    }

    (*node_count_serial)++;

    // make the move.
    // printf("Calling from searchRoot \n");
    victims_t x = make_move(&(rootNode.position), &(next_node.position), mv);

    if (is_ILLEGAL(x)) {
      continue;  // not a legal move
    }

    if (is_end_game_position(&(next_node.position))) {
      score =
          get_end_game_score(&(next_node.position), rootNode.pov, rootNode.ply);
      next_node.subpv[0] = NULL_MOVE;
      goto scored;
    }

    if (is_draw(&(next_node.position))) {
      score = get_draw_score(&(next_node.position), rootNode.ply);
      next_node.subpv[0] = NULL_MOVE;
      goto scored;
    }

    if (mv_index == 0 || rootNode.depth == 1) {
      // We guess that the first move is the principal variation
      score = -searchPV(&next_node, rootNode.depth - 1, node_count_serial);

      // Check if we should abort due to time control.
      if (abortf) {
        return 0;
      }
    } else {
      score = -scout_search(&next_node, rootNode.depth - 1, node_count_serial);

      // Check if we should abort due to time control.
      if (abortf) {
        return 0;
      }

      // If its score exceeds the current best score,
      if (score > rootNode.alpha) {
        score = -searchPV(&next_node, rootNode.depth - 1, node_count_serial);
        // Check if we should abort due to time control.
        if (abortf) {
          return 0;
        }
      }
    }

  scored:
    // only valid for the root node:
    tbassert((score > rootNode.best_score) == (score > rootNode.alpha),
             "score = %d, best = %d, alpha = %d\n", score, rootNode.best_score,
             rootNode.alpha);
    if (score > rootNode.best_score) {
      tbassert(score > rootNode.alpha, "score: %d, alpha: %d\n", score,
               rootNode.alpha);

      rootNode.best_score = score;
      simple_acquire(&pv[depth].mutex);
      if (pv[depth].has_been_set == false || score > pv[depth].score) {
        pv[depth].score = score;
        pv[depth].move = mv;
        pv[depth].has_been_set = true;
      }

      // pv[0] = mv;
      // memcpy(pv + 1, next_node.subpv, sizeof(move_t) * (MAX_PLY_IN_SEARCH - 1));
      // pv[MAX_PLY_IN_SEARCH - 1] = NULL_MOVE;

      // Print out based on UCI (universal chess interface)
      double et = elapsed_time();
      char pvbuf[MAX_PLY_IN_SEARCH * MAX_CHARS_IN_MOVE];
      getPV(pv, pvbuf, MAX_PLY_IN_SEARCH * MAX_CHARS_IN_MOVE);
      if (et < 0.00001) {
        et = 0.00001;  // hack so that we don't divide by 0
      }
      simple_release(&pv[depth].mutex);

      uint64_t nps = 1000 * *node_count_serial / et;
      fprintf(OUT,
              "info depth %d move_no %d time (microsec) %d nodes %" PRIu64
              " nps %" PRIu64 " thread %d\n",
              depth, mv_index + 1, (int)(et * 1000), *node_count_serial, nps, thread);
      fprintf(OUT, "info score cp %d pv %s\n", score, pvbuf);

      // Slide this move to the front of the move list
      for (int j = mv_index; j > 0; j--) {
        move_list[j] = move_list[j - 1];
      }
      move_list[0] = make_sortable(mv);
    }

    // Normal alpha-beta logic: if the current score is better than what the
    // maximizer has been able to get so far, take that new value.  Likewise,
    // score >= beta is the beta cutoff condition
    if (score > rootNode.alpha) {
      rootNode.alpha = score;
    }
    if (score >= rootNode.beta) {
      tbassert(0, "score: %d, beta: %d\n", score, rootNode.beta);
      break;
    }
  }

  return rootNode.best_score;
}
