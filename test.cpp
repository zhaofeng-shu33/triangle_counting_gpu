// Copyright 2019 zhaofeng-shu33

#include <iostream>

#include <gtest/gtest.h>  // NOLINT(build/include_order)

#include "nvtc/io.h"
#if GPU
#include "nvtc/counting_gpu.h"
#endif
#include "nvtc/counting_cpu.h"

TEST(split, array) {
    uint64_t arr[] = {0, 1, 3, 5, 8, 10, 18};
    uint64_t* out_arr;
    uint64_t max_num = get_split(arr, 7, 2, out_arr);
    EXPECT_EQ(max_num, 10);
    EXPECT_EQ(out_arr[0], 0);
    EXPECT_EQ(out_arr[1], 10);
    EXPECT_EQ(out_arr[2], 18);
}

TEST(swap, array) {
    int arr_static[] = {1, 2, 3, 4, 5, 6};
    int* arr = arr_static;
    swap_array(arr, 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 3);
    EXPECT_EQ(arr[2], 5);
    EXPECT_EQ(arr[3], 2);
    EXPECT_EQ(arr[4], 4);
    EXPECT_EQ(arr[5], 6);
}

TEST(io, swap) {
    int* arcs;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_complete_4.bin", arcs);
    swap_array(arcs, info_pair.second);
    EXPECT_EQ(arcs[0], 0);
    EXPECT_EQ(arcs[1], 0);
    EXPECT_EQ(arcs[2], 1);
    EXPECT_EQ(arcs[3], 0);
    EXPECT_EQ(arcs[4], 1);
    EXPECT_EQ(arcs[5], 2);
    EXPECT_EQ(arcs[6], 1);
    EXPECT_EQ(arcs[7], 2);
    EXPECT_EQ(arcs[8], 2);
    EXPECT_EQ(arcs[9], 3);
    EXPECT_EQ(arcs[10], 3);
    EXPECT_EQ(arcs[11], 3);
}

#if GPU
TEST(tc, io_bin) {
    int* edges;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_io.bin", edges);
    uint64_t trcount = GpuForward(edges, info_pair.first, info_pair.second);
    EXPECT_EQ(trcount, 1);
    free(edges);
    EXPECT_THROW(read_binfile_to_arclist("test_io_false.bin", edges),
                 std::logic_error);
}

TEST(tc, io_nvgraph) {
    int* edges;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_io_nvgraph.bin", edges);
    uint64_t trcount = GpuForward(edges, info_pair.first, info_pair.second);
    EXPECT_EQ(trcount, 3);
    free(edges);
}

TEST(split, io_nvgraph) {
    int* edges;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_io_nvgraph.bin", edges);
    uint64_t trcount = GpuForwardSplit(edges,
        info_pair.first, info_pair.second);
    EXPECT_EQ(trcount, 3);
    free(edges);
}

TEST(split, io_complete_4) {
    int* edges;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_complete_4.bin", edges);
    uint64_t trcount = GpuForwardSplit(edges,
        info_pair.first, info_pair.second, 3);
    EXPECT_EQ(trcount, 4);
    free(edges);
}
#endif

TEST(cpu, io_bin) {
    int* arcs;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_io.bin", arcs);
    uint64_t tcount = CpuForward(arcs, info_pair.first, info_pair.second);
    EXPECT_EQ(tcount, 1);
    free(arcs);
}

TEST(cpu, io_nvgraph) {
    int* arcs;
    std::pair<int, uint64_t> info_pair = read_binfile_to_arclist(
        "test_io_nvgraph.bin", arcs);
    uint64_t tcount = CpuForward(arcs, info_pair.first, info_pair.second);
    EXPECT_EQ(tcount, 3);
    free(arcs);
}


