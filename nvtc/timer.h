// Copyright 2019 zhaofeng-shu33
#pragma once

// Walltime timer
class Timer {
 public:
  virtual ~Timer() {}

  // Returns number of milliseconds since the last call to Done, Reset, or
  // constructor and prints it with label to stderr.
  virtual int Done(const char* label) = 0;

  static Timer* NewTimer();
};
