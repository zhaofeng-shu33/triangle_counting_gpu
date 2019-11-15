// Copyright 2019 zhaofeng-shu33
#pragma once
#include <vector>
#include <utility>
#include <map>

typedef std::vector< std::pair<int, int> > Edges;

std::pair<int, uint64_t> read_binfile_to_arclist(const char* file_name,
  int*& arcs);
uint64_t get_split(uint64_t* arr, int arr_len, int split_num,
  uint64_t*& out_arr);
void swap_array(int*& arr, uint64_t arr_len_2);

