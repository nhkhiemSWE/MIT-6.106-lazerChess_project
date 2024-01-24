// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#include "end_game.h"

#include <stdlib.h>

#include "tbassert.h"

bool player_wins(position_t* p, color_t c) {
  int white_monarchs = 0;
  int black_monarchs = 0;
  // for (fil_t f = 0; f < BOARD_WIDTH; f++) {
  //   for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
  //     piece_t piece = p->board[square_of(f, r)];
  //     if (ptype_of(piece) == MONARCH) {
  //       if (color_of(piece) == WHITE)
  //         white_monarchs++;
  //       else
  //         black_monarchs++;
  //     }
  //   }
  // }
  if (get_monarch(p, WHITE, 0)) white_monarchs ++;
  if (get_monarch(p, WHITE, 1)) white_monarchs ++;
  if (get_monarch(p, BLACK, 0)) black_monarchs ++;
  if (get_monarch(p, BLACK, 1)) black_monarchs ++;
  
  if (c)
    return ((color_to_move_of(p) == BLACK && white_monarchs < black_monarchs) ||
            !white_monarchs);
  else
    return ((color_to_move_of(p) == WHITE && white_monarchs > black_monarchs) ||
            !black_monarchs);
}

// Goal is to start your turn with more monarchs that your opponent.
// So, if at the start of their turn, either player has more monarchs,
// the game is over.
// Additionally, the game ends when either player has 0 monarchs.
bool is_game_over(position_t* p) {
  return player_wins(p, WHITE) || player_wins(p, BLACK);
}

// check the victim pieces returned by the move to determine if it's a
// game-over situation.  If so, also calculate the score depending on
// the pov (which player's point of view)
static score_t get_game_over_score(position_t* p, int pov, int ply) {
  score_t score;
  if (player_wins(p, BLACK)) {
    score = -WIN * pov;
  } else if (player_wins(p, WHITE)) {
    score = WIN * pov;
  } else {
    score = 0;
    tbassert(false, "Should not happen.\n");
  }
  if (score < 0) {
    score += ply;
  } else {
    score -= ply;
  }
  return score;
}

// Here's a great spot to add in a closing book/end game table if you choose to
// implement one.

// In this function, determine if the position is in the end game table or not.
bool is_end_game_position(position_t* p) { return is_game_over(p); }

// In this function, read the end game table and return the score of this
// position, accounting for who wins and how many moves away this end game
// position is.
//
// The score of an end-game position is dependent on:
//   - Whose point of view the search is occurring from
//   - How many layers deep this position occurs in the search
//   - How many more moves it will take the game to end once it reaches this
//   position.
score_t get_end_game_score(position_t* p, int pov, int ply) {
  // Note: pov and ply are for the move that *generated* this position, not the
  // position itself.
  //       This means you might have an off-by-one error when reading from your
  //       closing book.
  return get_game_over_score(p, pov, ply);
}
