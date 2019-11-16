// Copyright 2019 zhaofeng-shu33
#include "nvtc/counting_cpu.h"

uint64_t CpuForward(int* edges, int node_num, uint64_t edge_num) {
    uint64_t m = edge_num;
    int n = node_num;
    uint64_t* dev_nodes = new uint64_t[n + 1];
    // Calculate NodePointers
    for (uint64_t i = 0; i <= m; i++) {
        int prev = i > 0 ? edges[2 * i - 1] : -1;
        int next = i < m ? edges[2 * i + 1] : n;
        for (int j = prev + 1; j <= next; j++)
            dev_nodes[j] = i;
    }
    // Calculate Triangles
    uint64_t count = 0;
#pragma omp parallel for reduction(+:count)
    for (uint64_t i = 0; i < m; i++) {
        int u = edges[2 * i], v = edges[2 * i + 1];
        uint64_t u_it = dev_nodes[u], u_end = dev_nodes[u + 1];
        uint64_t v_it = dev_nodes[v], v_end = dev_nodes[v + 1];
        int a = edges[2 * u_it], b = edges[2 * v_it];
        while (u_it < u_end && v_it < v_end) {
            int d = a - b;
        if (d <= 0)
            a = edges[2 * (++u_it)];
        if (d >= 0)
            b = edges[2 * (++v_it)];
        if (d == 0)
            ++count;
        }
    }
    delete dev_nodes;
    return count;
}

uint64_t CalculateTrianglesSplitCPU(int* edges, uint64_t* dev_nodes,
    uint64_t start_range, uint64_t end_range) {
    uint64_t m = edge_num;
    int n = node_num;
    // Calculate Triangles
    uint64_t count = 0;
#pragma omp parallel for reduction(+:count)
    for (uint64_t i = start_range; i < end_range; i++) {
        int u = edges[2 * i], v = edges[2 * i + 1];
        uint64_t u_it = dev_nodes[u], u_end = dev_nodes[u + 1];
        uint64_t v_it = dev_nodes[v], v_end = dev_nodes[v + 1];
        int a = edges[2 * u_it], b = edges[2 * v_it];
        while (u_it < u_end && v_it < v_end) {
            int d = a - b;
        if (d <= 0)
            a = edges[2 * (++u_it)];
        if (d >= 0)
            b = edges[2 * (++v_it)];
        if (d == 0)
            ++count;
        }
    }
    return count;
}

