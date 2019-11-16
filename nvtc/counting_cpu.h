// Copyright 2019 zhaofeng-shu33
#pragma once
/// \file
/// \brief cpu counting method
///
#include "nvtc/io.h"

//! get the number of triangles in the graph, represented by edges
uint64_t CpuForward(int* edges, int node_num, uint64_t edge_num);

uint64_t CalculateTrianglesSplitCPU(int* edges, uint64_t* dev_nodes,
    uint64_t start_range, uint64_t end_range);
