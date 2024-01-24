// Copyright (c) 2022 MIT License by 6.172 / 6.106 Staff

////////////////////////////////////////////////////////////////////////////
//
// clktimer.h
//
// CClockTimer
//
// Remi Coulom
//
// October, 1996
//
////////////////////////////////////////////////////////////////////////////
#ifndef CLKTIMER_H
#define CLKTIMER_H

#include <cstdint>

#include "./chtimer.h"  // CChessTimer

class CClockTimer : public CChessTimer {  // clkt
 private:  //////////////////////////////////////////////////////////////////
  int64_t lPrevious;

 public:  ///////////////////////////////////////////////////////////////////
  CClockTimer();
  virtual void Wait(int64_t lMilliSeconds = 0);
  virtual void WaitInterval(int64_t lMilliSeconds = 0);
  virtual CTime GetInterval();
};

#endif  // CLKTIMER_H
