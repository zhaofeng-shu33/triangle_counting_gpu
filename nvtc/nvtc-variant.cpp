// Copyright 2019 zhaofeng-shu33
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <memory>
#include <iostream>
#include <vector>

#include "nvtc/io.h"
#include "nvtc/counting_gpu.h"
#include "nvtc/timer.h"
#include "nvtc/counting_cpu.h"

int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-f") != 0) {
        std::cout << "Usage: nvtc-variant -f input.bin" << std::endl;
        exit(-1);
    }
#if TIMECOUNTING
    std::unique_ptr<Timer> t(Timer::NewTimer());
#endif
    const char* io_hint = std::getenv("DATAIO");
    const char* device_hint = std::getenv("DEVICEHINT");
    int* edges;
    std::pair<int, uint64_t> info_pair;
    info_pair = read_binfile_to_arclist(argv[2], edges);
#if VERBOSE
    std::cout << "Num of Nodes: " << info_pair.first << std::endl;
    std::cout << "Num of Edges: " << info_pair.second << std::endl;
#endif

#if TRCOUNTING
    uint64_t result = 0;
#if TIMECOUNTING
    t->Done("Reading Data");
#endif
#if GPU
    if (device_hint == NULL || strcmp(device_hint, "GPU") == 0) {
       result = GpuForward(edges, info_pair.first, info_pair.second);
    } else if (strcmp(device_hint, "GPUSPLIT") == 0) {
        int split_num = GetSplitNum(info_pair.first, info_pair.second);
        result = GpuForwardSplit(edges, info_pair.first,
            info_pair.second, split_num);
    } else if (strcmp(device_hint, "CPU") == 0) {
        result = CpuForward(edges, info_pair.first, info_pair.second);
    } else {
        result = GpuForward(edges, info_pair.first, info_pair.second);
    }
#else
    result = CpuForward(edges, info_pair.first, info_pair.second);
#endif
    free(edges);
#if TIMECOUNTING
    t->Done("Compute number of triangles");
#endif
    std::cout << "There are " << result <<
            " triangles in the input graph." << std::endl;
#endif
}
