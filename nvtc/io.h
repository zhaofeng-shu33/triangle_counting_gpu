// Copyright 2019 zhaofeng-shu33
#pragma once
/// \file
/// \brief input utility
///
#include <vector>
#include <utility>
#include <map>

typedef std::vector< std::pair<int, int> > Edges;
//! \brief construct the static graph from a given file
//! \param[out] arcs the graph data structure which will be initialized,
//! use function _free_ to release the memory
//! \return (node_max_id, edge_num)
std::pair<int, uint64_t> read_binfile_to_arclist(const char* file_name,
  int*& arcs);  // NOLINT(runtime/references)
//! \brief get array split point
//! \param[in] arr the arr sorted in increasing order
//! \param[in] arr_len length of arr
//! \param[in] split_num the number of split
//! \param[out] out_arr has length split_num and contains index of split
//! \return the maximal number of split interval
uint64_t get_split(uint64_t* arr, int arr_len, int split_num,
  uint64_t*& out_arr);  // NOLINT(runtime/references)
//! \brief swap the paired array to single aligned format
void swap_array(int* arr, uint64_t arr_len);
