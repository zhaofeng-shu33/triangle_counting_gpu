#include "gpu.h"
#include "graph.h"
#include "timer.h"

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
    MyGraph myGraph(argv[2]);
#else    
    const char* io_hint = std::getenv("DATAIO");
    Edges edges;
    std::pair<int, int> info_pair;
    if(io_hint == NULL or strcmp(io_hint, "V1") == 0) {
        info_pair = read_binfile_to_arclist(argv[2], edges);
    }
    else if (strcmp(io_hint, "V2") == 0) {
        info_pair = read_binfile_to_arclist_v2(argv[2], edges);
    }
    else {
        info_pair = read_binfile_to_arclist(argv[2], edges);
    }
#if VERBOSE
    std::cout << "Num of Nodes: " << info_pair.first << std::endl;
    std::cout << "Num of Edges: " << info_pair.second << std::endl;
#endif
#endif

#if TRCOUNTING
#if TIMECOUNTING
    t->Reset();
    PreInitGpuContext(0);
    t->Done("Preinitialize context for device 0");
#endif
    uint64_t result = 0;
#if SECONDVERSION
    result = GpuForward_v2(myGraph);
#else
    result = GpuForward(edges);
#endif
    cout << "There are " << result <<
            " triangles in the input graph." << endl;
#endif
}
