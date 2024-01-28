// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

// This file is #included in search.c and is not compiled separately

// This file contains the scout search routine. Although this routine contains
//   some duplicated logic from the searchPV routine in search.c, it is
//   convenient to maintain it separately. This allows one to easily
//   parallelize scout search separately from searchPV.

#include <stdbool.h>

#include "search.h"
#include "simple_mutex.h"
#include "tbassert.h"

static bool abortf;

// Checks whether a node's parent has aborted.
//   If this occurs, we should just stop and return 0 immediately.
static bool parallel_parent_aborted(searchNode* node) {
  searchNode* pred = node->parent;
  while (pred != NULL) {
    if (pred->abort) {
      return true;
    }
    pred = pred->parent;
  }
  return false;
}

// Initialize a scout search node for a "Null Window" search.
// https://www.chessprogramming.org/Scout
// https://www.chessprogramming.org/Null_Window
static void initialize_scout_node(searchNode* node, int depth) {
  node->type = SEARCH_SCOUT;
  node->beta = -(node->parent->alpha);
  node->alpha = node->beta - 1;
  node->depth = depth;
  node->ply = node->parent->ply + 1;
  node->subpv[0] = NULL_MOVE;
  node->legal_move_count = 0;
  node->fake_color_to_move = color_to_move_of(&(node->position));
  // point of view = 1 for white, -1 for black
  node->pov = 1 - node->fake_color_to_move * 2;
  node->best_move_index = 0;  // index of best move found
  node->abort = false;
  node->killer = node->parent->killer;
  node->best_move_history = node->parent->best_move_history;
}

bool process_move(move_t mv, int index, searchNode* node, move_t killer_a, move_t killer_b, uint64_t* node_count_serial) {
  if (TRACE_MOVES) {
    print_move_info(mv, node->ply, &node->position);
  }
  // printf("Calling from process_move \n");
  moveEvaluationResult result = evaluateMove(node, mv, killer_a, killer_b, SEARCH_SCOUT, node_count_serial);

  finish_search(node->position.key, mv) ;

  if (result.type == MOVE_ILLEGAL || result.type == MOVE_IGNORE || abortf || parallel_parent_aborted(node)) {
    return false;
  }

  // A legal move, but when we are in quiescence
  // we only want to count moves that has a capture.
  if (result.type == MOVE_EVALUATED) {
    node->legal_move_count++;
  }

  // process the score. Note that this mutates fields in node.
  bool cutoff = search_process_score(node, mv, index, &result, SEARCH_SCOUT);

  if (cutoff) {
    node->abort = true;
    return true;
  }
  return false;
}

static score_t scout_search(searchNode* node, int depth,
                            uint64_t* node_count_serial) {
  // Initialize the search node.
  initialize_scout_node(node, depth);

  // check whether we should abort
  if (should_abort_check() || parallel_parent_aborted(node)) {
    return 0;
  }

  // Pre-evaluate this position.
  leafEvalResult pre_evaluation_result = evaluate_as_leaf(node, SEARCH_SCOUT);

  // If we decide to stop searching, return the pre-evaluation score.
  if (pre_evaluation_result.type == MOVE_EVALUATED) {
    return pre_evaluation_result.score;
  }

  // Populate some of the fields of this search node, using some
  //  of the information provided by the pre-evaluation.
  move_t hash_table_move = pre_evaluation_result.hash_table_move;
  node->best_score = pre_evaluation_result.score;
  node->quiescence = pre_evaluation_result.should_enter_quiescence;

  // Grab the killer-moves for later use.
  move_t killer_a = node->killer[KMT(node->ply, 0)];
  move_t killer_b = node->killer[KMT(node->ply, 1)];

  // Store the sorted move list on the stack.
  //   MAX_NUM_MOVES is all that we need.
  sortable_move_t move_list_with_sentinel[MAX_NUM_MOVES];
  move_list_with_sentinel[0].key = MAX_SORT_KEY;
  sortable_move_t* move_list = move_list_with_sentinel + 1;


  // Obtain the sorted move list.
  int num_of_moves = get_sortable_move_list_partial(node, move_list, hash_table_move);
  int number_of_moves_evaluated = 0;

  // A simple mutex. See simple_mutex.h for implementation details.
  simple_mutex_t node_mutex;
  init_simple_mutex(&node_mutex);

  sortable_move_t tried[MAX_NUM_MOVES];

  if (num_of_moves > 0) {
    move_t best_move = get_best_move(move_list, num_of_moves);
    tried[number_of_moves_evaluated].mv = best_move;
    // printf("Calling from scout_search after finding best_move \n");
    bool cutoff = process_move(best_move, number_of_moves_evaluated, node, killer_a, killer_b, node_count_serial);
    number_of_moves_evaluated++;

    if (!cutoff) { // Have to evaluate more than one move
      //Sort the move list
      sort_insertion(move_list_with_sentinel, num_of_moves + 1);

      tbassert(move_eq(best_move, move_list[0].mv), "Best move is not at the front of the move_list");

      sortable_move_t deferred[MAX_NUM_MOVES];
      int deferred_count = 0;
      bool isFirst = true;
      sortable_move_t* moves_to_scan = move_list + 1;

      for (int abd_pass = 0; abd_pass <2; abd_pass++) {
        int move_count = (abd_pass == 0) ? num_of_moves - 1 : deferred_count;
        for (int mv_index = 0; mv_index < move_count; mv_index++) {
          move_t mv = get_move(moves_to_scan[mv_index]);
          if (abd_pass == 0 && is_move_searching(node->position.key, mv) && !isFirst) {
            deferred[deferred_count++].mv =mv;
            isFirst = false;
            continue;
          }
          tried[number_of_moves_evaluated].mv = mv;
          set_search_move(node->position.key, mv);
          isFirst = false;

          bool cutoff = process_move(mv, number_of_moves_evaluated++, node, killer_a, killer_b, node_count_serial);
          if (cutoff) {
            mv_index ++;
            break;
          }
        }
        moves_to_scan = deferred;
      }
    }
  }
  // increase node count
  __sync_fetch_and_add(node_count_serial, 1);

  if (parallel_parent_aborted(node)) {
    return 0;
  }

  if (node->quiescence == false) {
    update_best_move_history(&(node->position), node->best_move_index,
                              tried, number_of_moves_evaluated, node->best_move_history);
  }

  tbassert(abs(node->best_score) != -INF, "best_score = %d\n",
           node->best_score);
  // Reads node->position.key, node->depth, node->best_score, and node->ply
  update_transposition_table(node);
  return node->best_score;
}
