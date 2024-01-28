// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

// Transposition table
//
// https://www.chessprogramming.org/Transposition
// https://www.chessprogramming.org/Transposition_Table
#include "tt.h"

#include <stdio.h>
#include <stdlib.h>
#include "simple_mutex.h"
#include "tbassert.h"

int HASH;    // hash table size in MBytes
int USE_TT;  // Use the transposition table.
// Turn off for deterministic behavior of the search.

#define NUM_LOCK 6
simple_mutex_t tt_lock[NUM_LOCK];
uint64_t simple_mutex_mask = NUM_LOCK-1;

// the actual record that holds the data for the transposition
// typedef to be ttRec_t in tt.h
struct compressedTTRec {
  uint64_t key;
  uint64_t data;
  //Data include: 10 extra zeros | 8 bits for age | 2 bits for bound | 8 bits for quality | 16 bits for score | 20 bits for move_t
  //move_t break down: 6 bits for to_sq | 6 bits for from_sq | 2 bits for ori | 2 bits for ptype_t
};
//move_t compressed
const uint64_t MOVE_SHIFT = 0;
const uint64_t MOVE_MASK = (((uint64_t) 0b11111111111111111111) << MOVE_SHIFT);
const uint64_t PTYPE_MOVE_SHIFT = 0;
const uint64_t PTYPE_MOVE_MASK = (((uint64_t) 0b11) << PTYPE_MOVE_SHIFT);
const uint64_t ORI_MOVE_SHIFT = 2;
const uint64_t ORI_MOVE_MASK = (((uint64_t) 0b11) << ORI_MOVE_SHIFT);
const uint64_t FROM_SQ_SHIFT = 4;
const uint64_t FROM_SQ_MASK = (((uint64_t) 0b11111111) << FROM_SQ_SHIFT);
const uint64_t TO_SQ_SHIFT = 12;
const uint64_t TO_SQ_MASK = (((uint64_t) 0b11111111) << TO_SQ_SHIFT);
//score compressed
const uint64_t SCORE_SHIFT = 20;
const uint64_t SCORE_MASK = (((uint64_t) 0b1111111111111111) << SCORE_SHIFT);
//quality compressed
const uint64_t QUALITY_SHIFT = 36;
const uint64_t QUALITY_MASK = (((uint64_t) 0b11111111) << QUALITY_SHIFT);
//bound compressed
const uint64_t BOUND_SHIFT = 44;
const uint64_t BOUND_MASK = (((uint64_t) 0b11) << BOUND_SHIFT);
//age compressed
const uint64_t AGE_SHIFT = 46;
const uint64_t AGE_MASK = (((uint64_t) 0b11111111) << AGE_SHIFT);

// -----------------------------------------------------------------------------
// CompressedTTRec_t getters
// -----------------------------------------------------------------------------
uint8_t static inline __attribute__((always_inline))  get_age_compressed(compressedTTRec_t* tt) {
  tt->key = 0;
  return  (uint8_t) ((tt->data & AGE_MASK) >> AGE_SHIFT);
}

ttBound_t static inline __attribute__((always_inline)) get_bound_compressed(compressedTTRec_t* tt) {
  uint8_t bound = (tt->data & BOUND_MASK) >> BOUND_SHIFT;
  return (ttBound_t) bound;
}

uint8_t static inline __attribute__((always_inline)) get_quality_compressed(compressedTTRec_t* tt) {
  return (uint8_t) ((tt->data & QUALITY_MASK) >> QUALITY_SHIFT);
}

score_t static inline __attribute__((always_inline)) get_score_compressed(compressedTTRec_t* tt) {
  return (int16_t) (((tt->data) & SCORE_MASK) >> SCORE_SHIFT);
}

move_t static inline __attribute__((always_inline)) get_move_compressed(compressedTTRec_t* tt) {
  move_t move;
  uint64_t data_compressed = tt->data;
  //Get ptype
  uint8_t ptype =  (data_compressed & PTYPE_MOVE_MASK) >> PTYPE_MOVE_SHIFT;
  move.typ = (ptype_t) ptype;
  uint8_t rot = (data_compressed & ORI_MOVE_MASK) >> ORI_MOVE_SHIFT;
  move.rot = (rot_t) rot;
  move.from_sq = (data_compressed & FROM_SQ_MASK) >> FROM_SQ_SHIFT;
  move.to_sq = (data_compressed & TO_SQ_MASK) >> TO_SQ_SHIFT;
  return move;
}

// -----------------------------------------------------------------------------
// CompressedTTRec_t setters
// -----------------------------------------------------------------------------
void static inline __attribute__((always_inline)) set_key_compressed (compressedTTRec_t* tt, uint64_t key) {
  tt->key = key;
}

void static inline __attribute__((always_inline)) set_age_compressed (compressedTTRec_t* tt, uint8_t age) {
  uint64_t data = tt->data;
  uint64_t new_age = (uint64_t) ((uint8_t) age);
  data &= ~AGE_MASK;
  data |= (new_age << AGE_SHIFT);
  tt->data = data;
  tbassert(get_age_compressed(tt) == age, "Age was not set correctly!");
}

void static inline __attribute__((always_inline)) set_bound_compressed (compressedTTRec_t* tt, ttBound_t bound) {
  uint64_t data = tt->data;
  uint64_t new_bound = (uint64_t) ((uint8_t) bound);
  data &= ~BOUND_MASK;
  data |= (new_bound << BOUND_SHIFT);
  tt->data = data;
  tbassert(get_bound_compressed(tt) == bound, "Bound was not set correctly!");
}

void static inline __attribute__((always_inline)) set_quality_compressed (compressedTTRec_t* tt, int quality) {
  tbassert(quality >= 0, "Quality can't be negative");
  tbassert(quality <= 255, "Quality is too large");
  uint64_t data = tt->data;
  uint64_t new_quality = (uint64_t) ((uint8_t) quality);
  data &= ~QUALITY_MASK;
  data |= (new_quality << QUALITY_SHIFT);
  tt->data = data;
  tbassert(get_quality_compressed(tt) == quality, "Quality was not set conrrectly!");
}

void static inline __attribute__((always_inline)) set_score_compressed (compressedTTRec_t* tt, score_t score) {
  uint64_t data = tt->data;
  uint64_t new_score = (uint64_t) ((uint16_t) score);
  data &= ~SCORE_MASK;
  data |= (new_score << SCORE_SHIFT);
  tt->data = data;
  tbassert(get_score_compressed(tt) == score, "Score was not set correctly!");
}

void static inline __attribute__((always_inline)) set_move_compressed (compressedTTRec_t* tt, move_t move) {
  uint64_t data = tt->data;
  uint64_t new_move = 0;
  uint64_t ptype = (uint64_t) (move.typ);
  uint64_t rot = (uint64_t) (move.rot);
  uint64_t from_sq = (uint64_t) move.from_sq;
  uint64_t to_sq = (uint64_t) move.to_sq; 

  new_move |= ptype << PTYPE_MOVE_SHIFT;
  new_move |= rot << ORI_MOVE_SHIFT;
  new_move |= from_sq << FROM_SQ_SHIFT;
  new_move |= to_sq << TO_SQ_SHIFT;

  data &= ~MOVE_MASK; 
  data |= (new_move) << MOVE_SHIFT;
  tt->data = data;
  tbassert(move_eq(get_move_compressed(tt), move), "Move was not set correctly!"); 
}

// each set is a RECORDE_PER_SET-way set-associative cache of records
#define RECORDS_PER_SET 2
typedef struct {
  compressedTTRec_t records[RECORDS_PER_SET];
} ttSet_t;

// struct def for the global transposition table
struct ttHashtable {
  uint64_t num_of_sets;  // how many sets in the hashtable
  uint64_t mask;         // a mask to map from key to set index
  uint16_t age;
  ttSet_t* tt_set;  // array of sets that contains the transposition
} hashtable;        // name of the global transposition table

// getting the move out of the record
move_t tt_move_of(compressedTTRec_t* rec) { return get_move_compressed(rec); }

// getting the score out of the record
score_t tt_score_of(compressedTTRec_t* rec) { return get_score_compressed(rec); }

size_t tt_get_bytes_per_record() { return sizeof(struct compressedTTRec); }

uint32_t tt_get_num_of_records() {
  return hashtable.num_of_sets * RECORDS_PER_SET;
}

void tt_resize_hashtable(int size_in_meg) {
  uint64_t size_in_bytes = (uint64_t)size_in_meg * (1ULL << 20);
  // total number of sets we could have in the hashtable
  uint64_t num_of_sets = size_in_bytes / sizeof(ttSet_t);

  uint64_t pow = 1;
  num_of_sets--;
  while (pow <= num_of_sets) {
    pow *= 2;
  }
  num_of_sets = pow;

  hashtable.num_of_sets = num_of_sets;
  hashtable.mask = num_of_sets - 1;
  hashtable.age = 0;

  free(hashtable.tt_set);  // free the old ones
  hashtable.tt_set = (ttSet_t*)malloc(sizeof(ttSet_t) * num_of_sets);

  if (hashtable.tt_set == NULL) {
    fprintf(stderr, "Hash table too big\n");
    exit(1);
  }

  // might as well clear the table while we are at it
  memset(hashtable.tt_set, 0, sizeof(ttSet_t) * hashtable.num_of_sets);
}

void tt_make_hashtable(int size_in_meg) {
  hashtable.tt_set = NULL;
  tt_resize_hashtable(size_in_meg);
  for (int i = 0; i< NUM_LOCK; i ++) {
    init_simple_mutex(&tt_lock[i]);
  }
}

void tt_free_hashtable() {
  free(hashtable.tt_set);
  hashtable.tt_set = NULL;
}

// age the hash table by incrementing global age
void tt_age_hashtable() { hashtable.age++; }

void tt_clear_hashtable() {
  memset(hashtable.tt_set, 0, sizeof(ttSet_t) * hashtable.num_of_sets);
  hashtable.age = 0;
}

void tt_hashtable_put(uint64_t key, uint8_t depth, score_t score, uint8_t bound_type,
                      move_t move) {
  tbassert(abs(score) != INF, "Score was infinite.\n");

  uint64_t set_index = key & hashtable.mask;
  // current record that we are looking into
  compressedTTRec_t* curr_rec = hashtable.tt_set[set_index].records;
  // best record to replace that we found so far
  compressedTTRec_t* rec_to_replace = curr_rec;
  int replacemt_val = -99;  // value of doing the replacement

  for (int i = 0; i < RECORDS_PER_SET; i++, curr_rec++) {
    int value = 0;  // points for sorting

    // always use entry if it's not used or has same key
    if (!curr_rec->key || key == curr_rec->key) {
      if (move_eq(move, NULL_MOVE)) {
        move = get_move_compressed(curr_rec);
      }
      set_key_compressed(curr_rec, key);
      set_quality_compressed(curr_rec, depth);
      set_move_compressed(curr_rec, move);
      set_age_compressed(curr_rec, hashtable.age);
      set_score_compressed(curr_rec, score);
      set_bound_compressed(curr_rec, bound_type);
      return;
    }

    // otherwise, potential candidate for replacement
    if (get_age_compressed(curr_rec) < get_age_compressed(rec_to_replace)) {
      value -= 6;  // prefer not to replace if same age
    }
    if (get_quality_compressed(curr_rec)  < get_quality_compressed(rec_to_replace)) {
      value += 1;  // prefer to replace if worse quality
    }
    if (value > replacemt_val) {
      replacemt_val = value;
      rec_to_replace = curr_rec;
    }
  }
  // update the record that we are replacing with this record
  set_key_compressed(rec_to_replace, key);
  set_quality_compressed(rec_to_replace, depth);
  set_move_compressed(rec_to_replace, move);
  set_age_compressed(rec_to_replace, hashtable.age);
  set_score_compressed(rec_to_replace, score);
  set_bound_compressed(rec_to_replace, (ttBound_t) bound_type);
}

compressedTTRec_t* tt_hashtable_get(uint64_t key) {
  if (!USE_TT) {
    return NULL;  // done if we are not using the transposition table
  }

  uint64_t set_index = key & hashtable.mask;
  compressedTTRec_t* rec = hashtable.tt_set[set_index].records;

  compressedTTRec_t* found = NULL;
  for (int i = 0; i < RECORDS_PER_SET; i++, rec++) {
    if (rec->key == key) {  // found the record that we are looking for
      found = rec;
      break;
    }
  }
  return found;
}

score_t win_in(int ply) { return WIN - ply; }

score_t lose_in(int ply) { return -WIN + ply; }

// when we insert score for a position into the hashtable, it does keeps the
// pure score that does not account for which ply you are in the search tree;
// when you retrieve the score from the hashtable, however, you want to
// consider the value of the position based on where you are in the search tree
score_t tt_adjust_score_from_hashtable(compressedTTRec_t* rec, int ply_in_search) {
  score_t score = get_score_compressed(rec);
  if (score >= win_in(MAX_PLY_IN_SEARCH)) {
    return score - ply_in_search;
  }
  if (score <= lose_in(MAX_PLY_IN_SEARCH)) {
    return score + ply_in_search;
  }
  return score;
}

// the inverse of tt_adjust_score_for_hashtable
score_t tt_adjust_score_for_hashtable(score_t score, int ply_in_search) {
  if (score >= win_in(MAX_PLY_IN_SEARCH)) {
    return score + ply_in_search;
  }
  if (score <= lose_in(MAX_PLY_IN_SEARCH)) {
    return score - ply_in_search;
  }
  return score;
}

// Whether we can use this record or not
bool tt_is_usable(compressedTTRec_t* tt, int depth, score_t beta) {
  // can't use this record if we are searching at depth higher than the
  // depth of this record.
  if (get_quality_compressed(tt) < depth) {
    return false;
  }
  // otherwise check whether the score falls within the bounds
  if ((get_bound_compressed(tt) == LOWER) && get_score_compressed(tt) >= beta) {
    return true;
  }
  if ((get_bound_compressed(tt) == UPPER) && get_score_compressed(tt) < beta) {
    return true;
  }
  return false;
}
