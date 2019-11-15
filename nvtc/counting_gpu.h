// Copyright 2019 zhaofeng-shu33
#pragma once
/// \file
/// \brief gpu counting method
///
#include <stdint.h>

#include "nvtc/io.h"

//! counting triangles with enough GPU memory
uint64_t GpuForward(int* edges, int num_nodes, uint64_t num_edges);
//! counting large triangles which exceeds the GPU memory capacity
//! by splitting the edges
uint64_t GpuForwardSplit(int* edges, int num_nodes, uint64_t num_edges,
    int split_num = 2);
//! get the minimum split number for current GPU device
int GetSplitNum(int num_nodes, uint64_t num_edges);
