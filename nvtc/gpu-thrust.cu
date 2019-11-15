// Copyright 2019 zhaofeng-shu33
#include "nvtc/gpu-thrust.h"

#include <thrust/device_ptr.h>
#include <thrust/functional.h>
#include <thrust/reduce.h>
#include <thrust/sort.h>

uint64_t SumResults(int size, uint64_t* results) {
  thrust::device_ptr<uint64_t> ptr(results);
  return thrust::reduce(ptr, ptr + size);
}
