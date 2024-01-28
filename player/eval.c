// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#include "eval.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "move_gen.h"
#include "tbassert.h"

// -----------------------------------------------------------------------------
// Evaluation
// -----------------------------------------------------------------------------
//Set to 0 to turn on Laser Coverage, set to 13 to turn off Laser Coverage
//Change value to modify when to turn on laser coverage
// #define PAWN_LIMIT 0

typedef int32_t ev_score_t;  // Static evaluator uses "hi res" values

int RANDOMIZE;

int PTOUCH_weight;
int PPROX_weight;
int MFACE_weight;
int MCEDE_weight;
int LCOVERAGE_weight;
int PMID_weight;
int MMID_weight;
int PMAT_weight = PAWN_EV_VALUE;
int RELQI_weight;
int ABSQI_weight;

enum heuristics_t {
  PTOUCH,
  PPROX,
  MFACE,
  MCEDE,
  LCOVERAGE,
  PMID,
  MMID,
  PMAT,
  RELQI,
  ABSQI,
  NUM_HEURISTICS
};
char* heuristic_strs[NUM_HEURISTICS] = {"PTOUCH",    "PPROX", "MFACE", "MCEDE",
                                        "LCOVERAGE", "PMID",  "MMID",  "PMAT",
                                        "RELQI",     "ABSQI"};
// Boolean array for heuristics that scale along floating point values
// (versus those that are directly proportional to pawn values)
bool floating_point_heuristics[NUM_HEURISTICS] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 1};

static const ev_score_t centrality_lookup_table[BOARD_SIZE] = {
0,1,2,3,3,2,1,0,
1,2,3,4,4,3,2,1,
2,3,4,5,5,4,3,2,
3,4,5,6,6,5,4,3,
3,4,5,6,6,5,4,3,
2,3,4,5,5,4,3,2,
1,2,3,4,4,3,2,1,
0,1,2,3,3,2,1,0
};

static const uint64_t neighbors[100] = {
  0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 
  0ULL, 770ULL, 1797ULL, 3594ULL, 7188ULL, 14376ULL, 28752ULL, 57504ULL, 49216ULL, 0ULL, 
  0ULL, 197123ULL, 460039ULL, 920078ULL, 1840156ULL, 3680312ULL, 7360624ULL, 14721248ULL, 12599488ULL, 0ULL, 
  0ULL, 50463488ULL, 117769984ULL, 235539968ULL, 471079936ULL, 942159872ULL, 1884319744ULL, 3768639488ULL, 3225468928ULL, 0ULL, 
  0ULL, 12918652928ULL, 30149115904ULL, 60298231808ULL, 120596463616ULL, 241192927232ULL, 482385854464ULL, 964771708928ULL, 825720045568ULL, 0ULL, 
  0ULL, 3307175149568ULL, 7718173671424ULL, 15436347342848ULL, 30872694685696ULL, 61745389371392ULL, 123490778742784ULL, 246981557485568ULL, 211384331665408ULL, 0ULL, 
  0ULL, 846636838289408ULL, 1975852459884544ULL, 3951704919769088ULL, 7903409839538176ULL, 15806819679076352ULL, 31613639358152704ULL, 63227278716305408ULL, 54114388906344448ULL, 0ULL, 
  0ULL, 216739030602088448ULL, 505818229730443264ULL, 1011636459460886528ULL, 2023272918921773056ULL, 4046545837843546112ULL, 8093091675687092224ULL, 16186183351374184448ULL, 13853283560024178688ULL, 0ULL, 
  0ULL, 144959613005987840ULL, 362258295026614272ULL, 724516590053228544ULL, 1449033180106457088ULL, 2898066360212914176ULL, 5796132720425828352ULL, 11592265440851656704ULL, 4665729213955833856ULL, 0ULL, 
  0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL
};

static const double inverse[16] = {1.0/1, 1.0/2, 1.0/3, 1.0/4, 1.0/5, 1.0/6, 1.0/7, 1.0/8, 1.0/9, 1.0/10, 1.0/11, 1.0/12, 1.0/13, 1.0/14, 1.0/15, 1.0/16};

// static const float delta_ratio_mult_dist_lookup[BOARD_WIDTH] = {1.0f / 1.0f, 1.0f / 2.0f, 1.0f / 3.0f, 1.0f / 4.0f, 1.0f / 5.0f, 1.0f / 6.0f, 1.0f / 7.0f, 1.0f / 8.0f};

// Heuristics for static evaluation - described in the leiserchess codewalk
// slides

// For PTOUCH heuristic: Does a Pawn touch a neighboring Pawn?
bool static inline __attribute__((always_inline)) p_touch(uint64_t pawns, square_t sq) {
  return pawns & neighbors[sq];
}

// test routine for do_pawns_touch()
void test_ptouch(position_t* p) {
  char sq_str[MAX_CHARS_IN_MOVE];

  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t sq = square_of(f, r);
      piece_t piece = p->board[sq];
      ptype_t piece_type = ptype_of(piece);

      if (piece_type == PAWN) {  // find a Pawn
        square_to_str(sq, sq_str, MAX_CHARS_IN_MOVE);

        for (int d = 0; d < NUM_DIR; d++) {  // search surrounding squares
          square_t curr_sq = sq + dir_of((compass_t)d);
          piece_t curr_piece = p->board[curr_sq];
          if (ptype_of(curr_piece) == PAWN) {
            if (do_pawns_touch((pawn_ori_t) ori_of(piece), (pawn_ori_t) ori_of(curr_piece),
                               (compass_t)d)) {
              square_to_str(curr_sq, sq_str, MAX_CHARS_IN_MOVE);
            }
          }
        }
      }
    }
  }
}

ev_score_t static inline __attribute__((always_inline)) mface_pair( piece_t piece, int8_t delta_fil, int8_t delta_rnk) {
  int bonus = 0;
  
  switch ((monarch_ori_t) ori_of(piece)) {
    case NN:
      bonus = delta_rnk;
      break;
    case EE:
      bonus = delta_fil;
      break;
    case SS:
      bonus = -delta_rnk;
      break;
    case WW:
      bonus = -delta_fil;
      break;
    default:
      tbassert(false, "Illegal Monarch orientation.\n");
  }
  return (bonus * PAWN_EV_VALUE) * inverse[(abs(delta_rnk) + abs(delta_fil)) - 1];
}

// MFACE heuristic: bonus (or penalty) for Monarch facing toward the other
// Monarch
ev_score_t static inline __attribute__((always_inline)) mface(position_t* p, piece_t piece, fil_t f, rnk_t r) {
  color_t c = color_of(piece);
  ev_score_t total = 0;
  square_t opp_sq = get_monarch(p, opp_color(c), 0);
  
  if (ptype_of(p->board[opp_sq]) == MONARCH) {
    int8_t delta_fil = fil_of(opp_sq) - f;
    int8_t delta_rnk = rnk_of(opp_sq) - r;
    total += mface_pair(piece, delta_fil, delta_rnk);
  }
  opp_sq = get_monarch(p, opp_color(c), 1);
  
  if (ptype_of(p->board[opp_sq]) == MONARCH) {
    int8_t delta_fil = fil_of(opp_sq) - f;
    int8_t delta_rnk = rnk_of(opp_sq) - r;
    total += mface_pair(piece, delta_fil, delta_rnk);
  }
  return total;
}

ev_score_t static inline __attribute__((always_inline)) mcede_pair(fil_t f, rnk_t r, int8_t delta_fil, int8_t delta_rnk) {

  int penalty = 0;
  if (delta_fil * EAST >= 0 && delta_rnk * NORTH >= 0) {  // NE quadrant
    penalty = (BOARD_WIDTH - f) * (BOARD_WIDTH - r);
  } else if (delta_fil * EAST >= 0 && delta_rnk * SOUTH >= 0) {  // SE quadrant
    penalty = (BOARD_WIDTH - f) * (r + 1);
  } else if (delta_fil * WEST >= 0 && delta_rnk * SOUTH >= 0) {  // SW quadrant
    penalty = (f + 1) * (r + 1);
  } else if (delta_fil * WEST >= 0 && delta_rnk * NORTH >= 0) {  // NW quadrant
    penalty = (f + 1) * (BOARD_WIDTH - r);
  } else {
    tbassert(false,
             "You need some direction in life!\n");  // Shouldn't happen.
  }
  return (PAWN_EV_VALUE * penalty) / (BOARD_WIDTH * BOARD_WIDTH);
}

// MCEDE heuristic: penalty for ceding opp Monarch room to move.
ev_score_t static inline __attribute__((always_inline)) mcede(position_t* p, piece_t piece, fil_t f, rnk_t r) {

  color_t c = color_of(piece);
  ev_score_t total = 0;
  tbassert(ptype_of(piece) == MONARCH, "piece.typ = %d\n", ptype_of(piece));

  square_t opp_sq = get_monarch(p, opp_color(c), 0);
  
  if (ptype_of(p->board[opp_sq]) == MONARCH) {
    int8_t delta_fil = fil_of(opp_sq) - f;
    int8_t delta_rnk = rnk_of(opp_sq) - r;
    total += mcede_pair(f, r, delta_fil, delta_rnk);
  }
  opp_sq = get_monarch(p, opp_color(c), 1);
  if (ptype_of(p->board[opp_sq]) == MONARCH) {
    int8_t delta_fil = fil_of(opp_sq) - f;
    int8_t delta_rnk = rnk_of(opp_sq) - r;
    total += mcede_pair(f, r, delta_fil, delta_rnk);
  }

  return total;
}

// Marks the path/line-of-sight of the laser until it hits a piece or goes off
// the board.
// Increment for each time you touch a square with the laser
//
// p : Current board state.
// c : Color of monarch shooting laser.
// laser_map : End result will be stored here. Every square on the
//             path of the laser is marked with mark_mask.
// mark_mask : What each square is marked with.
void add_laser_path(position_t* p, color_t c, float* laser_map, int n) {
  tbassert(n == 0 || n == 1, "bad monarch index %d\n", n);
  square_t sq = get_monarch(p, c, n);
  int bdir = (int) ori_of(p->board[sq]);
  int length = 1;
;
  if (!sq) return;
  tbassert(ptype_of(p->board[sq]) == MONARCH, "ptype: %d, sq: %d, n: %d\n",
           ptype_of(p->board[sq]), sq, n);

  while (true) {
    sq += beam_of(bdir);
    // set laser map to min
    
    if (laser_map[sq] > length) {
      laser_map[sq] = length;
    }
    length++;

    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(p->board[sq])) {
      case EMPTY:  // empty square
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(p->board[sq]));
        if (bdir < 0) {  // Hit back of Pawn
          return;
        }
        break;
      case MONARCH:  // Monarch
        return;      // sorry, game over my friend!
        break;
      case INVALID:  // Ran off edge of board
        return;
        break;
      default:  // Shouldna happen, man!
        tbassert(false, "Not cool, man.  Not cool.\n");
        break;
    }
  }
}

float static inline __attribute__((always_inline)) mult_dist(float delta_fil, float delta_rnk) {
  if (delta_fil == 0 && delta_rnk == 0) {
    return 2;
  }
  float x = (1 / (delta_fil + 1)) * (1 / (delta_rnk + 1));
  return x;
}

// Harmonic-ish distance: 1/(|dx|+1) + 1/(|dy|+1)
float static inline __attribute__((always_inline)) h_dist(rnk_t rnk_a, fil_t fil_a, square_t b) {
  int delta_fil = abs(fil_a - fil_of(b));
  int delta_rnk = abs(rnk_a - rnk_of(b));
  float x = inverse[delta_fil] + inverse[delta_rnk];
  return x;
}

#define GET_CENTRAL(f,r) centrality_lookup_table[f<<3 | r]

// Distance from the center: D^2 - d^2 where D is the maximum Euclidean distance
// from the center possible
ev_score_t concave_centrality(fil_t f, rnk_t r) {
  const int CENTER_LOC = 4;
  const int max_sq_dist = 4 * 4 + 4 * 4;
  const int file_dist = abs(f - CENTER_LOC);
  const int rank_dist = abs(r - CENTER_LOC);
  const int sq_dist = file_dist * file_dist + rank_dist * rank_dist;
  return max_sq_dist - sq_dist;
}

//OPTIMIZED with piece_loc
float static inline __attribute__((always_inline)) abs_qi(position_t* p, color_t target_color) {
  int qi = 0;
  uint64_t pieces = p->piece_loc[target_color];
  while (pieces) {
    uint64_t loc = pieces & (-pieces);
    pieces ^= loc;
    uint8_t sq64 = __builtin_ctzll(loc);
    square_t sq = sq_to_bit_index[sq64];

    if (ptype_of(p->board[sq]) == PAWN) {
      qi += qi_at(sq);
    }
  }
  return (float) qi;
}

//OPTIMIZED with piece_loc
float static inline __attribute__((always_inline)) rel_qi(position_t* p) {
  int qi = 0;
  square_t black_pawns[MAX_NUM_PAWNS_PER_COLOR];
  square_t white_pawns[MAX_NUM_PAWNS_PER_COLOR];
  int black_pawns_count = 0, white_pawns_count = 0;

  uint64_t white_pieces = p->piece_loc[WHITE];
  uint64_t black_pieces = p->piece_loc[BLACK];

  square_t monarch[2][2] ;
  monarch[WHITE][0] = get_monarch(p, WHITE, 0);
  monarch[WHITE][1] = get_monarch(p, WHITE, 1);
  monarch[BLACK][0] = get_monarch(p, BLACK, 0);
  monarch[BLACK][1] = get_monarch(p, BLACK, 1);
  //remove monarch set bits from white_pieces and black_pieces
  white_pieces ^= (sq_to_bitmask[monarch[WHITE][0]]);
  white_pieces ^= (sq_to_bitmask[monarch[WHITE][1]]);
  black_pieces ^= (sq_to_bitmask[monarch[BLACK][0]]);
  black_pieces ^= (sq_to_bitmask[monarch[BLACK][1]]);
  

  while (white_pieces) {
    uint64_t w_loc = (white_pieces) & (-white_pieces);
    square_t w_sq = __builtin_ctzll(w_loc);
    white_pawns[white_pawns_count++] = sq_to_bit_index[w_sq];
    white_pieces ^= w_loc;
  }

  while (black_pieces) {
    uint64_t b_loc = (black_pieces) & (-black_pieces);
    square_t b_sq = __builtin_ctzll(b_loc);
    black_pawns[black_pawns_count++] = sq_to_bit_index[b_sq];
    black_pieces ^= b_loc;
  }
  // if white > black: 1; white < black: -1; otherwise 0)
  for (int i = 0; i < white_pawns_count; i++) {
    for (int j = 0; j < black_pawns_count; j++) {
      if (qi_at(white_pawns[i]) > qi_at(black_pawns[j])) {
        qi += 1;
      } else if (qi_at(white_pawns[i]) < qi_at(black_pawns[j])) {
        qi -= 1;
      }
    }
  }
  return ((float)qi /
          ((float)((white_pawns_count + 1) * (black_pawns_count + 1))));
}

// Static evaluation.  Returns score
score_t eval(position_t* p, bool verbose) {
  // seed rand_r with a value of 1, as per
  // https://linux.die.net/man/3/rand_r
  // static __thread unsigned int seed = 1; //not using RANDOMIZE

  // verbose = true: print out components of score
  ev_score_t score[2][NUM_HEURISTICS] = {0};
  char sq_str[MAX_CHARS_IN_MOVE];

  square_t monarch[2][2] = {};
  monarch[WHITE][0] = get_monarch(p, WHITE, 0);
  monarch[WHITE][1] = get_monarch(p, WHITE, 1);
  monarch[BLACK][0] = get_monarch(p, BLACK, 0);
  monarch[BLACK][1] = get_monarch(p, BLACK, 1);

  uint64_t pawns = p->piece_loc[BLACK] | p->piece_loc[WHITE];
  pawns &= ~sq_to_bitmask[monarch[WHITE][0]];
  pawns &= ~sq_to_bitmask[monarch[WHITE][1]];
  pawns &= ~sq_to_bitmask[monarch[BLACK][0]];
  pawns &= ~sq_to_bitmask[monarch[BLACK][1]];

  for (color_t c= 0; c<= 1; c++) {
    uint64_t pieces = p->piece_loc[c];

    while (pieces) {
      uint64_t loc = pieces & (-pieces);
      square_t sq = sq_to_bit_index[__builtin_ctzll(loc)];
      piece_t piece = p->board[sq];
      fil_t f = fil_of(sq);
      rnk_t r = rnk_of(sq);
      
      pieces ^= loc;
      if (verbose) {
        square_to_str(sq, sq_str, MAX_CHARS_IN_MOVE);
      }

      ev_score_t centrality = GET_CENTRAL(f,r);

      switch (ptype_of(piece)) {
        case PAWN: {
          score[c][PMAT] += 1;
          if (verbose) {
            printf("PMATerial bonus for %s Pawn on %s\n", color_to_str(c),
                    sq_str);
          }

            // PTOUCH heuristic: Penalize for touching another Pawn
          if (p_touch(pawns, sq)) {
            score[c][PTOUCH] -= 1;
            if (verbose) {
              printf("PTOUCH penalty for %s Pawn on %s\n", color_to_str(c),
                      sq_str);
            }
          }

          // PPROXimity heuristic: Pawns should try to be proximate to
          // Monarchs according to harmonic-ish distance metric.
          float pweight = 0;

          // calculate pweight for white
          if (monarch[WHITE][0] > 0)
            pweight += h_dist(r, f, monarch[WHITE][0]);
          if (monarch[WHITE][1] > 0)
            pweight += h_dist(r, f, monarch[WHITE][1]);
          // calculate pweight for white
          if (monarch[BLACK][0] > 0)
            pweight += h_dist(r, f, monarch[BLACK][0]);
          if (monarch[BLACK][1] > 0)
            pweight += h_dist(r, f, monarch[BLACK][1]);

          ev_score_t prox_bonus = pweight * PAWN_EV_VALUE; //REMOVED scaling
          score[c][PPROX] += prox_bonus;
          if (verbose) {
            printf("PPROXimity bonus %d for %s Pawn on %s\n", prox_bonus,
                    color_to_str(c), sq_str);
          }

          // PMID heuristic: Pawns should try to be close to the center of
          // the board to maximize usefulness
          score[c][PMID] += centrality;
          if (verbose) {
            printf("PMID bonus %d for %s Pawn on %s\n", centrality,
                    color_to_str(c), sq_str);
          }

          break;
        } 
        case MONARCH: {
          // MFACE heuristic
          ev_score_t kface_bonus = mface(p, piece, f, r);
          score[c][MFACE] += kface_bonus;
          if (verbose) {
            printf("MFACE bonus %d for %s Monarch on %s\n", kface_bonus,
                  color_to_str(c), sq_str);
          }

          // MCEDE heuristic
          ev_score_t penalty = mcede(p, piece, f, r);
          score[c][MCEDE] -= penalty;
          if (verbose) {
            printf("MCEDE penalty %d for %s Monarch on %s\n", penalty,
                  color_to_str(c), sq_str);
          }

          // MMID heuristic
          score[c][MMID] += centrality;
          if (verbose) {
            printf("MMID bonus %d for %s Monarch on %s\n", centrality,
                  color_to_str(c), sq_str);
          }
          break;
        }
        case INVALID: {
          break;
        }
        case EMPTY: {
          break;
        }
        default:
          tbassert(false, "Jose says: no way!\n");  // No way, Jose!
      }
    }
  }

  // RELQI heuristic
  score[WHITE][RELQI] = PAWN_EV_VALUE * rel_qi(p);
  score[BLACK][RELQI] = -score[WHITE][RELQI];

  score[WHITE][ABSQI] = PAWN_EV_VALUE * abs_qi(p, WHITE);
  score[BLACK][ABSQI] = PAWN_EV_VALUE * abs_qi(p, BLACK);

  int weights[NUM_HEURISTICS] = {
      PTOUCH_weight, PPROX_weight, MFACE_weight, MCEDE_weight, LCOVERAGE_weight,
      PMID_weight,   MMID_weight,  PMAT_weight,  RELQI_weight, ABSQI_weight};

  ev_score_t total_score[2] = {0, 0};
  for (color_t c = 0; c <= 1; c++) {
    for (int i = 0; i < NUM_HEURISTICS; i++) {
      ev_score_t bonus;
      if (floating_point_heuristics[i]) {
        bonus = score[c][i] * (weights[i] / (float)PAWN_EV_VALUE);
        if (verbose) {
        printf("Total %s score of %d for %s\n", heuristic_strs[i],
                score[c][i], color_to_str(c));
        }
      } else {
        bonus = score[c][i] * weights[i] ;
        if (verbose) {
        printf("Total %s score of %d for %s\n", heuristic_strs[i],
                score[c][i], color_to_str(c));
        }
      }
      
      total_score[c] += bonus;
      if (verbose) {
        printf("Final %s contribution of %d for %s\n", heuristic_strs[i], bonus,
               color_to_str(c));
      }
    }
  }

  ev_score_t tot = total_score[WHITE] - total_score[BLACK];

  if (color_to_move_of(p) == BLACK) {
    tot = -tot;
  }

  return tot / EV_SCORE_RATIO;
}