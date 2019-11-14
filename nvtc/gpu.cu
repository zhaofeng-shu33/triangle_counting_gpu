#include "gpu.h"

#include "gpu-thrust.h"
#include "timer.h"

#include <cuda_profiler_api.h>
#include <cuda_runtime.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "MyGraph.h"
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

__global__ void CalculateFlags(int m, int* edges, int* nodes, bool* flags) {
  int from = blockDim.x * blockIdx.x + threadIdx.x;
  int step = gridDim.x * blockDim.x;
  for (int i = from; i < m; i += step) {
    int a = edges[2 * i];
    int b = edges[2 * i + 1];
    int deg_a = nodes[a + 1] - nodes[a];
    int deg_b = nodes[b + 1] - nodes[b];
    flags[i] = (deg_a < deg_b) || (deg_a == deg_b && a < b);
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

__global__ void UnzipEdgesSplitFirst(int m, int* edges, int* unzipped_edges, uint64_t start, uint64_t end) {
  int from = blockDim.x * blockIdx.x + threadIdx.x;
  int step = gridDim.x * blockDim.x;
  for (int i = from; i < end - start; i += step) {
    unzipped_edges[i] = edges[2 * (i + start)];
  }
}

__global__ void UnzipEdgesSplitSecond(int m, int* edges, int* unzipped_edges, uint64_t start, uint64_t end) {
  int from = blockDim.x * blockIdx.x + threadIdx.x;
  int step = gridDim.x * blockDim.x;
  for (int i = from; i < end - start; i += step) {
    unzipped_edges[i] = edges[2 * (i + start) + 1];
  }
}

__global__ void CalculateTriangles_v2(int n, int* dev_neighbor, int64_t* dev_offset, int* dev_length, uint64_t* results,int deviceCount = 1, int deviceIdx = 0) {
   int from =
    gridDim.x * blockDim.x * deviceIdx +
    blockDim.x * blockIdx.x +
    threadIdx.x;
  int step = deviceCount * gridDim.x * blockDim.x;
  
  uint64_t count = 0;
  for (int i = from; i < n; i += step) {	
    for(int u = dev_offset[i]; u <= dev_offset[i]+dev_length[i]-1; u++){
    	int j = dev_neighbor[u];
        int64_t j_it = dev_offset[j];
        int64_t i_it = dev_offset[i];
	
 	while(j_it <= dev_offset[j]+dev_length[j]-1 && i_it <= dev_offset[i]+dev_length[i]-1){
        int d = dev_neighbor[i_it] - dev_neighbor[j_it];
		if ( d == 0 ){
			count++;
			i_it++;
			j_it++;
		}
		if (d < 0)
			i_it++; 
		if (d > 0)
			j_it++;
	}
  }
  }
  results[blockDim.x * blockIdx.x + threadIdx.x] = count;
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

Edges RemoveBackwardEdgesCPU(const Edges& unordered_edges) {
  int n = NumVertices(unordered_edges);
  int m = unordered_edges.size();

  vector<int> deg(n);
  for (int i = 0; i < m; ++i)
    ++deg[unordered_edges[i].first];

  vector< pair<int, int> > edges;
  edges.reserve(m / 2);
  for (int i = 0; i < m; ++i) {
    int s = unordered_edges[i].first, t = unordered_edges[i].second;
    if (deg[s] > deg[t] || (deg[s] == deg[t] && s > t))
      edges.push_back(make_pair(s, t));
  }

  return edges;
}

uint64_t MultiGPUCalculateTriangles(
    int n, int m, int* dev_edges, int* dev_nodes, int device_count) {
  vector<int*> multi_dev_edges(device_count);
  vector<int*> multi_dev_nodes(device_count);

  multi_dev_edges[0] = dev_edges;
  multi_dev_nodes[0] = dev_nodes;

  for (int i = 1; i < device_count; ++i) {
    CUCHECK(cudaSetDevice(i));
    CUCHECK(cudaMalloc(&multi_dev_edges[i], m * 2 * sizeof(int)));
    CUCHECK(cudaMalloc(&multi_dev_nodes[i], (n + 1) * sizeof(int)));
    int dst = i, src = (i + 1) >> 2;
    CUCHECK(cudaMemcpyPeer(
          multi_dev_edges[dst], dst, multi_dev_edges[src], src,
          m * 2 * sizeof(int)));
    CUCHECK(cudaMemcpyPeer(
          multi_dev_nodes[dst], dst, multi_dev_nodes[src], src,
          (n + 1) * sizeof(int)));
  }

  vector<int> NUM_BLOCKS(device_count);
  vector<uint64_t*> multi_dev_results(device_count);

  for (int i = 0; i < device_count; ++i) {
    CUCHECK(cudaSetDevice(i));
    NUM_BLOCKS[i] = NUM_BLOCKS_PER_MP * NumberOfMPs();
    CUCHECK(cudaMalloc(
          &multi_dev_results[i],
          NUM_BLOCKS[i] * NUM_THREADS * sizeof(uint64_t)));
  }

  for (int i = 0; i < device_count; ++i) {
    CUCHECK(cudaSetDevice(i));
    CUCHECK(cudaFuncSetCacheConfig(CalculateTriangles, cudaFuncCachePreferL1));
    CalculateTriangles<<<NUM_BLOCKS[i], NUM_THREADS>>>(
        m, multi_dev_edges[i], multi_dev_nodes[i], multi_dev_results[i],
        device_count, i);
  }

  uint64_t result = 0;

  for (int i = 0; i < device_count; ++i) {
    CUCHECK(cudaSetDevice(i));
    CUCHECK(cudaDeviceSynchronize());
    result += SumResults(NUM_BLOCKS[i] * NUM_THREADS, multi_dev_results[i]);
  }

  for (int i = 1; i < device_count; ++i) {
    CUCHECK(cudaSetDevice(i));
    CUCHECK(cudaFree(multi_dev_edges[i]));
    CUCHECK(cudaFree(multi_dev_nodes[i]));
  }

  for (int i = 0; i < device_count; ++i) {
    CUCHECK(cudaSetDevice(i));
    CUCHECK(cudaFree(multi_dev_results[i]));
  }

  cudaSetDevice(0);
  return result;
}

uint64_t GpuForward(int* edges, int num_nodes, uint64_t num_edges) {
  return MultiGpuForward(edges, 1, num_nodes, num_edges);
}

uint64_t GpuForward_v2(const MyGraph& myGraph){
    int64_t* dev_offset;
    int* dev_neighbor;
    int* dev_length;
    CUCHECK(cudaMalloc(&dev_offset, (myGraph.nodeid_max + 2) * sizeof(int64_t)));
    CUCHECK(cudaMemcpyAsync(
       dev_offset, myGraph.offset, (myGraph.nodeid_max + 2) * sizeof(int64_t), cudaMemcpyHostToDevice));
    CUCHECK(cudaDeviceSynchronize());
    CUCHECK(cudaMalloc(&dev_length, (myGraph.nodeid_max + 1) * sizeof(int)));
    CUCHECK(cudaMemcpyAsync(
      dev_length, myGraph.length, (myGraph.nodeid_max + 1) * sizeof(int), cudaMemcpyHostToDevice));
    CUCHECK(cudaDeviceSynchronize());
    CUCHECK(cudaMalloc(&dev_neighbor, ( myGraph.edge_num) * sizeof(int)));
    CUCHECK(cudaMemcpyAsync(
       dev_neighbor, myGraph.neighboor, ( myGraph.edge_num) * sizeof(int), cudaMemcpyHostToDevice));
    CUCHECK(cudaDeviceSynchronize());
    const int NUM_BLOCKS = NUM_BLOCKS_PER_MP * NumberOfMPs();	
    uint64_t* dev_results;
    CUCHECK(cudaMalloc(&dev_results,
          NUM_BLOCKS * NUM_THREADS * sizeof(uint64_t)));

    CalculateTriangles_v2<<<NUM_BLOCKS, NUM_THREADS>>>(
        myGraph.nodeid_max + 1, dev_neighbor, dev_offset, dev_length, dev_results);
    CUCHECK(cudaDeviceSynchronize());
    uint64_t result = SumResults(NUM_BLOCKS * NUM_THREADS, dev_results);
    return result;
}
//! get the split_num based of edge num and node num
int GetSplitNum(int num_nodes, uint64_t num_edges) {
    size_t mem = GlobalMemory(); // in Byte
    mem -= num_nodes * 8; // uint64_t
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

  if (device_count == 1) {
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
  } else {
    result = MultiGPUCalculateTriangles(
        n, m, dev_edges, dev_nodes, device_count);
#if TIMECOUNTING        
    timer->Done("Calculate triangles on multi GPU");
#endif    
  }

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
