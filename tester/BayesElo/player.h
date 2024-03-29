// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

////////////////////////////////////////////////////////////////////////////
//
// player.h
//
// CPlayer class definition
//
// Remi Coulom
//
// june 1996
//
////////////////////////////////////////////////////////////////////////////
#ifndef PLAYER_H
#define PLAYER_H

class CPlayer {  // player
 private:  //////////////////////////////////////////////////////////////////
  int player;

 public:  ///////////////////////////////////////////////////////////////////
  enum {
    White = 0,
    Black = 1
  };

  CPlayer(int i) {
    player = i;
  }
  int Opponent() const {
    return player ^ Black;
  }
};

#endif  // PLAYER_H
