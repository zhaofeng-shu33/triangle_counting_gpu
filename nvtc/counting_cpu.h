// Copyright 2019 zhaofeng-shu33
#pragma once
#include "nvtc/io.h"

//! get the number of triangles in the graph, represented by edges
uint64_t CpuForward(int* edges, int node_num, uint64_t edge_num);
