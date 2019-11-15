// Copyright 2019 zhaofeng-shu33
#pragma once
/// \file
/// \brief utility function using [thrust](https://docs.nvidia.com/cuda/thrust/index.html) library

#include <stdint.h>

//! get the summation of results
uint64_t SumResults(int size, uint64_t* results);
