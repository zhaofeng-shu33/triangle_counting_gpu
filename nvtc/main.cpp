#include "gpu.h"
#include "graph.h"
#include "timer.h"
#include "counting_cpu.h"
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

using namespace std;


int main(int argc, char *argv[]) {
    if (argc != 3 || strcmp(argv[1], "-f") != 0) {
        std::cout << "Usage: nvtc-variant -f input.bin" << std::endl;
        exit(-1);
    }
#if TIMECOUNTING 
    unique_ptr<Timer> t(Timer::NewTimer());
#endif
#if SECONDVERSION
    TRCountingGraph TRCountingGraph(argv[2]);
#else    
    const char* io_hint = std::getenv("DATAIO");
    const char* device_hint = std::getenv("DEVICEHINT");
    int* edges;
    std::pair<int, uint64_t> info_pair;
    info_pair = read_binfile_to_arclist_v2(argv[2], edges);
#if VERBOSE    
    std::cout << "Num of Nodes: " << info_pair.first << std::endl;
    std::cout << "Num of Edges: " << info_pair.second << std::endl;
#endif
#endif

#if TRCOUNTING
    uint64_t result = 0;
    t->Done("Reading Data");
#if SECONDVERSION
    result = GpuForward_v2(TRCountingGraph);
#else
#if GPU
    if(device_hint == NULL || strcmp(device_hint, "GPU") == 0){
       result = GpuForward(edges, info_pair.first, info_pair.second);
    }
    else if (strcmp(device_hint, "CPU") == 0) {
        result = CpuForward(edges, info_pair.first, info_pair.second);
    } else {
        result = GpuForward(edges, info_pair.first, info_pair.second);
    }
#else
	result = CpuForward(edges, info_pair.first, info_pair.second);
#endif
    free(edges);
#endif
#if TIMECOUNTING    
    t->Done("Compute number of triangles");
#endif
    cout << "There are " << result <<
            " triangles in the input graph." << endl;
#endif
}
