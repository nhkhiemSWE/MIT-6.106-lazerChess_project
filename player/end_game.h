// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

#ifndef END_GAME_H
#define END_GAME_H

#include <stdbool.h>

#include "move_gen.h"
#include "search.h"

bool player_wins(position_t* p, color_t c);
bool is_end_game_position(position_t* p);
bool is_game_over(position_t* p);
score_t get_end_game_score(position_t* p, int pov, int ply);

#endif  // END_GAME_H
