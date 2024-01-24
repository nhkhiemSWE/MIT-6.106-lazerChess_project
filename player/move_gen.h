// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

// The MAX_NUM_MOVES is just an estimate
#define MAX_NUM_MOVES 256      // real number = 8 * (11) + 1 = 89
#define MAX_PLY_IN_SEARCH 100  // up to 100 ply
#define MAX_PLY_IN_GAME 4096   // long game!  ;^)

// Used for debugging and display
#define MAX_CHARS_IN_MOVE 16  // Could be less
#define MAX_CHARS_IN_TOKEN 64

// -----------------------------------------------------------------------------
// Board
// -----------------------------------------------------------------------------

// The board is centered in a ARR_WIDTH x ARR_WIDTH array, with the
// (ample) excess height and width being used for sentinels.
#define ARR_WIDTH 10
#define ARR_SIZE (ARR_WIDTH * ARR_WIDTH)
#define MAX_NUM_PAWNS_PER_COLOR 6

// Board is BOARD_WIDTH x BOARD_WIDTH
#define BOARD_WIDTH 8
#define BOARD_SIZE (BOARD_WIDTH*BOARD_WIDTH)

typedef uint8_t square_t;
typedef int8_t rnk_t;
typedef int8_t fil_t;

#define FIL_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)
#define RNK_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)

#define FIL_SHIFT 4
#define FIL_MASK 0xF
#define RNK_SHIFT 0
#define RNK_MASK 0xF

static const uint64_t mailbox[100] = {
0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
0ULL, 1ULL<<0, 1ULL<<1, 1ULL<<2, 1ULL<<3, 1ULL<<4, 1ULL<<5, 1ULL<<6, 1ULL<<7, 0ULL,
0ULL, 1ULL<<8, 1ULL<<9, 1ULL<<10, 1ULL<<11, 1ULL<<12, 1ULL<<13, 1ULL<<14, 1ULL<<15, 0ULL,
0ULL, 1ULL<<16, 1ULL<<17, 1ULL<<18, 1ULL<<19, 1ULL<<20, 1ULL<<21, 1ULL<<22, 1ULL<<23, 0ULL,
0ULL, 1ULL<<24, 1ULL<<25, 1ULL<<26, 1ULL<<27, 1ULL<<28, 1ULL<<29, 1ULL<<30, 1ULL<<31, 0ULL,
0ULL, 1ULL<<32, 1ULL<<33, 1ULL<<34, 1ULL<<35, 1ULL<<36, 1ULL<<37, 1ULL<<38, 1ULL<<39, 0ULL,
0ULL, 1ULL<<40, 1ULL<<41, 1ULL<<42, 1ULL<<43, 1ULL<<44, 1ULL<<45, 1ULL<<46, 1ULL<<47, 0ULL,
0ULL, 1ULL<<48, 1ULL<<49, 1ULL<<50, 1ULL<<51, 1ULL<<52, 1ULL<<53, 1ULL<<54, 1ULL<<55, 0ULL,
0ULL, 1ULL<<56, 1ULL<<57, 1ULL<<58, 1ULL<<59, 1ULL<<60, 1ULL<<61, 1ULL<<62, 1ULL<<63, 0ULL,
0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};

static const int mailbox64[64] = {
  11, 12, 13, 14, 15, 16, 17, 18,
  21, 22, 23, 24, 25, 26, 27, 28,
  31, 32, 33, 34, 35, 36, 37, 38,
  41, 42, 43, 44, 45, 46, 47, 48,
  51, 52, 53, 54, 55, 56, 57, 58,
  61, 62, 63, 64, 65, 66, 67, 68,
  71, 72, 73, 74, 75, 76, 77, 78,
  81, 82, 83, 84, 85, 86, 87, 88,
};

// -----------------------------------------------------------------------------
// Pieces
// -----------------------------------------------------------------------------

typedef uint8_t piece_t;
#define NULL_PIECE 0

#define MAX_PAWNS (BOARD_WIDTH * BOARD_WIDTH)  // maximum number of Pawns total
#define MAX_MONARCHS 2  // maximum number of Monarchs per color

#define PIECE_INDEX_SIZE 5  // Width of index returned by piece_index()

// -----------------------------------------------------------------------------
// Piece types
// -----------------------------------------------------------------------------

typedef enum { EMPTY, PAWN, MONARCH, INVALID } ptype_t;
#define PTYPE_SHIFT 2
#define PTYPE_MASK  3

// -----------------------------------------------------------------------------
// Colors
// -----------------------------------------------------------------------------

typedef enum { WHITE = 0, BLACK = 1 } color_t;
#define COLOR_SHIFT 4
#define COLOR_MASK  1

// -----------------------------------------------------------------------------
// Orientations
// -----------------------------------------------------------------------------

#define NUM_ORI 4
#define ORI_SHIFT 0
#define ORI_MASK  3

typedef enum { NN, EE, SS, WW } monarch_ori_t;
typedef enum { NW, NE, SE, SW } pawn_ori_t;

// -----------------------------------------------------------------------------
// Piece helper functions
// -----------------------------------------------------------------------------

//Getting the color of the piece
#define color_of(x) ((color_t)(((x) >> COLOR_SHIFT) & COLOR_MASK))
//Setting the color of the piece p to c
#define set_color(p,c)   *(p) = (((c) & COLOR_MASK) << COLOR_SHIFT) | (*(p) & ~(COLOR_MASK << COLOR_SHIFT))

//Getting the ptype of the piece p
#define ptype_of(p)      ((ptype_t) (((p) >> PTYPE_SHIFT) & PTYPE_MASK))
//Setting the ptype of the piece p to a new type pt
#define set_ptype(p, pt) *(p) = (((pt) & PTYPE_MASK) << PTYPE_SHIFT) | (*(p) & ~(PTYPE_MASK << PTYPE_SHIFT))

//Getting the orientation of the piece p
#define ori_of(p)        (((p) >> ORI_SHIFT) & ORI_MASK)
//Setting the orientation of the piece p to a new orientation ori
#define set_ori(p, ori)  *(p) = (((ori) & ORI_MASK) << ORI_SHIFT) | (*(p) & ~(ORI_MASK << ORI_SHIFT))

// -----------------------------------------------------------------------------
// Directions
// -----------------------------------------------------------------------------

typedef enum {
  dirNW,
  dirN,
  dirNE,
  dirE,
  dirSE,
  dirS,
  dirSW,
  dirW,
  NUM_DIR
} compass_t;

#define NORTH 1
#define SOUTH (-1)
#define EAST ARR_WIDTH
#define WEST (-ARR_WIDTH)

static const int dir[NUM_DIR] = {NORTH + WEST, NORTH, NORTH + EAST, EAST,
                                 SOUTH + EAST, SOUTH, SOUTH + WEST, WEST};

// -----------------------------------------------------------------------------
// Moves
// -----------------------------------------------------------------------------

// Rotations
#define NUM_ROT 4

typedef enum { NONE, RIGHT, UTURN, LEFT } rot_t;

typedef struct {
  uint8_t typ: 2;
  uint8_t rot: 2;
  square_t from_sq;
  square_t to_sq;
} move_t;

#define NULL_MOVE\
  (move_t) { (ptype_t)0, (rot_t)0, (square_t)0, (square_t)0 }

typedef uint32_t sort_key_t;
typedef struct {
  sort_key_t key;
  move_t mv;
} sortable_move_t;


// -----------------------------------------------------------------------------
// Victims
// -----------------------------------------------------------------------------

#define MAX_ZAPPED 15
typedef struct victims_t {
  int8_t count: 3;
  bool removed_color[2];
} victims_t;

// returned by make move in illegal situation
#define ILLEGAL_ZAPPED -1

// -----------------------------------------------------------------------------
// Position
// -----------------------------------------------------------------------------

// Board representation is square-centric with sentinels.
//
// https://www.chessprogramming.org/Board_Representation
// https://www.chessprogramming.org/Mailbox
// https://www.chessprogramming.org/10x12_Board

typedef struct position {
  piece_t board[ARR_SIZE];
  struct position* history;  // history of position
  uint64_t key;              // hash key
  int ply;                // ply since beginning of game (even=White, odd=Black)
  int nply_since_victim;  // number of ply since last capture
  move_t last_move;       // move that led to this position
  victims_t victims;      // pieces destroyed by shooter
  bool was_played;        // TRUE: position was actually played;
                          // FALSE: position is being considered in search
  uint64_t piece_loc[2];  // The location of all pieces of each color
  square_t monarch_loc[2][2];
  // uint64_t laser_map[2];  // The laser coverage map
  // uint8_t  killable[2];   // The number of pieces which can be zapped at the current position..
                          // ..in case the player want to make a null move
} position_t;

// -----------------------------------------------------------------------------
// Function prototypes
// -----------------------------------------------------------------------------

const char* color_to_str(color_t c);
__attribute__((always_inline)) color_t color_to_move_of(position_t* p);
__attribute__((always_inline)) color_t opp_color(color_t c);

void init_zob();
void print_qi_map();
uint64_t compute_zob_key(position_t* p);
void update_piece_loc(position_t* p);

__attribute__((always_inline)) square_t get_monarch(position_t* p, color_t c, int num) ;
__attribute__((always_inline)) square_t square_of(fil_t f, rnk_t r);
__attribute__((always_inline)) fil_t fil_of(square_t sq);
__attribute__((always_inline)) rnk_t rnk_of(square_t sq);
__attribute__((always_inline)) int square_to_str(square_t sq, char* buf, size_t bufsize);

__attribute__((always_inline)) int dir_of(compass_t d);
__attribute__((always_inline)) int beam_of(int direction);
__attribute__((always_inline)) int reflect_of(int beam_dir, int pawn_ori);

__attribute__((always_inline)) move_t move_of(ptype_t typ, rot_t rot, square_t from_sq, square_t to_sq);
void move_to_str(move_t mv, char* buf, size_t bufsize);
__attribute__((always_inline)) bool move_eq(move_t a, move_t b);
bool sortable_move_seq(sortable_move_t a, sortable_move_t b);
__attribute__((always_inline)) sortable_move_t make_sortable(move_t mv);
__attribute__((always_inline)) uint8_t qi_at(square_t sq);
__attribute__((always_inline)) bool qi_at_dest_is_higher(square_t src, square_t dest);


__attribute__((always_inline)) square_t fire_lasers(position_t* p, color_t c);
square_t fire_laser(position_t* p, square_t monarch_loc);
void mark_laser_map(position_t* p, color_t c, square_t sq);

int generate_all(position_t* p, sortable_move_t* sortable_move_list);
__attribute__((always_inline)) int generate_all_with_color(position_t* p, sortable_move_t* sortable_move_list,
                            color_t color_to_move);
void do_perft(position_t* gme, int depth);
void low_level_make_move(position_t* old, position_t* p, move_t mv);
victims_t make_move(position_t* old, position_t* p, move_t mv);

//For leiserchess.c
victims_t actually_make_move(position_t* old, position_t* p, move_t mv);
void display(position_t* p);
void bitboardDisplay(position_t* p);
void laserMapDisplay(position_t* p);

void laser_coverage_make_move(position_t* pos, move_t mv, piece_t* previous_pieces);
void laser_coverage_unmake_move(position_t *pos, move_t mv, piece_t *previous_pieces);

victims_t ILLEGAL();
bool is_ILLEGAL(victims_t victims);
bool zero_victims(victims_t victims);
bool victim_exists(victims_t victims);

bool do_pawns_touch(pawn_ori_t ori_p1, pawn_ori_t ori_p2, compass_t dir);
void update_victim(position_t* p, square_t victim_sq);
// static inline void add_removed_pawn(position_t* p, color_t c);
#endif  // MOVE_GEN_H
