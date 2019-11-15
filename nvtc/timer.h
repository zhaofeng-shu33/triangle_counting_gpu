// Copyright 2019 zhaofeng-shu33
#pragma once
/// \file
/// \brief time utility


//! \brief: timer
class Timer {
 public:
  virtual ~Timer() {}

  //! \brief: Returns number of milliseconds since the last call to Done
  virtual int Done(const char* label) = 0;

  static Timer* NewTimer();
};
