// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#include "./fen.h"

#include <stdbool.h>
#include <stdio.h>

#include "tbassert.h"

const char* STARTPOS_FEN =
    "nn6nn/sesw1sesw1sesw/8/8/8/8/NENW1NENW1NENW/SS6SS W";
const char* ENDGAME_FEN = "ss7/8/8/8/8/8/8/7NN W";

static void fen_error(const char* fen, int c_count, const char* msg) {
  fprintf(stderr, "\nError in FEN string:\n");
  fprintf(stderr, "   %s\n  ", fen);  // Indent 3 spaces
  for (int i = 0; i < c_count; ++i) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");  // graphical pointer to error character in string
  fprintf(stderr, "%s\n", msg);
}

// parse_fen_board
// Input:   board representation as a fen string
//          unpopulated board position struct
// Output:  index of where board description ends or 0 if parsing error
//          (populated) board position struct
static int parse_fen_board(position_t* p, const char* fen) {
  // Invariant: square (f, r) is last square filled.
  // Fill from last rank to first rank, from first file to last file
  fil_t f = -1;
  rnk_t r = BOARD_WIDTH - 1;
  int white_monarch_count = 0;
  int black_monarch_count = 0;

  // Current and next characters from input FEN description
  char c, next_c;

  // Invariant: fen[c_count] is next character to be read
  int c_count = 0;

  // Loop also breaks internally if (f, r) == (BOARD_WIDTH-1, 0)
  while ((c = fen[c_count++]) != '\0') {
    piece_t* piece;

    switch (c) {
      // ignore whitespace until the end
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if ((f == BOARD_WIDTH - 1) && (r == 0)) {  // our job is done
          return c_count;
        }
        break;

      // digits
      case '1':
        if (fen[c_count] == '0') {
          c_count++;
          c += 9;
        }
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        while (c > '0') {
          if (++f >= BOARD_WIDTH) {
            fen_error(fen, c_count, "Too many squares in rank.\n");
            return 0;
          }
          set_ptype(&p->board[square_of(f, r)], EMPTY);
          c--;
        }
        break;

      // pieces
      case 'N':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        square_t white_sq = square_of(f, r);
        piece = &(p->board[white_sq]);
        set_color(piece, WHITE);
        p->piece_loc[WHITE] |= sq_to_bitmask[white_sq];

        if (next_c == 'N') {  // White Monarch facing North
          if (white_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[WHITE][white_monarch_count++] = white_sq;
          set_ori(piece, NN);
          set_ptype(piece, MONARCH);
        } else if (next_c == 'W') {  // White Pawn facing NW
          set_ori(piece, NW);
          set_ptype(piece, PAWN);
        } else if (next_c == 'E') {  // White Pawn facing NE
          set_ori(piece, NE);
          set_ptype(piece, PAWN);
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 'n':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        square_t black_sq = square_of(f, r);
        piece = &(p->board[black_sq]);
        set_color(piece, BLACK);
        p->piece_loc[BLACK] |= sq_to_bitmask[black_sq];

        if (next_c == 'n') {  // Black Monarch facing North
          if (black_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[BLACK][black_monarch_count++] = black_sq;
          set_ori(piece, NN);
          set_ptype(piece, MONARCH);
        } else if (next_c == 'w') {  // Black Pawn facing NW
          set_ori(piece, NW);
          set_ptype(piece, PAWN);
        } else if (next_c == 'e') {  // Black Pawn facing NE
          set_ori(piece, NE);
          set_ptype(piece, PAWN);
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 'S':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        white_sq = square_of(f, r);
        piece = &(p->board[white_sq]);
        set_color(piece, WHITE);
        p->piece_loc[WHITE] |= sq_to_bitmask[white_sq];

        if (next_c == 'S') {  // White Monarch facing SOUTH
          if (white_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[WHITE][white_monarch_count++] = white_sq;
          set_ori(piece, SS);
          set_ptype(piece, MONARCH);
        } else if (next_c == 'W') {  // White Pawn facing SW
          set_ori(piece, SW);
          set_ptype(piece, PAWN);
        } else if (next_c == 'E') {  // White Pawn facing SE
          set_ori(piece, SE);
          set_ptype(piece, PAWN);
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 's':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        black_sq = square_of(f, r);
        piece = &(p->board[black_sq]);
        set_color(piece, BLACK);
        p->piece_loc[BLACK] |= sq_to_bitmask[black_sq];

        if (next_c == 's') {  // Black Monarch facing South
          if (black_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[BLACK][black_monarch_count++] = black_sq;
          set_ori(piece, SS);
          set_ptype(piece, MONARCH);
        } else if (next_c == 'w') {  // Black Pawn facing SW
          set_ori(piece, SW);
          set_ptype(piece, PAWN);
        } else if (next_c == 'e') {  // Black Pawn facing SE
          set_ori(piece, SE);
          set_ptype(piece, PAWN);
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 'E':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        white_sq = square_of(f, r);
        piece = &(p->board[white_sq]);


        if (next_c == 'E') {  // White Monarch facing East
          if (white_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[WHITE][white_monarch_count++] = white_sq;
          set_ptype(piece, MONARCH);
          set_color(piece, WHITE);
          set_ori(piece, EE);
          p->piece_loc[WHITE] |= sq_to_bitmask[white_sq];
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 'W':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        white_sq = square_of(f, r);
        piece = &(p->board[white_sq]);


        if (next_c == 'W') {  // White Monarch facing West
          if (white_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[WHITE][white_monarch_count++] = white_sq;
          set_ptype(piece, MONARCH);
          set_color(piece, WHITE);
          set_ori(piece, WW);
          p->piece_loc[WHITE] |= sq_to_bitmask[white_sq];
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 'e':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        black_sq = square_of(f, r);
        piece = &(p->board[black_sq]);


        if (next_c == 'e') {  // Black Monarch facing East
          if (black_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[BLACK][black_monarch_count++] = black_sq;
          set_ptype(piece, MONARCH);
          set_color(piece, BLACK);
          set_ori(piece, EE);
          p->piece_loc[BLACK] |= sq_to_bitmask[black_sq];
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      case 'w':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];
        black_sq = square_of(f, r);
        piece = &(p->board[black_sq]);


        if (next_c == 'w') {  // Black Monarch facing West
          if (black_monarch_count > 1) {
            fen_error(fen, c_count, "Too many white monarchs");
            return 0;
          }
          p->monarch_loc[BLACK][black_monarch_count++] = black_sq;
          set_ptype(piece, MONARCH);
          set_color(piece, BLACK);
          set_ori(piece, WW);
          p->piece_loc[BLACK] |= sq_to_bitmask[black_sq];
        } else {
          fen_error(fen, c_count + 1, "Syntax error");
          return 0;
        }
        break;

      // end of rank
      case '/':
        if (f == BOARD_WIDTH - 1) {
          f = -1;
          if (--r < 0) {
            fen_error(fen, c_count, "Too many ranks");
            return 0;
          }
        } else {
          fen_error(fen, c_count, "Too few squares in rank");
          return 0;
        }
        break;

      default:
        fen_error(fen, c_count, "Syntax error");
        return 0;
        break;
    }  // end switch
  }    // end while

  if ((f == BOARD_WIDTH - 1) && (r == 0)) {
    return c_count;
  } else {
    fen_error(fen, c_count, "Too few squares specified");
    return 0;
  }
}

// returns 0 if no error
static int get_sq_from_str(const char* fen, int* c_count, square_t* sq) {
  char c, next_c;

  while ((c = fen[(*c_count)++]) != '\0') {
    // skip whitespace
    if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r')) {
      continue;
    } else {
      break;  // found nonwhite character
    }
  }

  if (c == '\0') {
    *sq = 0;
    return 0;
  }

  // get file and rank
  if ((c - 'a' < 0) || (c - 'a') > BOARD_WIDTH) {
    fen_error(fen, *c_count, "Illegal specification of last move");
    return 1;
  }

  next_c = fen[(*c_count)++];
  if (next_c == '\0') {
    fen_error(fen, *c_count, "FEN ended before last move fully specified");
  }

  if ((next_c - '0' < 0) || (next_c - '0') > BOARD_WIDTH) {
    fen_error(fen, *c_count, "Illegal specification of last move");
    return 1;
  }

  *sq = square_of(c - 'a', next_c - '0');
  return 0;
}

// Translate a fen string into a board position struct
// Returns 1 if there is an error
int fen_to_pos(position_t* p, const char* fen) {
  static position_t dmy1, dmy2;

  // these sentinels simplify checking previous
  // states without stepping past null pointers.
  dmy1.key = 0;
  dmy1.victims.count = 1;
  // dmy1.victims = 0;
  dmy1.history = NULL;

  dmy2.key = 0;
  dmy2.victims.count = 1;
  // dmy2.victims = 0;
  dmy2.history = &dmy1;

  p->key = 0;            // hash key
  p->victims.count = 0;  // piece destroyed by shooter
  p->history = &dmy2;    // history
  p->was_played = true;  // mark position as played
  p->nply_since_victim = 0;
  //Initialize the piece loc of both color to be 0
  p->piece_loc[WHITE] = 0;
  p->piece_loc[BLACK] = 0;
  //Initialize the removed victims
  p->victims.removed_color[WHITE] = false;
  p->victims.removed_color[BLACK] = false;

  int c_count = 0;  // Invariant: fen[c_count] is next char to be read

  for (int i = 0; i < ARR_SIZE; ++i) {
    p->board[i] = NULL_PIECE;
    set_ptype(&p->board[i], INVALID);  // squares are invalid until filled
  }

  c_count = parse_fen_board(p, fen);
  if (c_count <= 0) {
    return 1;  // parse error
  }

  // p->laser_map[WHITE] = 0;
  // p->laser_map[BLACK] = 0;
  // #pragma clang loop unroll(full)
  // for (int c = 0; c < 2; c ++) {
  //   for (int m = 0; m < MAX_MONARCHS; m ++) {
  //     mark_laser_map(p, c, p->monarch_loc[c][m]);
  //   }
  // }

  int monarchs[2] = {0, 0};

  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    for (rnk_t r = 0; r < BOARD_WIDTH; ++r) {
      square_t sq = square_of(f, r);
      piece_t x = p->board[sq];
      ptype_t typ = ptype_of(x);
      if (typ == MONARCH) {
        monarchs[color_of(x)]++;
      }
    }
  }

  if (monarchs[WHITE] == 0) {
    fen_error(fen, c_count, "No White Monarchs");
    return 1;
  } else if (monarchs[WHITE] > MAX_MONARCHS) {
    fen_error(fen, c_count, "Too many White Monarchs");
    return 1;
  } else if (monarchs[BLACK] == 0) {
    fen_error(fen, c_count, "No Black Monarchs");
    return 1;
  } else if (monarchs[BLACK] > MAX_MONARCHS) {
    fen_error(fen, c_count, "Too many Black Monarchs");
    return 1;
  }

  char c;
  bool done = false;
  // Look for color to move and set ply accordingly
  while (!done && (c = fen[c_count++]) != '\0') {
    switch (c) {
      // ignore whitespace until the end
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        break;

      // White to move
      case 'W':
        p->ply = 0;
        done = true;
        break;

      // Black to move
      case 'B':
        p->ply = 1;
        done = true;
        break;

      default:
        fen_error(fen, c_count, "Must specify White (W) or Black (B) to move");
        return 1;
        break;
    }
  }

  // Look for last move, if it exists
  square_t lm_from_sq, lm_to_sq;
  rot_t lm_rot;
  if (get_sq_from_str(fen, &c_count, &lm_from_sq) != 0) {  // parse error
    return 1;
  }
  if (lm_from_sq == 0) {       // from-square of last move
    p->last_move = NULL_MOVE;  // no last move specified
    p->key = compute_zob_key(p);
    // update_piece_loc(p);
    return 0;
  }

  c = fen[c_count];

  switch (c) {
    case 'R':  // Check for simple rotations
      lm_to_sq = lm_from_sq;
      lm_rot = RIGHT;
      break;
    case 'U':
      lm_to_sq = lm_from_sq;
      lm_rot = UTURN;
      break;
    case 'L':
      lm_to_sq = lm_from_sq;
      lm_rot = LEFT;
      break;

    default:  // Not a rotation
      lm_rot = NONE;
      if (get_sq_from_str(fen, &c_count, &lm_to_sq) != 0) {
        return 1;
      }
      break;
  }
  p->last_move = move_of(EMPTY, lm_rot, lm_from_sq, lm_to_sq);
  p->key = compute_zob_key(p);
  return 0;  // everything is okay
}

// Monarch orientations
// This code is duplicated from move_gen.c
static const char* monarch_ori_to_rep[2][NUM_ORI] = {{"NN", "EE", "SS", "WW"},
                                                     {"nn", "ee", "ss", "ww"}};

// Pawn orientations
static const char* pawn_ori_to_rep[2][NUM_ORI] = {{"NW", "NE", "SE", "SW"},
                                                  {"nw", "ne", "se", "sw"}};

// Translate a position struct into a fen string
// NOTE: When you use the test framework in search.c, you should modify this
// function to match your optimized board representation in move_gen.c
//
// Input:   (populated) position struct
//          empty string where FEN characters will be written
// Output:  null
void pos_to_fen(position_t* p, char* fen) {
  int pos = 0;
  int i;

  for (rnk_t r = BOARD_WIDTH - 1; r >= 0; --r) {
    int empty_in_a_row = 0;
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);
      ptype_t sq_type = ptype_of(p->board[sq]);

      if (sq_type == INVALID) {     // invalid square
        tbassert(false, "Bad news, yo.\n");  // This is bad!
      }

      if (sq_type == EMPTY) {  // empty square
        empty_in_a_row++;
        continue;
      } else {
        if (empty_in_a_row) {
          fen[pos++] = '0' + empty_in_a_row;
        }
        empty_in_a_row = 0;

        color_t c = color_of(p->board[sq]);

        if (sq_type == MONARCH) {
          int ori = ori_of(p->board[sq]);
          for (i = 0; i < 2; i++) {
            fen[pos++] = monarch_ori_to_rep[c][ori][i];
          }
          continue;
        }

        if (sq_type == PAWN) {
          int ori = ori_of(p->board[sq]);
          for (i = 0; i < 2; i++) {
            fen[pos++] = pawn_ori_to_rep[c][ori][i];
          }
          continue;
        }
      }
    }
    // assert: for larger boards, we need more general solns
    tbassert(BOARD_WIDTH <= 10, "BOARD_WIDTH = %d\n", BOARD_WIDTH);
    if (empty_in_a_row == 10) {
      fen[pos++] = '1';
      fen[pos++] = '0';
    } else if (empty_in_a_row) {
      fen[pos++] = '0' + empty_in_a_row;
    }
    if (r) {
      fen[pos++] = '/';
    }
  }

  // Set the turn
  fen[pos++] = ' ';
  if (p->ply % 2 == 0) {
    fen[pos++] = 'W';
  } else {
    fen[pos++] = 'B';
  }
  fen[pos++] = '\0';
}
