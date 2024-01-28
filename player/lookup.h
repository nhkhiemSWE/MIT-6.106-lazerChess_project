// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#ifndef LOOKUP_H
#define LOOKUP_H

#define OPEN_BOOK_DEPTH 4

int lookup_sizes[OPEN_BOOK_DEPTH] = {2, 96, 98, 2};

// Move histories are paired with their best moves in the lookup tables
// IE: lookup_table_depth_n[] = {move_history1, best_move1, move_history2,
// best_move2, ...}
// a move history is the concatenation of the move strings, e.g.
// a lookup_table_depth_3[] could look like: {"move1move2", "move3", ...}
const char* lookup_table_depth_1[] = {"", "a1U"};
const char* lookup_table_depth_2[] = {"b1a2", "a6U", "b1b2", "a7b6", "b1c2", "a7b6", "b1c1", "a7b6",
                                    "b1a1", "a7b6", "b1R", "a6L", "b1U", "a7a6", "b1L", "a7a6",
                                    "d1c1", "a7a6", "d1R", "a7a6", "d1U", "a7a6", "d1L", "a7a6",
                                    "d1c2", "a7a6", "d1d2", "a7a6", "d1e2", "a7a6", "d1e1", "a7a6",
                                    "a1b2", "a6U", "a1R", "a7L", "a1U", "a7b6", "a1L", "a7a6",
                                    "g1h2", "h6U", "g1g2", "h7g6", "g1f2", "h7g6", "g1f1", "h7g6",
                                    "g1h1", "h7g6", "g1L", "h6R", "g1U", "h7h6", "g1R", "h7h6",
                                    "e1f1","h7h6", "e1L", "h7h6", "e1U", "h7h6", "e1R", "h6g5",
                                    "e1f2", "h7h6", "e1e2", "h7h6", "e1d2", "h7h6", "e1d1", "h7h6",
                                    "h1g2", "h6U", "h1L", "h7R", "h1U", "h7g6", "h1R", "h7h6",
                                    "h0g1", "a7a6", "a0b1", "h7g6",
                                    "h0h1", "a6b5", "a0a1", "h6g5", "h0L", "a7a6", "a0R", "h7h6",                                
                                      };
const char* lookup_table_depth_3[] = {"b1a2a6U", "a0b1", "b1b2a7b6", "a0a1", "b1c2a7b6", "a1U", "b1c1a7b6", "h0g1",
                                      "d1c2a7a6", "b1R", "d1d2a7a6", "b1c2", "d1e2a7a6", "h0g1", "d1e1a7a6", "h0g1",
                                      "b1a1a7b6", "h0h1", "b1Ra6L", "d1c2", "b1Ua7a6", "a0b0", "b1La7a6", "a1b0",
                                      "d1c1a7a6", "a1a2", "d1Ra7a6", "h0g1", "d1Ua7a6", "h0g1", "d1La7b6", "h0g1",
                                      "g1h2h6U", "h0g1", "g1g2h7g6", "h0h1", "g1f2h7g6", "h1U", "g1f1h7g6", "a0b1",
                                      "e1f2h7h6", "g1L", "e1e2h7h6", "g1f2", "e1d2h7h6", "a0b1", "e1d1h7h6", "a0b1",
                                      "g1h1h7g6", "a0a1", "g1Lh6R", "e1f2", "g1Uh7h6", "h0g0", "g1Rh7h6", "h1g0",
                                      "e1f1h7h6", "h1h2", "e1Lh7h6", "a0b1", "e1Uh7h6", "a0b1", "e1Rh7g6", "a0b1",
                                      "a1b2a6U", "a0b1", "a1Ra7L", "a0a1", "a1Ua7b7", "a0U", "a1La7a6", "a1a2",
                                      "h1g2h6U", "h0g1", "h1Lh7R", "h0h1", "h1Uh7g7", "h0U", "h1Rh7h6", "h1h2",
                                      "a0b1h7g6", "h0R", "h0g1a7a6", "a0L", "a1b2h7g6", "a0U", "h1g2a7b6", "h0U",
                                      "h0h1a6b5", "h1R", "a0a1h6g5", "a1L", "h0La7b6", "h1g2", "a0Rh7h6", "a1b2",
                                      "a1Ua7b6", "a0U"};

const char* lookup_table_depth_4[]= {"a0b1h7g6a0R", "a7R"};

const char** lookup_tables[OPEN_BOOK_DEPTH] = {lookup_table_depth_1,
                                               lookup_table_depth_2,
                                               lookup_table_depth_3,
                                               lookup_table_depth_4};

#endif  // LOOKUP_H
