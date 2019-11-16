#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <map>
#include <ctime>
#include <cstdlib>
#include <stdint.h>
#include <algorithm>
#include <omp.h>
#include <chrono>
#include <mutex>

class TRCountingGraph{
	public:
		// Construct Function
		TRCountingGraph(const char* file_name);
		
		//Query arc exist using go-through check.
		bool arc_exist(int u, int v);

		//Query arc exist using binary search.
		bool arc_exist_sorted(int u, int v);

		// node ID -> neighboor table offset from int* neighboor.
		int64_t* offset;

		// node ID -> Node degree.
		int* degree;

		// neighboor table starting address
		int* neighboor;
		int* neighboor_start;

		// maximum node id
		int64_t nodeid_max;

		// total number of edges
		int64_t edge_num;

		//mutex* lock;
		std::mutex fin_lock;

	private:
		void sort_neighboor(int* d);
		bool inner_arc_exist(int u, int v, int* d);
};
