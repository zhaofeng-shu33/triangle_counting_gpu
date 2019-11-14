// Copyright 2019 zhaofeng-shu33
#include "io.h"

#include <algorithm>
#include <fstream>
#if __GNUG__
#include <bits/stdc++.h>
#else
#define INT_MAX 2147483647
#endif

using namespace std;

uint64_t get_edge(std::ifstream& fin) {
    fin.seekg(0, fin.end);
    uint64_t edge_size = fin.tellg();
    fin.seekg(0, fin.beg);    
    if (edge_size % 8 != 0) {
        throw std::logic_error( std::string{} + "not multiply of 8 at " +  __FILE__ +  ":" + std::to_string(__LINE__));
    }
    return edge_size / 8;
}
uint64_t get_split(uint64_t* arr, int arr_len, int split_num, uint64_t*& out_arr) {
    out_arr = new uint64_t[split_num + 1];
    int counter = 0;
    uint64_t max_num = arr[arr_len - 1];
    for (int i = 0; i < split_num; i++) {
        while (arr[counter] < i * max_num / split_num)
            counter++;
        out_arr[i] = arr[counter];
    }
    out_arr[split_num] = max_num;
    max_num = out_arr[1] - out_arr[0];
    for (int i = 1; i < split_num; i++) {
        if(max_num < out_arr[i + 1] - out_arr[i])
            max_num = out_arr[i + 1] - out_arr[i];
    }
    return max_num;
}
// swap_array(arr = {1,2,3,4,5,6},3) -> arr = {1,3,5,2,4,6}
void swap_array(int*& arr, uint64_t arr_len_2) {
    uint64_t empty_pos = 1;
    for (uint64_t i = 1; i < arr_len_2; i++) {
        std::swap(arr[empty_pos], arr[2 * i]);
        empty_pos++;
    }
    std::sort(arr + arr_len_2, arr + 2 * arr_len_2);
}

//! V2 allows node with zero degree
std::pair<int, uint64_t> read_binfile_to_arclist(const char* file_name, int*& arcs){
    std::ifstream fin;
    fin.open(file_name, std::ifstream::binary | std::ifstream::in);
    uint64_t file_size = get_edge(fin);
#if VERBOSE
    std::cout << "num of edges before cleanup: " << file_size << std::endl;
#endif
    arcs = reinterpret_cast<int*>(malloc(2 * sizeof(int) * file_size));
    fin.read(reinterpret_cast<char*>(arcs),
        2 * file_size * sizeof(int));
    int node_num = 0;
    for (int i = 0;
        i < file_size; ++i) {
        if (arcs[2 * i] > node_num) {
            node_num = arcs[2 * i];
        } else if (arcs[2 * i + 1] > node_num) {
            node_num = arcs[2 * i + 1];
        }
        if (arcs[2 * i + 1] == arcs[2 * i]) {
            arcs[2 * i + 1] = INT_MAX;
            arcs[2 * i] = INT_MAX;
        } else if (arcs[2 * i] > arcs[2 * i + 1]) {
            std::swap(arcs[2 * i], arcs[2 * i + 1]);
        }
    }
    // sort arcs
    uint64_t* arcs_start_ptr = reinterpret_cast<uint64_t*>(arcs);
    std::sort(arcs_start_ptr, arcs_start_ptr + file_size);
    // remove the duplicate
    uint64_t* last_value = reinterpret_cast<uint64_t*>(arcs);
    uint64_t j = 1;
    for (uint64_t i = 1; i < file_size - 1; i++) {
        while (*(last_value + j - 1) == *(last_value + i)) {
            arcs[2 * i] = INT_MAX;
            arcs[2 * i + 1] = INT_MAX;
            i++;
        }
        j = i + 1;
    }
    // sort arcs again
    std::sort(arcs_start_ptr, arcs_start_ptr + file_size);
    // find the number of duplicate edges
    uint64_t edges = 0;
    while (edges < file_size) {
        if (arcs[2 * edges] == INT_MAX) {
            break;
        }
        edges++;
    }
    return std::make_pair(node_num + 1, edges);
}

