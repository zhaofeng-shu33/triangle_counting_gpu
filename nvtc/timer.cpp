// Copyright 2019 zhaofeng-shu33
#include "nvtc/timer.h"

#include <chrono>  // NOLINT(build/c++11)
#include <iostream>

namespace {
class TimerImpl : public Timer {
 public:
  TimerImpl() : last(Clock::now()) {}
  virtual ~TimerImpl() {}

  virtual int Done(const char* label) {
    Clock::time_point now = Clock::now();
    using std::chrono::duration_cast;
    int res = duration_cast<std::chrono::milliseconds>(now - last).count();
    last = now;
    std::cout << label << " " << res << " ms" << std::endl;
    return res;
  }

  typedef std::chrono::high_resolution_clock Clock;

 private:
  Clock::time_point last;
};
}  // namespace

Timer* Timer::NewTimer() { return new TimerImpl; }
