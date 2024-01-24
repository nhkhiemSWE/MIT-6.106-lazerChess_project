// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

// This file is #included in search.c and is not compiled separately

#include "search.h"

// tic counter for how often we should check for abort
static int tics = 0;
static double sstart;        // start time of a search in milliseconds
static double timeout;       // time elapsed before abort
static bool abortf = false;  // abort flag for search

static score_t fmarg[10] = {0,
                            PAWN_VALUE / 2,
                            PAWN_VALUE,
                            (PAWN_VALUE * 5) / 2,
                            (PAWN_VALUE * 9) / 2,
                            PAWN_VALUE * 7,
                            PAWN_VALUE * 10,
                            PAWN_VALUE * 15,
                            PAWN_VALUE * 20,
                            PAWN_VALUE * 30};

static const uint64_t MAX_SORT_KEY = (1ULL << 32) - 1;

void init_abort_timer(double goal_time) {
  sstart = milliseconds();
  // don't go over any more than 3 times the goal
  timeout = sstart + goal_time * 3.0;
}

double elapsed_time() { return milliseconds() - sstart; }

bool should_abort() { return abortf; }

void reset_abort() { abortf = false; }

void init_tics() { tics = 0; }

move_t get_move(sortable_move_t sortable_mv) { return sortable_mv.mv; }

static score_t get_draw_score(position_t* p, int ply) {
  position_t* x = p->history;
  uint64_t cur = p->key;
  score_t score;
  while (true) {
    if (!zero_victims(x->victims)) {
      break;  // cannot be a repetition
    }
    x = x->history;
    if (!zero_victims(x->victims)) {
      break;  // cannot be a repetition
    }
    if (x->key == cur) {  // is a repetition
      if (ply & 1) {
        score = -DRAW;
      } else {
        score = DRAW;
      }
      return score;
    }
    x = x->history;
  }
  // assert(false);  // This should not occur.
  return (score_t)0;
}

// Detect draws -- either move repetition, or too many moves since last capture
bool is_draw(position_t* p) {
  if (!DETECT_DRAWS) {
    return false;  // no draw detected
  }

  // Check whether it has been 2*NMOVES_DRAW ply since the last capture
  if (p->nply_since_victim >= 2 * NMOVES_DRAW) {
    return true;
  }

  // Check whether this move has been repeated at least DRAW_NUM_REPS times over
  // the history of the game or 2x within the search
  const uint64_t cur = p->key;
  const int ply = p->ply;
  size_t reps_history = 0;
  size_t reps_search = 0;

  while (p != NULL) {
    const bool isSamePlayer = p->ply % 2 == ply % 2;
    const bool isSameBoard = p->key == cur;
    if (isSamePlayer && isSameBoard) {
      reps_history++;
      if (!(p->was_played)) {
        reps_search++;
      }
    };
    p = p->history;
  }
  if ((reps_history >= DRAW_NUM_REPS) || (reps_search >= 2)) {
    return true;
  }

  return false;
}

static void getPV(move_t* pv, char* buf, size_t bufsize) {
  buf[0] = 0;

  for (int i = 0; i < (MAX_PLY_IN_SEARCH - 1) && !move_eq(pv[i], NULL_MOVE);
       i++) {
    char a[MAX_CHARS_IN_MOVE];
    move_to_str(pv[i], a, MAX_CHARS_IN_MOVE);
    if (i != 0) {
      strncat(buf, " ",
              bufsize - strlen(buf) - 1);  // - 1, for the terminating '\0'
    }
    strncat(buf, a,
            bufsize - strlen(buf) - 1);  // - 1, for the terminating '\0'
  }
}

static void print_move_info(move_t mv, int ply, position_t* pos) {
  char buf[MAX_CHARS_IN_MOVE];
  move_to_str(mv, buf, MAX_CHARS_IN_MOVE);
  printf("info");
  int i = 0;
  while (pos != NULL && i < ply) {
    char _buf[MAX_CHARS_IN_MOVE];
    move_to_str(pos->last_move, _buf, MAX_CHARS_IN_MOVE);
    printf(" %s", _buf);
    pos = pos->history;
    i++;
  }
  printf(" %s %d\n", buf, ply);
}

// Evaluates the node before performing a full search.
//   does a few things differently if in scout search.
static leafEvalResult evaluate_as_leaf(searchNode* node, searchType_t type) {
  leafEvalResult result;
  result.type = MOVE_IGNORE;
  result.score = -INF;
  result.should_enter_quiescence = false;
  result.hash_table_move = NULL_MOVE;

  // get transposition table record if available.
  //
  // https://www.chessprogramming.org/Transposition_Table
  compressedTTRec_t* rec = tt_hashtable_get(node->position.key);
  if (rec) {
    if (type == SEARCH_SCOUT && tt_is_usable(rec, node->depth, node->beta)) {
      result.type = MOVE_EVALUATED;
      result.score = tt_adjust_score_from_hashtable(rec, node->ply);
      return result;
    }
    result.hash_table_move = tt_move_of(rec);
  }

  // stand pat (having-the-move) bonus
  //
  // https://www.chessprogramming.org/Quiescence_Search#Standing_Pat
  score_t sps = eval(&(node->position), false) + HMB;
  bool quiescence = (node->depth <= 0);  // are we in quiescence?
  result.should_enter_quiescence = quiescence;
  if (quiescence) {
    result.score = sps;
    if (result.score >= node->beta) {
      result.type = MOVE_EVALUATED;
      return result;
    }
  }

  // margin based forward pruning
  if (type == SEARCH_SCOUT && USE_NMM) {
    if (node->depth <= 2) {
      if (node->depth == 1 && sps >= node->beta + 3 * PAWN_VALUE) {
        result.type = MOVE_EVALUATED;
        result.score = node->beta;
        return result;
      }
      if (node->depth == 2 && sps >= node->beta + 5 * PAWN_VALUE) {
        result.type = MOVE_EVALUATED;
        result.score = node->beta;
        return result;
      }
    }
  }

  // extended futility pruning
  //
  // https://www.chessprogramming.org/Futility_Pruning#Extended_Futility_Pruning
  if (type == SEARCH_SCOUT && node->depth <= FUT_DEPTH && node->depth > 0) {
    if (sps + fmarg[node->depth] < node->beta) {
      // treat this ply as a quiescence ply, look only at captures
      result.should_enter_quiescence = true;
      result.score = sps;
    }
  }
  return result;
}

// Evaluate the move by performing a search.
static moveEvaluationResult evaluateMove(searchNode* node, move_t mv,
                                         move_t killer_a, move_t killer_b,
                                         searchType_t type,
                                         uint64_t* node_count_serial) {
  int ext = 0;           // extensions
  bool blunder = false;  // shoot our own piece
  moveEvaluationResult result;
  result.next_node.subpv[0] = NULL_MOVE;
  result.next_node.parent = node;

  // Make the move, and get any victim pieces.
  victims_t victims =
      make_move(&(node->position), &(result.next_node.position), mv);

  // Check whether the game is a game over position - either someone has won or
  // it's in our closing book.
  if (is_end_game_position(&(result.next_node.position))) {
    // Compute the end-game score.
    result.type = MOVE_GAMEOVER;
    result.score =
        get_end_game_score(&(result.next_node.position), node->pov, node->ply);
    return result;
  }

  // Ignore noncapture moves when in quiescence.
  if (zero_victims(victims) && node->quiescence) {
    result.type = MOVE_IGNORE;
    return result;
  }

  // Check whether the game results in a draw.
  if (is_draw(&(result.next_node.position))) {
    result.type = MOVE_GAMEOVER;
    result.score = get_draw_score(&(result.next_node.position), node->ply);
    return result;
  }

  // Check whether we blundered (caused only our own pieces to be zapped).
  //FIXED:
  color_t c = node->fake_color_to_move;
  blunder = victims.removed_color[c] && !victims.removed_color[opp_color(c)];

  // Do not consider moves that are blunders while in quiescence.
  if (node->quiescence && blunder) {
    result.type = MOVE_IGNORE;
    return result;
  }

  // Extend the search-depth by 1 if we captured a piece, since that means the
  // move was interesting.
  //
  // https://www.chessprogramming.org/Capture_Extensions
  if (victim_exists(victims) && !blunder) {
    ext = 1;
  }

  // Late move reductions - or LMR. Only done in scout search.
  //
  // https://www.chessprogramming.org/Late_Move_Reductions
  int next_reduction = 0;
  if (type == SEARCH_SCOUT && node->legal_move_count + 1 >= LMR_R1 &&
      node->depth > 2 && zero_victims(victims) && !move_eq(mv, killer_a) &&
      !move_eq(mv, killer_b)) {
    if (node->legal_move_count + 1 >= LMR_R2) {
      next_reduction = 2;
    } else {
      next_reduction = 1;
    }
  }

  result.type = MOVE_EVALUATED;
  int search_depth = ext + node->depth - 1;

  // Check if we need to perform a reduced-depth search.
  //
  // After a reduced-depth search, a full-depth search will be performed if the
  //  reduced-depth search did not trigger a cut-off.
  if (next_reduction > 0) {
    search_depth -= next_reduction;
    int reduced_depth_score =
        -scout_search(&(result.next_node), search_depth, node_count_serial);
    if (reduced_depth_score < node->beta) {
      result.score = reduced_depth_score;
      return result;
    }
    search_depth += next_reduction;
  }

  // Check if we should abort due to time control.
  if (abortf) {
    result.score = 0;
    result.type = MOVE_IGNORE;
    return result;
  }

  if (type == SEARCH_SCOUT) {
    result.score =
        -scout_search(&(result.next_node), search_depth, node_count_serial);
  } else {
    if (node->legal_move_count == 0 || node->quiescence) {
      result.score =
          -searchPV(&(result.next_node), search_depth, node_count_serial);
    } else {
      result.score =
          -scout_search(&(result.next_node), search_depth, node_count_serial);
      if (result.score > node->alpha) {
        result.score = -searchPV(&(result.next_node), node->depth + ext - 1,
                                 node_count_serial);
      }
    }
  }

  return result;
}

// Insertion sort of the move list.
//DELETED

static void heapify(sortable_move_t* move_list, int num_of_moves, int root) {
  int largest = root;
  int left = 2 * root + 1;
  int right = 2 * root + 2;

  if (left < num_of_moves &&
      move_list[left].key > move_list[largest].key) {
    largest = left;
  }

  if (right < num_of_moves &&
      move_list[right].key > move_list[largest].key) {
    largest = right;
  }

  if (largest != root) {
    sortable_move_t temp = move_list[root];
    move_list[root] = move_list[largest];
    move_list[largest] = temp;

    heapify(move_list, num_of_moves, largest);
  }
}

void buildMaxHeap(sortable_move_t* move_list, int num_of_moves) {
  for (int i = num_of_moves / 2 - 1; i >= 0; i--) {
    heapify(move_list, num_of_moves, i);
  }
}

move_t get_move_from_heap(sortable_move_t* move_heap,
                                 sortable_move_t* sorted_moves, int num_sorted, int num_moves) {

  int heap_size = num_moves - num_sorted;
  if (heap_size == 0) {
    return NULL_MOVE;
  }

  sortable_move_t max = move_heap[0];

  move_heap[0] = move_heap[heap_size - 1];

  heapify(move_heap, heap_size - 1, 0);

  sorted_moves[num_sorted] = max;
  return max.mv;
}

// Returns true if a cutoff was triggered, false otherwise.
static bool search_process_score(searchNode* node, move_t mv, int mv_index,
                                 moveEvaluationResult* result,
                                 searchType_t type) {
  if (result->score > node->best_score) {
    node->best_score = result->score;
    node->best_move_index = mv_index;
    node->subpv[0] = mv;

    // write best move into right position in PV buffer.
    memcpy(node->subpv + 1, result->next_node.subpv,
           sizeof(move_t) * (MAX_PLY_IN_SEARCH - 1));
    node->subpv[MAX_PLY_IN_SEARCH - 1] = NULL_MOVE;

    if (type != SEARCH_SCOUT && result->score > node->alpha) {
      node->alpha = result->score;
    }

    if (result->score >= node->beta) {
      if (!move_eq(mv, killer[KMT(node->ply, 0)]) && ENABLE_TABLES) {
        killer[KMT(node->ply, 1)] = killer[KMT(node->ply, 0)];
        killer[KMT(node->ply, 0)] = mv;
      }
      return true;
    }
  }
  return false;
}

// Check if we should abort.
static bool should_abort_check() {
  tics++;
  if ((tics & ABORT_CHECK_PERIOD) == 0) {
    if (milliseconds() >= timeout) {
      abortf = true;
      return true;
    }
  }
  return false;
}

// Obtain a sorted move list.
//
// https://www.chessprogramming.org/Move_Ordering

static uint8_t get_move_list_and_process_best(searchNode* node, sortable_move_t* move_list, int num_of_moves, sortable_move_t* sorted_moves, move_t hash_table_move) {
  // number of moves in list
  move_t killer_a = killer[KMT(node->ply, 0)];
  move_t killer_b = killer[KMT(node->ply, 1)];

  uint8_t num_best = 0;

  // sort special moves to the front
  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    move_t mv = get_move(move_list[mv_index]);
    if (move_eq(mv, hash_table_move)) {
      move_list[mv_index].key = MAX_SORT_KEY;
      sorted_moves[num_best++] = move_list[mv_index];    
    } else if (move_eq(mv, killer_a)) {
      move_list[mv_index].key = MAX_SORT_KEY - 1;
      sorted_moves[num_best++] = move_list[mv_index];
    } else if (move_eq(mv, killer_b)) {
      move_list[mv_index].key = MAX_SORT_KEY - 2;
      sorted_moves[num_best++] = move_list[mv_index];
    }
  }

    //sort moves in sorted_moves
  for (int i = 1; i < num_best; ++i) {
    sortable_move_t temp = sorted_moves[i];
    int j = i - 1;

    // Move elements of sorted_moves[0..i-1], that are smaller than key,
    // to one position ahead of their current position
    while (j >= 0 && sorted_moves[j].key < temp.key) {
        sorted_moves[j + 1] = sorted_moves[j];
        j = j - 1;
    }
    sorted_moves[j + 1] = temp;
  }

  return num_best;
}
static int process_move_list_after_best(searchNode* node, sortable_move_t* move_list, move_t hash_table_move, int num_of_moves) {
  color_t fake_color_to_move = color_to_move_of(&(node->position));

  move_t killer_a = killer[KMT(node->ply, 0)];
  move_t killer_b = killer[KMT(node->ply, 1)];

  int current_index = 0;
  int num_zeros = 0;

  for (int i = 0; i < num_of_moves; i++) {
    move_t mv = get_move(move_list[current_index]);
    if (move_eq(mv, hash_table_move)) {
      move_list[current_index].key = MAX_SORT_KEY;
      current_index++;
    } else if (move_eq(mv, killer_a)) {
      move_list[current_index].key = MAX_SORT_KEY - 1;
      current_index++;
    } else if (move_eq(mv, killer_b)) {
      move_list[current_index].key = MAX_SORT_KEY - 2;
      current_index++;
    } else {
      ptype_t pce = mv.typ;
      rot_t ro = mv.rot;
      square_t fs = mv.from_sq;
      square_t ts = mv.to_sq;
      int ot = (ori_of(node->position.board[fs]) + ro) % NUM_ORI;
      move_list[current_index].key =
          best_move_history[BMH(fake_color_to_move, pce, ts, ot)];
      if (move_list[current_index].key == 0) {
        num_zeros++;
        sortable_move_t tmp = move_list[num_of_moves - num_zeros];
        move_list[num_of_moves-num_zeros] = move_list[current_index];
        move_list[current_index] = tmp;
      } else {
        current_index++;
      }
    }
  }
  return num_zeros;
}
