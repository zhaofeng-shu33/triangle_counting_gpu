// Copyright 2019 zhaofeng-shu33
#pragma once

#include <stdint.h>

#include "nvtc/io.h"

void PreInitGpuContext(int device = 0);
uint64_t GpuForward(int* edges, int num_nodes, uint64_t num_edges);
uint64_t MultiGpuForward(int* edges, int device_count, int num_nodes,
    uint64_t num_edges);
uint64_t GpuForwardSplit(int* edges, int num_nodes, uint64_t num_edges,
    int split_num = 2);
int GetSplitNum(int num_nodes, uint64_t num_edges);
