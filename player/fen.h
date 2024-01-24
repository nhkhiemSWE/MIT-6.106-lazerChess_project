// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#ifndef FEN_H
#define FEN_H

#include "move_gen.h"

extern const char* STARTPOS_FEN;
extern const char* ENDGAME_FEN;

// Assuming BOARD_WIDTH is at most 99, MAX_FEN_CHARS is
//   BOARD_WIDTH * BOARD_WIDTH * 2  (for a piece in every square)
//   + BOARD_WIDTH - 1  (for slashes delineating ranks)
//   + 2  (for final space and player turn)
#define MAX_FEN_CHARS ((BOARD_WIDTH) * (BOARD_WIDTH)*2 + (BOARD_WIDTH)-1 + 2)

int fen_to_pos(position_t* p, const char* fen);
void pos_to_fen(position_t* p, char* fen);

#endif  // FEN_H
