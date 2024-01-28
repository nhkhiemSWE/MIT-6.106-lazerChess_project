// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "end_game.h"
#include "eval.h"
#include "fen.h"
#include "move_gen.h"
#include "search.h"
#include "tbassert.h"
#include "util.h"

static const char* color_strs[3] = {"White", "Black"};

const char* color_to_str(color_t c) { return color_strs[c]; }

// -----------------------------------------------------------------------------
// Piece getters and setters (including color, ptype, orientation)
// -----------------------------------------------------------------------------

// Gets the 1st or 2nd monarch of the given color, 0 Indexed: 0 => 1st, 1 => 2nd

//Use a bitboard representation for a quick look up
__attribute__((always_inline)) square_t get_monarch(position_t* p, color_t c, int num) {
  return p->monarch_loc[c][num];
}

// which color is moving next
__attribute__((always_inline)) color_t color_to_move_of(position_t* p) {
  if ((p->ply & 1) == 0) {
    return WHITE;
  } else {
    return BLACK;
  }
}

__attribute__((always_inline)) color_t opp_color(color_t c) {
  return c^1;
}

__attribute__((always_inline)) bool move_eq(move_t a, move_t b) {
  return a.typ == b.typ && a.rot == b.rot && a.from_sq == b.from_sq &&
         a.to_sq == b.to_sq;
}

__attribute__((always_inline)) bool sortable_move_seq(sortable_move_t a, sortable_move_t b) {
  return move_eq(a.mv, b.mv) && a.key == b.key;
}

__attribute__((always_inline)) sortable_move_t make_sortable(move_t mv) {
  sortable_move_t sortable;
  sortable.key = 0;
  sortable.mv = mv;
  return sortable;
}

__attribute__((always_inline)) bool qi_at_dest_is_higher(square_t src, square_t dest) {
  return (qi_at(dest) > qi_at(src));
}

// -----------------------------------------------------------------------------
// Piece orientation strings
// -----------------------------------------------------------------------------

// Monarch orientations
const char* monarch_ori_to_rep[2][NUM_ORI] = {{"NN", "EE", "SS", "WW"},
                                              {"nn", "ee", "ss", "ww"}};

// Pawn orientations
const char* pawn_ori_to_rep[2][NUM_ORI] = {{"NW", "NE", "SE", "SW"},
                                           {"nw", "ne", "se", "sw"}};

const char* nesw_to_str[NUM_ORI] = {"north", "east", "south", "west"};

// -----------------------------------------------------------------------------
// Board hashing
// -----------------------------------------------------------------------------

// Zobrist hashing
//
// https://www.chessprogramming.org/Zobrist_Hashing
//
// NOTE: Zobrist hashing uses piece_t as an integer index into to the zob table.
// So if you change your piece representation, you'll need to recompute what the
// old piece representation is when indexing into the zob table to get the same
// node counts.
#define ZOB_ARR_SIZE (16*16)
static uint64_t zob[ZOB_ARR_SIZE][1 << PIECE_INDEX_SIZE];
static uint64_t zob_color;
uint64_t myrand();

uint64_t compute_zob_key(position_t* p) {
  uint64_t key = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t sq = square_of(f, r);
      key ^= zob[sq][(int) p->board[sq]];
    }
  }
  if (color_to_move_of(p) == BLACK) {
    key ^= zob_color;
  }

  return key;
}

//ADDED functions:
void update_piece_loc(position_t* p) {
  uint64_t key_white = 0;
  uint64_t key_black = 0;
  for (fil_t file = 0; file < BOARD_WIDTH; file ++) {
    for (rnk_t rnk = 0; rnk < BOARD_WIDTH; rnk ++) {
      square_t sq = square_of(file, rnk);
      piece_t piece = p->board[sq];
      if (piece) {
        if (color_of(piece) == WHITE) {
          key_white |= sq_to_bitmask[sq];
        } else {
          key_black |= sq_to_bitmask[sq];
        }
      }
    }
  }
  p->piece_loc[WHITE] = key_white;
  p->piece_loc[BLACK] = key_black;
}

void init_zob() {
  for (int i = 0; i < BOARD_SIZE; i++) {
    for (int j = 0; j < (1 << PIECE_INDEX_SIZE); j++) {
      zob[i][j] = myrand();
    }
  }
  zob_color = myrand();
}

// -----------------------------------------------------------------------------
// Constant tables
// -----------------------------------------------------------------------------

static uint8_t qi_table[ARR_SIZE] = { 
0, 0, 0, 8.0, 16.0, 16.0, 8.0, 0, 0, 0,  
0, 0.0, 24.0, 40.0, 48.0, 48.0, 40.0, 24.0, 0.0, 0, 
0, 24.0, 48.0, 64.0, 72.0, 72.0, 64.0, 48.0, 24.0, 0, 
8.0, 40.0, 64.0, 80.0, 88.0, 88.0, 80.0, 64.0, 40.0, 8.0, 
16.0, 48.0, 72.0, 88.0, 96.0, 96.0, 88.0, 72.0, 48.0, 16.0,
16.0, 48.0, 72.0, 88.0, 96.0, 96.0, 88.0, 72.0, 48.0, 16.0, 
8.0, 40.0, 64.0, 80.0, 88.0, 88.0, 80.0, 64.0, 40.0, 8.0,
0, 24.0, 48.0, 64.0, 72.0, 72.0, 64.0, 48.0, 24.0, 0, 
0, 0.0, 24.0, 40.0, 48.0, 48.0, 40.0, 24.0, 0.0, 0, 
0, 0, 0, 8.0, 16.0, 16.0, 8.0, 0, 0, 0};
static int8_t fil_table[ARR_SIZE] = { 
-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 
0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,  
1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 
2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 
3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 
4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 
5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 
6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, 6.0,
7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, 
8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, 8.0,
};
static int8_t rnk_table[ARR_SIZE] = { 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
-1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 
};
static const uint8_t f_r_to_sq[BOARD_WIDTH][BOARD_WIDTH] = {
  {11, 12, 13, 14, 15, 16, 17, 18},
  {21, 22, 23, 24, 25, 26, 27, 28},
  {31, 32, 33, 34, 35, 36, 37, 38},
  {41, 42, 43, 44, 45, 46, 47, 48},
  {51, 52, 53, 54, 55, 56, 57, 58},
  {61, 62, 63, 64, 65, 66, 67, 68},
  {71, 72, 73, 74, 75, 76, 77, 78},
  {81, 82, 83, 84, 85, 86, 87, 88},
};


__attribute__((always_inline)) uint8_t qi_at(square_t sq) {
  uint8_t qi = qi_table[sq];
  DEBUG_LOG(1, "Qi at square %d is %d\n", sq, qi);
  return qi;
}

// -----------------------------------------------------------------------------
// Squares
// -----------------------------------------------------------------------------

// For no square, use 0, which is guaranteed to be off board
__attribute__((always_inline)) square_t square_of(fil_t f, rnk_t r) {
  return f_r_to_sq[f][r];
}

// Finds file of square
__attribute__((always_inline)) fil_t fil_of(square_t sq) {
  return fil_table[sq];
}

// Finds rank of square
__attribute__((always_inline)) rnk_t rnk_of(square_t sq) {
  return rnk_table[sq];  
}

// converts a square to string notation, returns number of characters printed
int square_to_str(square_t sq, char* buf, size_t bufsize) {
  fil_t f = fil_of(sq);
  rnk_t r = rnk_of(sq);
  if (f >= 0) {
    return snprintf(buf, bufsize, "%c%d", 'a' + f, r);
  } else {
    return snprintf(buf, bufsize, "%c%d", 'z' + f + 1, r);
  }
}

// -----------------------------------------------------------------------------
// Board direction and laser direction
// -----------------------------------------------------------------------------

// direction map

__attribute__((always_inline)) int dir_of(compass_t d) {
  tbassert(d >= 0 && d < NUM_DIR, "d: %d\n", d);
  return dir[d];
}

// directions for laser: NN, EE, SS, WW
static int beam[NUM_ORI] = {NORTH, EAST, SOUTH, WEST};

__attribute__((always_inline)) int beam_of(int direction) {
  tbassert(direction >= 0 && direction < NUM_ORI, "dir: %d\n", direction);
  return beam[direction];
}

// reflect[beam_dir][pawn_orientation]
// sentinel -1 indicates back of Pawn
int reflect[NUM_ORI][NUM_ORI] = {
    // NW  NE  SE  SW
    {-1, -1, EE, WW},  // NN
    {NN, -1, -1, SS},  // EE
    {WW, EE, -1, -1},  // SS
    {-1, NN, SS, -1}   // WW
};

__attribute__((always_inline)) int reflect_of(int beam_dir, int pawn_ori) {
  tbassert(beam_dir >= 0 && beam_dir < NUM_ORI, "beam-dir: %d\n", beam_dir);
  tbassert(pawn_ori >= 0 && pawn_ori < NUM_ORI, "pawn-ori: %d\n", pawn_ori);
  return reflect[beam_dir][pawn_ori];
}

// -----------------------------------------------------------------------------
// Move getters and setters
// -----------------------------------------------------------------------------

__attribute__((always_inline)) move_t move_of(ptype_t typ, rot_t rot, square_t from_sq, square_t to_sq) {
  move_t mv;
  mv.typ = typ;
  mv.rot = rot;
  mv.from_sq = from_sq;
  mv.to_sq = to_sq;
  return mv;
}

// converts a move to string notation for FEN
void move_to_str(move_t mv, char* buf, size_t bufsize) {
  square_t f = mv.from_sq;  // from-square
  square_t t = mv.to_sq;    // to-square
  rot_t r = mv.rot;         // rotation
  const char* orig_buf = buf;

  buf += square_to_str(f, buf, bufsize);
  if (f != t) {
    buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
  } else {
    switch (r) {
      case NONE:
        buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
        break;
      case RIGHT:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "R");
        break;
      case UTURN:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "U");
        break;
      case LEFT:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "L");
        break;
      default:
        tbassert(false, "Whoa, now.  Whoa, I say.\n");  // Bad, bad, bad
        break;
    }
  }
}

// -----------------------------------------------------------------------------
// Move generation
// -----------------------------------------------------------------------------

// Generate all moves from position p.  Returns number of moves.
//
// https://www.chessprogramming.org/Move_Generation
int generate_all(position_t* p, sortable_move_t* sortable_move_list) {
  color_t color_to_move = color_to_move_of(p);
  uint64_t pieces = p->piece_loc[color_to_move];
  int move_count = 0;

      while (pieces) {
        uint64_t loc = pieces & (-pieces);//remove the least significant 1
        square_t sq = sq_to_bit_index[__builtin_ctzll(loc)];
        piece_t piece = p->board[sq];
        ptype_t typ = ptype_of(piece);
        // directions
        for (int d = 0; d < (int)NUM_DIR; d++) {
          compass_t dir = (compass_t)d;
          int dest = sq + dir_of(dir);
          ptype_t dest_type = ptype_of(p->board[dest]);
          if (dest_type == INVALID) continue;
          if (dest_type == MONARCH) continue;
          if (dest_type != EMPTY &&
              ptype_of(p->board[sq]) == dest_type &&
              qi_at_dest_is_higher(sq, dest))
            continue;

          WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
          WHEN_DEBUG_VERBOSE({
            move_to_str(move_of(typ, (rot_t)0, sq, dest), buf,
                        MAX_CHARS_IN_MOVE);
            DEBUG_LOG(1, "Before: %s ", buf);
          });
          tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);

          move_t mv = move_of(typ, (rot_t)0, sq, dest);
          sortable_move_list[move_count++] = make_sortable(mv);

          WHEN_DEBUG_VERBOSE({
            move_to_str(get_move(sortable_move_list[move_count - 1]), buf,
                        MAX_CHARS_IN_MOVE);
            DEBUG_LOG(1, "After: %s\n", buf);
          });
        }

        // For all non-360-degree rotations (i.e., not rot==0, which is NONE)
        for (int rot = 1; rot < NUM_ROT; ++rot) {
          tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
          move_t mv = move_of(typ, (rot_t)rot, sq, sq);
          sortable_move_list[move_count++] = make_sortable(mv);
        }
        pieces ^= loc; 
      }
  //   }
  // }

  // null move
  if (fire_lasers(p, color_to_move)) {
    move_t mv;
    square_t monarch_0 = p->monarch_loc[color_to_move][0];
    square_t monarch_1 = p->monarch_loc[color_to_move][1];
    if (ptype_of(p->board[monarch_0]) == MONARCH)
      mv = move_of(MONARCH, (rot_t)0, monarch_0, monarch_0);
    else
      mv = move_of(MONARCH, (rot_t)0, monarch_1, monarch_1);
    sortable_move_list[move_count++] = make_sortable(mv);
  }

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "\nGenerated moves: ");
    for (int i = 0; i < move_count; ++i) {
      char buf[MAX_CHARS_IN_MOVE];
      move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "%s ", buf);
    }
    DEBUG_LOG(1, "\n");
  });

  return move_count;
}

int generate_all_with_color(position_t* p, sortable_move_t* sortable_move_list,
                            color_t color) {
  color_t color_to_move = color_to_move_of(p);
  if (color_to_move != color) {
    p->ply++;
  }

  int num_moves = generate_all(p, sortable_move_list);

  if (color_to_move != color) {
    p->ply--;
  }

  return num_moves;
}

// -----------------------------------------------------------------------------
// Move execution
// -----------------------------------------------------------------------------
// Returns the square of piece that would be zapped by the laser if fired once,
// or 0 if no such piece exists.
//
// p : Current board state.
// c : Color of monarch shooting laser.
// n : Monarch ID
static const uint64_t ray_in_rank[8] = {0x0101010101010101, 0x0202020202020202, 0x0404040404040404,
                                        0x0808080808080808, 0x1010101010101010, 0x2020202020202020,
                                        0x4040404040404040, 0x8080808080808080};
static const uint64_t ray_in_file[8] = {0x00000000000000ff, 0x000000000000ff00, 0x0000000000ff0000,
                                        0x00000000ff000000, 0x000000ff00000000, 0x0000ff0000000000,
                                        0x00ff000000000000, 0xff00000000000000};   

square_t fire_laser(position_t* p, square_t sq) {
  if (ptype_of(p->board[sq]) == EMPTY || ptype_of(p->board[sq]) == INVALID)
    return 0;
  // uint64_t shot = sq_to_bitmask[sq];
  uint64_t pieces = p->piece_loc[WHITE] | p->piece_loc[BLACK];
  int bdir = ori_of(p->board[sq]);
  tbassert(ptype_of(p->board[sq]) == MONARCH, "ptype: %d\n", ptype_of(p->board[sq]));

  while (true) {
    uint64_t ray;
    uint64_t shot = sq_to_bitmask[sq];
    if (bdir & 0b01) {
      //Shooting North or South
      ray = ray_in_rank[rnk_of(sq)];
    } else {
      //Shooting West or East
      ray = ray_in_file[fil_of(sq)];
    }
    if (bdir & 0b10) {
      //Shooting in negative direction (towards lower files/ranks)
      ray &= (shot-1);
    } else {
      //Shooting in positive direction (towards higher files/ranks)
      ray &= -(shot*2);
    }
    
    uint64_t hit = ray & pieces;
    if (hit == 0) {
      return 0;
    }

    if (bdir & 0b10) {
      //Shooting south or west, get the most-significant bit
      sq = sq_to_bit_index[63 - __builtin_clzl(hit)] ;
    } else {
      //Shooting north or east, get the least-significant bit
      sq = sq_to_bit_index[__builtin_ctzl(hit)];
    }
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);
    piece_t piece = p->board[sq];
    if (ptype_of(piece) == PAWN) {
      bdir = reflect_of(bdir, ori_of(piece));
      if (bdir < 0) {
        return sq;
      }
    } else {
      return sq;
    }
  }
}

// returns number of victims
__attribute__((always_inline)) square_t fire_lasers(position_t* p, color_t c) {
  square_t sq = get_monarch(p, c, 0);
  int victims = 0;
  if (ptype_of(p->board[sq]) == MONARCH)
    if (fire_laser(p, sq)) victims++;
  sq = get_monarch(p, c, 1);
  if (ptype_of(p->board[sq]) == MONARCH)
    if (fire_laser(p, sq)) victims++;
  return victims;
}

void low_level_make_move(position_t* old, position_t* p, move_t mv) {
  tbassert(!move_eq(mv, NULL_MOVE), "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
  WHEN_DEBUG_VERBOSE({
    move_to_str(mv, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "low_level_make_move: %s\n", buf);
  });

  tbassert(old->key == compute_zob_key(old),
           "old->key: %" PRIu64 ", zob-key: %" PRIu64 "\n", old->key,
           compute_zob_key(old));

  WHEN_DEBUG_VERBOSE({
    fprintf(stderr, "Before:\n");
    display(old);
  });

  square_t from_sq = mv.from_sq;
  square_t to_sq = mv.to_sq;
  int dir = to_sq - from_sq;
  square_t next_sq = from_sq + dir + dir;
  rot_t rot = mv.rot;

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "low_level_make_move 2:\n");
    square_to_str(from_sq, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "from_sq: %s\n", buf);
    square_to_str(to_sq, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "to_sq: %s\n", buf);
    square_to_str(next_sq, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "next_sq: %s\n", buf);
    switch (rot) {
      case NONE:
        DEBUG_LOG(1, "rot: none\n");
        break;
      case RIGHT:
        DEBUG_LOG(1, "rot: R\n");
        break;
      case UTURN:
        DEBUG_LOG(1, "rot: U\n");
        break;
      case LEFT:
        DEBUG_LOG(1, "rot: L\n");
        break;
      default:
        tbassert(false, "Not like a boss at all.\n");  // Bad, bad, bad
        break;
    }
  });

  *p = *old;
  p->history = old;
  p->last_move = mv;

  tbassert(from_sq < ARR_SIZE && from_sq > 0, "from_sq: %d\n", from_sq);
  tbassert((p->board[from_sq]) < (1 << PIECE_INDEX_SIZE) &&
               (p->board[from_sq]) >= 0,
           "p->board[from_sq]: %d\n", (p->board[from_sq]));
  tbassert(to_sq < ARR_SIZE && to_sq > 0, "to_sq: %d\n", to_sq);
  tbassert((p->board[to_sq]) < (1 << PIECE_INDEX_SIZE) &&
               (p->board[to_sq]) >= 0,
           "p->board[to_sq]: %d\n", (p->board[to_sq]));

  p->key ^= zob_color;  // swap color to move

  piece_t from_piece = p->board[from_sq];
  piece_t to_piece = p->board[to_sq];
  piece_t next_piece = p->board[next_sq];

  if (to_sq != from_sq) {  // move, not rotation
    // Hash key updates
    p->key ^= zob[from_sq]
                 [(from_piece)];  // remove from_piece from from_sq
    p->key ^= zob[to_sq][(to_piece)];  // remove to_piece from to_sq

    p->board[from_sq] = NULL_PIECE;
    p->key ^= zob[from_sq][0];

    color_t from_color = color_of(from_piece);
    color_t to_color = color_of(to_piece);
    
    // UPDATE:piece_loc
    // First, remove the from_piece and to_piece
    p->piece_loc[from_color] &= (~sq_to_bitmask[from_sq]); //CLEAR
    p->piece_loc[to_color]  &= (~sq_to_bitmask[to_sq]);

    int monarch_ptr = 0;
    if (ptype_of(from_piece) == MONARCH) {
      if (p->monarch_loc[from_color][monarch_ptr] == from_sq) {
        p->monarch_loc[from_color][monarch_ptr] = 0;
      } else {
        monarch_ptr = 1;
        p->monarch_loc[from_color][monarch_ptr] = 0;
      }
      p->monarch_loc[from_color][monarch_ptr] = to_sq;
    }

    if (ptype_of(next_piece) == EMPTY) {
      // empty
      p->board[to_sq] = from_piece;
      p->key ^= zob[to_sq][(from_piece)];
      p->board[next_sq] = to_piece;
      p->key ^= zob[next_sq][(next_piece)];
      p->key ^= zob[next_sq][(to_piece)];
      // CASE 1: next_sq is empty
      p->piece_loc[from_color] |= sq_to_bitmask[to_sq]; //ADD
      if (ptype_of(to_piece) == PAWN || ptype_of(to_piece) == MONARCH) {
        p->piece_loc[to_color] |= sq_to_bitmask[next_sq]; //ADD
      }

    } else {
      // off board or occupied
      p->board[to_sq] = from_piece;
      p->key ^= zob[to_sq][(from_piece)];
      //CASE 2: next_sq is off board or occupied
      p->piece_loc[from_color] |= sq_to_bitmask[to_sq];
    }

  } else {  // rotation
    // remove from_piece from from_sq in hash
    p->key ^= zob[from_sq][from_piece];
    set_ori(&from_piece, (uint8_t)((rot + ori_of(from_piece)) % NUM_ORI));
    p->board[from_sq] = from_piece;  // place rotated piece on board
    p->key ^= zob[from_sq][from_piece];  // ... and in hash
  }

  // Increment ply
  p->ply++;

  tbassert(p->key == compute_zob_key(p),
           "p->key: %" PRIu64 ", zob-key: %" PRIu64 "\n", p->key,
           compute_zob_key(p));

  WHEN_DEBUG_VERBOSE({
    fprintf(stderr, "After:\n");
    display(p);
  });
}

bool do_pawns_touch(pawn_ori_t ori_p1, pawn_ori_t ori_p2, compass_t dir) {
  tbassert((0 <= ori_p1) && (ori_p1 < NUM_ORI), "Bad p1 orientation! %d\n",
           ori_p1);
  tbassert((0 <= ori_p2) && (ori_p2 < NUM_ORI), "Bad p2 orientation! %d\n",
           ori_p2);
  tbassert((0 <= dir) && (dir < NUM_DIR), "Bad direction! %d\n", dir);

  switch (dir) {
    case dirNW:
      return ori_p1 != NW && ori_p2 != SE;
    case dirNE:
      return ori_p1 != NE && ori_p2 != SW;
    case dirN:
      return !((ori_p1 == NW && ori_p2 == SE) ||
               (ori_p1 == NE && ori_p2 == SW));
    case dirW:
      return !((ori_p1 == NW && ori_p2 == SE) ||
               (ori_p1 == SW && ori_p2 == NE));
    case dirE:
      return do_pawns_touch(ori_p2, ori_p1, dirW);
    case dirSW:
      return do_pawns_touch(ori_p2, ori_p1, dirNE);
    case dirS:
      return do_pawns_touch(ori_p2, ori_p1, dirN);
    case dirSE:
      return do_pawns_touch(ori_p2, ori_p1, dirNW);

    default:
      tbassert(false, "Lost your sense of direction, eh?");
      return false;
  }
}

// -----------------------------------------------------------------------------
// Victims update
// -----------------------------------------------------------------------------

// returns victim pieces (does not mark position as actually played)
victims_t make_move(position_t* old, position_t* p, move_t mv) {
  tbassert(!move_eq(mv, NULL_MOVE), "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);

  // move phase 1 - moving a piece
  low_level_make_move(old, p, mv);
  p->victims.count = 0;
  p->victims.removed_color[WHITE] = false;
  p->victims.removed_color[BLACK] = false;
  square_t victim_sq1 = 0;
  square_t victim_sq2 = 0;
  color_t previous_color = color_to_move_of(old);
  square_t monarch_0 = get_monarch(p, previous_color, 0);
  square_t monarch_1 = get_monarch(p, previous_color, 1);
  victim_sq1 = fire_laser(p, monarch_0);
  victim_sq2 = fire_laser(p, monarch_1);

  if (victim_sq1) {
    WHEN_DEBUG_VERBOSE({
      square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(3, "Zapping piece on %s\n", buf);
    });
    // We definitely hit something with the laser.  Remove it from the board.
      piece_t piece = p->board[victim_sq1];
      WHEN_DEBUG_VERBOSE({
        fprintf(stdout, "Before:\n");
        display(p);
      });
      // remove victim
      tbassert(victim_sq1 > 0 && victim_sq1 < ARR_WIDTH * ARR_WIDTH, "Bad square! %d\n", victim_sq1);
      p->key ^= zob[victim_sq1][piece];
      p->board[victim_sq1] = NULL_PIECE;
      p->key ^= zob[victim_sq1][0];
      color_t c = color_of(piece);
      p->piece_loc[c] &= (~sq_to_bitmask[victim_sq1]); //CLEAR
      
      if (ptype_of(piece) == MONARCH) {
        if (p->monarch_loc[c][0] == victim_sq1) {
          p->monarch_loc[c][0] = 0;
        } else {
          p-> monarch_loc[c][1] = 0;
        }
      }

      tbassert(p->key == compute_zob_key(p),
              "p->key: %" PRIu64 ", zob-key: %" PRIu64 "\n", p->key,
              compute_zob_key(p));
      WHEN_DEBUG_VERBOSE({
        fprintf(stdout, "After:\n");
        display(p);
      });
    p->victims.count++;
    p->victims.removed_color[c] = true;
  }

  if (victim_sq2) {
    WHEN_DEBUG_VERBOSE({
      square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(3, "Zapping piece on %s\n", buf);
    });
    piece_t piece = p->board[victim_sq2];
    WHEN_DEBUG_VERBOSE({
      fprintf(stdout, "Before:\n");
      display(p);
    });
    tbassert(victim_sq2 > 0 && victim_sq2 < ARR_WIDTH * ARR_WIDTH, "Bad square! %d\n", victim_sq2);
    p->key ^= zob[victim_sq2][piece];
    p->board[victim_sq2] = NULL_PIECE;
    p->key ^= zob[victim_sq2][0];
    color_t c = color_of(piece);
    p->piece_loc[c] &= (~sq_to_bitmask[victim_sq2]); //CLEAR
    
    if (ptype_of(piece) == MONARCH) {
      if (p->monarch_loc[c][0] == victim_sq2) {
        p->monarch_loc[c][0] = 0;
      } else {
        p-> monarch_loc[c][1] = 0;
      }
    }

    tbassert(p->key == compute_zob_key(p),
            "p->key: %" PRIu64 ", zob-key: %" PRIu64 "\n", p->key,
            compute_zob_key(p));
    WHEN_DEBUG_VERBOSE({
      fprintf(stdout, "After:\n");
      display(p);
    });
    // We definitely hit something with the laser.  Remove it from the board.
    p->victims.count++;
    p->victims.removed_color[c] = true;
  }

  piece_t to_piece = old->board[mv.to_sq];
  if (ptype_of(to_piece) == PAWN) {
    // Check if we squashed the piece at mv.to_sq: is the cell it's pushed to empty?
    int dir = mv.to_sq - mv.from_sq;
    square_t pushed_next_sq = mv.to_sq + dir;
    if (mv.to_sq != mv.from_sq && ptype_of(old->board[pushed_next_sq]) != EMPTY) {
      p->victims.count++;
      p->victims.removed_color[color_of(to_piece)] = true;
    }
  }

  if (p->victims.count == 0) {
    // increment ply counter since last victim
    p->nply_since_victim = old->nply_since_victim + 1;
  } else {
    // reset ply counter since last victim
    p->nply_since_victim = 0;
  }
  // mark position as not played (yet)
  p->was_played = false;
  return p->victims;
}

victims_t actually_make_move(position_t* old, position_t* p, move_t mv) {
  make_move(old, p, mv);
  p->was_played = true;
  return p->victims;
}

// -----------------------------------------------------------------------------
// Move path enumeration (perft)
// -----------------------------------------------------------------------------

// Helper function for do_perft() (for root call, use ply 0).
static uint64_t perft_search(position_t* p, int depth, int ply) {
  uint64_t node_count = 0;
  position_t np;
  sortable_move_t lst[MAX_NUM_MOVES];
  int num_moves;
  int i;

  if (depth == 0) {
    return 1;
  }

  num_moves = generate_all(p, lst);

  if (depth == 1) {
    return num_moves;
  }

  for (i = 0; i < num_moves; i++) {
    move_t mv = get_move(lst[i]);
    make_move(p, &np, mv);

    if (is_game_over(&np)) {
      // do not expand further: hit a Monarch
      node_count++;
      continue;
    }

    uint64_t partialcount = perft_search(&np, depth - 1, ply + 1);
    node_count += partialcount;
  }

  return node_count;
}

// Debugging function to perform a sanity check that the move generator is
// working correctly.  Not a thorough test, but quick.
//
// https://www.chessprogramming.org/Perft
void do_perft(position_t* gme, int depth) {
  for (int d = 0; d <= depth; d++) {
    printf("info perft %2d ", d);
    uint64_t j = perft_search(gme, d, 0);
    printf("%" PRIu64 "\n", j);
  }
}

// -----------------------------------------------------------------------------
// Position display
// -----------------------------------------------------------------------------

void display(position_t* p) {
  char buf[MAX_CHARS_IN_MOVE];

  printf("info Ply: %d\n", p->ply);
  printf("info Color to move: %s\n", color_to_str(color_to_move_of(p)));

  square_to_str(get_monarch(p, WHITE, 0), buf, MAX_CHARS_IN_MOVE);
  printf("info White Monarch: %s\n", buf);
  square_to_str(get_monarch(p, WHITE, 1), buf, MAX_CHARS_IN_MOVE);
  printf("info White Monarch: %s\n", buf);
  square_to_str(get_monarch(p, BLACK, 0), buf, MAX_CHARS_IN_MOVE);
  printf("info Black Monarch: %s\n", buf);
  square_to_str(get_monarch(p, BLACK, 1), buf, MAX_CHARS_IN_MOVE);
  printf("info Black Monarch: %s\n", buf);
  printf("info");
  for (rnk_t r = BOARD_WIDTH - 1; r >= 0; --r) {
    printf("\ninfo %1d ", r);
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);
      ptype_t sq_type = ptype_of(p->board[sq]);
      if (sq_type == INVALID) printf("%d \n", sq);
      tbassert(sq_type != INVALID, "p->board[sq].typ: %d\n",
               sq_type);
      if (sq_type == EMPTY) {  // empty square
        printf(" --");
        continue;
      }

      color_t c = color_of(p->board[sq]);

      if (sq_type == MONARCH) {
        int ori = (int) ori_of(p->board[sq]);
        printf(" %2s", monarch_ori_to_rep[c][ori]);
        continue;
      }

      if (sq_type == PAWN) {
        int ori = (int) ori_of(p->board[sq]);
        printf(" %2s", pawn_ori_to_rep[c][ori]);
        continue;
      }
    }
  }

  printf("\ninfo    ");
  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    printf(" %c ", 'a' + f);
  }
  printf("\n");
  printf("DoneDisplay\n");  // DO NOT DELETE THIS LINE!!! It is needed for the
                            // autotester.
  printf("\n");
}

void bitboardDisplay(position_t* p) {
  char buf[MAX_CHARS_IN_MOVE];

    printf("info Ply: %d\n", p->ply);
    printf("info Color to move: %s\n", color_to_str(color_to_move_of(p)));

    square_to_str(get_monarch(p, WHITE, 0), buf, MAX_CHARS_IN_MOVE);
    printf("info White Monarch: %s\n", buf);
    square_to_str(get_monarch(p, WHITE, 1), buf, MAX_CHARS_IN_MOVE);
    printf("info White Monarch: %s\n", buf);
    square_to_str(get_monarch(p, BLACK, 0), buf, MAX_CHARS_IN_MOVE);
    printf("info Black Monarch: %s\n", buf);
    square_to_str(get_monarch(p, BLACK, 1), buf, MAX_CHARS_IN_MOVE);
    printf("info Black Monarch: %s\n", buf);
    printf("info");
    for (rnk_t r = BOARD_WIDTH - 1; r >= 0; --r) {
      printf("\ninfo %1d ", r);
      for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
        square_t sq = square_of(f, r);
        if (p->piece_loc[WHITE] & (sq_to_bitmask[sq])) {
          printf("  0");
        }

        else if (p->piece_loc[BLACK] & (sq_to_bitmask[sq])) {
          printf("  1");
        }

        else {
          printf(" --");
        }
      }
    }

    printf("\ninfo    ");
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      printf(" %c ", 'a' + f);
    }
    printf("\n");
    printf("DoneDisplay\n");  // DO NOT DELETE THIS LINE!!! It is needed for the
                              // autotester.
    printf("\n");
}

// -----------------------------------------------------------------------------
// Illegal move signalling
// -----------------------------------------------------------------------------

victims_t ILLEGAL() {
  victims_t v;
  v.count = ILLEGAL_ZAPPED;
  return v;
}

bool is_ILLEGAL(victims_t victims) { return (victims.count == ILLEGAL_ZAPPED); }

bool zero_victims(victims_t victims) { return (victims.count == 0); }

bool victim_exists(victims_t victims) { return (victims.count > 0); }
