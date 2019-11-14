// Copyright 2019 zhaofeng-shu33
#include "counting_gpu.h"

#include "gpu-thrust.h"
#include "timer.h"

#include <cuda_profiler_api.h>
#include <cuda_runtime.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std;

#define NUM_THREADS 64
#define NUM_BLOCKS_GENERIC 112
#define NUM_BLOCKS_PER_MP 8

template<bool ZIPPED>
__global__ void CalculateNodePointers(int n, int m, int* edges, int* nodes) {
  int from = blockDim.x * blockIdx.x + threadIdx.x;
  int step = gridDim.x * blockDim.x;
  for (int i = from; i <= m; i += step) {
    int prev = i > 0 ? edges[ZIPPED ? (2 * (i - 1) + 1) : (m + i - 1)] : -1;
    int next = i < m ? edges[ZIPPED ? (2 * i + 1) : (m + i)] : n;
    for (int j = prev + 1; j <= next; ++j)
      nodes[j] = i;
  }
}

__global__ void UnzipEdges(int m, int* edges, int* unzipped_edges) {
  int from = blockDim.x * blockIdx.x + threadIdx.x;
  int step = gridDim.x * blockDim.x;
  for (int i = from; i < m; i += step) {
    unzipped_edges[i] = edges[2 * i];
    unzipped_edges[m + i] = edges[2 * i + 1];
  }
}

__global__ void CalculateTriangles(
    int m, const int* __restrict__ edges, const int* __restrict__ nodes,
    uint64_t* results, int deviceCount = 1, int deviceIdx = 0) {
  int from =
    gridDim.x * blockDim.x * deviceIdx +
    blockDim.x * blockIdx.x +
    threadIdx.x;
  int step = deviceCount * gridDim.x * blockDim.x;
  uint64_t count = 0;

  for (int i = from; i < m; i += step) {
    int u = edges[i], v = edges[m + i];

    int u_it = nodes[u], u_end = nodes[u + 1];
    int v_it = nodes[v], v_end = nodes[v + 1];

    int a = edges[u_it], b = edges[v_it];
    while (u_it < u_end && v_it < v_end) {
      int d = a - b;
      if (d <= 0)
        a = edges[++u_it];
      if (d >= 0)
        b = edges[++v_it];
      if (d == 0)
        ++count;
    }
  }

  results[blockDim.x * blockIdx.x + threadIdx.x] = count;
}

__global__ void CalculateTriangleSplit(int edge_len,
    const int* __restrict__ edges, const int* __restrict__ edges_i, const int* __restrict__ edges_j, const uint64_t* __restrict__ nodes,
    uint64_t* results, const uint64_t* __restrict__ dev_node_index, int i, int j) {
  int from =
    blockDim.x * blockIdx.x +
    threadIdx.x;
  int step = gridDim.x * blockDim.x;
  uint64_t count = 0;
  // itering over edges_i
  for (uint64_t r = from; r < edge_len; r += step) {
    int u = edges[r], v = edges[r + edge_len];
    if(nodes[u] >= dev_node_index[i+1] || nodes[u + 1] < dev_node_index[i] || nodes[v] >= dev_node_index[j + 1] || nodes[v + 1] < dev_node_index[j])
        continue;
    uint64_t u_it = nodes[u] - dev_node_index[i];
    uint64_t u_end = nodes[u + 1] - dev_node_index[i];
    uint64_t v_it = nodes[v] - dev_node_index[j];
    uint64_t v_end = nodes[v + 1] - dev_node_index[j];
    // if u_it or v_it not in edges, continue the loop
    int a = edges_i[u_it], b = edges_j[v_it];
    while (u_it < u_end && v_it < v_end) {
      int d = a - b;
      if (d <= 0)
        a = edges_i[++u_it];
      if (d >= 0)
        b = edges_j[++v_it];
      if (d == 0)
        ++count;
    }
  }

  results[blockDim.x * blockIdx.x + threadIdx.x] = count;
}
void CudaAssert(cudaError_t status, const char* code, const char* file, int l) {
  if (status == cudaSuccess) return;
  cerr << "Cuda error: " << code << ", file " << file << ", line " << l << endl;
  exit(1);
}

#define CUCHECK(x) CudaAssert(x, #x, __FILE__, __LINE__)

int NumberOfMPs() {
  int dev, val;
  CUCHECK(cudaGetDevice(&dev));
  CUCHECK(cudaDeviceGetAttribute(&val, cudaDevAttrMultiProcessorCount, dev));
  return val;
}

size_t GlobalMemory() {
  int dev;
  cudaDeviceProp prop;
  CUCHECK(cudaGetDevice(&dev));
  CUCHECK(cudaGetDeviceProperties(&prop, dev));
  return prop.totalGlobalMem;
}

uint64_t GpuForward(int* edges, int num_nodes, uint64_t num_edges) {
  return MultiGpuForward(edges, 1, num_nodes, num_edges);
}

//! get the split_num based of edge num and node num
int GetSplitNum(int num_nodes, uint64_t num_edges) {
    uint64_t mem = (uint64_t)GlobalMemory(); // in Byte
    mem -= (uint64_t)num_nodes * 8; // uint64_t
    return (1 + 16 * num_edges / mem);
}

uint64_t GpuForwardSplit(int* edges, int num_nodes, uint64_t num_edges, int split_num) {
#if TIMECOUNTING
  Timer* timer = Timer::NewTimer();
#endif
  CUCHECK(cudaSetDevice(0));
  const int NUM_BLOCKS = NUM_BLOCKS_PER_MP * NumberOfMPs();

  uint64_t m = num_edges;
  int n = num_nodes;

  int *dev_edges, *dev_edges_i, *dev_edges_j;
  uint64_t* dev_nodes;
  uint64_t* host_nodes;
  swap_array(edges, num_edges);
  CUCHECK(cudaMalloc(&dev_nodes, (n + 1) * sizeof(uint64_t)));
  // compute node pointers in CPU
  host_nodes = new uint64_t [n + 1];
  // Calculate NodePointers
  for (uint64_t i = 0; i <= m; i++) {
     int prev = i > 0 ? edges[m + i - 1] : -1;
     int next = i < m ? edges[m + i] : n;
     for (int j = prev + 1; j <= next; j++)
       host_nodes[j] = i;  
  }
  // copy node pointers from CPU memory to GPU memory
  CUCHECK(cudaMemcpyAsync(
      dev_nodes, host_nodes, (n + 1) * sizeof(uint64_t), cudaMemcpyHostToDevice));
  CUCHECK(cudaDeviceSynchronize());

  uint64_t result = 0;
   
  // calculate split index in host_nodes which makes the split even
  uint64_t* node_index = new uint64_t[split_num + 1];
  uint64_t max_len = get_split(host_nodes, n + 1, split_num, node_index);
  CUCHECK(cudaMalloc(&dev_edges, 2 * sizeof(int) * max_len));
  CUCHECK(cudaMalloc(&dev_edges_i, sizeof(int) * max_len));
  CUCHECK(cudaMalloc(&dev_edges_j, sizeof(int) * max_len));
  uint64_t* dev_results;
  CUCHECK(cudaMalloc(&dev_results,
	  NUM_BLOCKS * NUM_THREADS * sizeof(uint64_t)));
  cudaFuncSetCacheConfig(CalculateTriangleSplit, cudaFuncCachePreferL1);

  uint64_t* dev_node_index;
  CUCHECK(cudaMalloc(&dev_node_index, (split_num + 1) * sizeof(uint64_t)));
  CUCHECK(cudaMemcpy(dev_node_index, node_index, (split_num + 1)* sizeof(uint64_t), cudaMemcpyHostToDevice));
  for(int t = 0; t < split_num; t++)
    for(int i = 0; i < split_num; i++)
      for(int j = 0; j < split_num; j++){
          uint64_t data_offset = node_index[t + 1] - node_index[t];
          CUCHECK(cudaMemcpy(dev_edges, edges + node_index[t], sizeof(int) * data_offset, cudaMemcpyHostToDevice));
          CUCHECK(cudaMemcpy(dev_edges + data_offset, edges + m + node_index[t], sizeof(int) * data_offset,
           cudaMemcpyHostToDevice));
          CUCHECK(cudaMemcpy(dev_edges_i, edges + node_index[i], sizeof(int) * (node_index[i + 1] - node_index[i]),
            cudaMemcpyHostToDevice));
          CUCHECK(cudaMemcpy(dev_edges_j, edges + node_index[j], sizeof(int) * (node_index[j + 1] - node_index[j]),
            cudaMemcpyHostToDevice));
          // node id dev_node_index[i]~dev_node_index[i+1] and dev_node_index[j]~dev_node_index[j+1]
 	    CalculateTriangleSplit<<<NUM_BLOCKS, NUM_THREADS>>>(
		data_offset, dev_edges, dev_edges_i, dev_edges_j, dev_nodes, dev_results, dev_node_index, i, j);
	    CUCHECK(cudaDeviceSynchronize());
	    // Reduce
	  result += SumResults(NUM_BLOCKS * NUM_THREADS, dev_results);
    }
#if TIMECOUNTING    
  timer->Done("Calculate triangles used time: ");
#endif
  CUCHECK(cudaFree(dev_results));
  CUCHECK(cudaFree(dev_edges));
  CUCHECK(cudaFree(dev_edges_i));
  CUCHECK(cudaFree(dev_edges_j));
  CUCHECK(cudaFree(dev_nodes));
#if TIMECOUNTING
  delete timer;
#endif
  delete node_index;
  free(host_nodes);
  return result;
}

uint64_t MultiGpuForward(int* edges, int device_count, int num_nodes, uint64_t num_edges) {
#if TIMECOUNTING
  Timer* timer = Timer::NewTimer();
#endif
  CUCHECK(cudaSetDevice(0));
  const int NUM_BLOCKS = NUM_BLOCKS_PER_MP * NumberOfMPs();

  uint64_t m = num_edges;
  int n = num_nodes;

  int* dev_edges;
  int* dev_nodes;

  
  int* dev_temp;
  CUCHECK(cudaMalloc(&dev_temp, m * 2 * sizeof(int)));
  CUCHECK(cudaMemcpyAsync(
      dev_temp, edges, m * 2 * sizeof(int), cudaMemcpyHostToDevice));
  CUCHECK(cudaDeviceSynchronize());
  // Memcpy edges from host to device
  SortEdges(m, dev_temp);
  CUCHECK(cudaDeviceSynchronize());
  // Sort edges

  CUCHECK(cudaMalloc(&dev_edges, m * 2 * sizeof(int)));
  UnzipEdges<<<NUM_BLOCKS, NUM_THREADS>>>(m, dev_temp, dev_edges);
  CUCHECK(cudaFree(dev_temp));
  CUCHECK(cudaDeviceSynchronize());
  // Unzip edges


  CUCHECK(cudaMalloc(&dev_nodes, (n + 1) * sizeof(int)));
  CalculateNodePointers<false><<<NUM_BLOCKS, NUM_THREADS>>>(
      n, m, dev_edges, dev_nodes);
  CUCHECK(cudaDeviceSynchronize());
  // Calculate nodes array for one-way unzipped edges
  uint64_t result = 0;

  
  uint64_t* dev_results;
  CUCHECK(cudaMalloc(&dev_results,
        NUM_BLOCKS * NUM_THREADS * sizeof(uint64_t)));
  cudaFuncSetCacheConfig(CalculateTriangles, cudaFuncCachePreferL1);
  cudaProfilerStart();
  CalculateTriangles<<<NUM_BLOCKS, NUM_THREADS>>>(
      m, dev_edges, dev_nodes, dev_results);
  CUCHECK(cudaDeviceSynchronize());
  cudaProfilerStop();
  // Reduce
  result = SumResults(NUM_BLOCKS * NUM_THREADS, dev_results);
#if TIMECOUNTING    
  timer->Done("Calculate triangles used time: ");
#endif
  CUCHECK(cudaFree(dev_results));
  

  CUCHECK(cudaFree(dev_edges));
  CUCHECK(cudaFree(dev_nodes));
#if TIMECOUNTING
  delete timer;
#endif
  return result;
}

void PreInitGpuContext(int device) {
  CUCHECK(cudaSetDevice(device));
  CUCHECK(cudaFree(NULL));
}
