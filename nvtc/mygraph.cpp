#include "MyGraph.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <set>
#include <ctime>
#include <cstdlib>
#include <stdint.h>
#include <algorithm>
#include <omp.h>
#include <chrono>
#include <thread>
#include <mutex>
#define BUFFERSIZE 8192*16
#define BATCHSIZE BUFFERSIZE/8
#define INTMAX 2147483647
#define THREADNUM 16

using namespace std;

void foo(){return;};
void loadbatch(MyGraph* G,std::ifstream* fin, int* _temp, bool* state);

MyGraph::MyGraph(const char* file_name){
	// Temporal variables
    std::ifstream fin;
	char buffer[BUFFERSIZE];
	char u_array[4], v_array[4];
	int64_t counter = 0;
	int *u, *v;
	int x, y;
	int node_max = 0;

	// Compute edge num by file length
	fin.open(file_name, ifstream::binary | ifstream::in);
	fin.seekg(0, fin.end);
	edge_num = fin.tellg()/8;
	fin.seekg(0, fin.beg);
	
	//Round 1, Get max id
	cout << "Round 1, Get max id" << endl;
	while (counter + BATCHSIZE < edge_num) {
		fin.read(buffer, BUFFERSIZE);
		u = reinterpret_cast<int*>(buffer);
		for (int j = 0; j < BATCHSIZE; j++) {
			x = *(u + 2 * j);
			y = *(u + 2 * j + 1);
			if (x > node_max) {
				node_max = x;
			}
			if (y > node_max) {
				node_max = y;
			}
		}
		counter = counter + BATCHSIZE;
	}
	for (int64_t i = counter; i < edge_num; i++) {
		fin.read(u_array, 4);
		fin.read(v_array, 4);
		u = reinterpret_cast<int*>(u_array);
		v = reinterpret_cast<int*>(v_array);
		x = *u;
		y = *v;
		if (x > node_max) {
			node_max = x;
		}
		if (y > node_max) {
			node_max = y;
		}
	}
	nodeid_max = node_max;
	
	// Call for mem
	offset = new int64_t[nodeid_max +2];
	degree = new int[nodeid_max + 1];
	neighboor = new int[edge_num];
	lock = new mutex[nodeid_max + 1];
	int* _temp = new int[nodeid_max + 1];

	//Round 2, Get node degree
	cout << "Round 2, Get node degree" << endl;
	fin.seekg(0, fin.beg);
	counter = 0;
	while (counter + BATCHSIZE < edge_num) {
		fin.read(buffer, BUFFERSIZE);
		u = reinterpret_cast<int*>(buffer);
		for (int j = 0; j < BATCHSIZE; j++) {
			x = *(u + 2 * j);
			y = *(u + 2 * j + 1);
			if(x<y)
				_temp[x]++;
			if(x>y)
				_temp[y]++;
		}
		counter = counter + BATCHSIZE;
	}
	for (int64_t i = counter; i < edge_num; i++) {
		fin.read(u_array, 4);
		fin.read(v_array, 4);
		u = reinterpret_cast<int*>(u_array);
		v = reinterpret_cast<int*>(v_array);
		x = *u;
		y = *v;
		if(x<y)
			_temp[x]++;
		if(x>y)
			_temp[y]++;
	}

	offset[0] = 0;
	for (int64_t i = 1; i <= nodeid_max+1; i++) {
		offset[i] = offset[i - 1] + _temp[i - 1];
	}

	//Round 3, Record neighboors
	cout << "Round 3, Record neighboors" << endl;
	fin.seekg(0, fin.beg);
	counter = 0;
	bool thread_state[THREADNUM]={false};
	thread* ths[THREADNUM];
	for(int i=0;i<THREADNUM;i++)
		ths[i] = new thread(foo);
	int i = 0;
	while (counter + BATCHSIZE < edge_num ) {
		if(!thread_state[i]){
			thread_state[i] = true;
			if (ths[i]->joinable())
				ths[i]->join();
			ths[i]->~thread();
			ths[i] = new thread(loadbatch,this,&fin,_temp,thread_state+i);
			counter = counter + BATCHSIZE;
		}
		i = (i+1)%16;	
	}
	bool done = false;
	while(!done){
		for(i=0;i<16;i++){
			if(thread_state[i])
				break;
		}
		if(i==16)
			done = true;
	}
	for (int64_t i = counter; i < edge_num; i++) {
		fin.read(u_array, 4);
		fin.read(v_array, 4);
		u = reinterpret_cast<int*>(u_array);
		v = reinterpret_cast<int*>(v_array);
		x = *u;
		y = *v;
		if(x<y)
		neighboor[offset[x] + degree[x]++] = y;
		if(x>y)
		neighboor[offset[y] + degree[y]++] = x;
	}

	sort_neighboor(_temp);

	#pragma omp parallel for
	for (int64_t i = 0; i <= nodeid_max; i++) {
		int m,n;
		if (_temp[i]>1){
			for(m=0;m<_temp[i];){
				// if(neighboor[offset[i]+m]==i){
				// 	degree[i]--;
				// 	neighboor[offset[i]+m] = INTMAX;
				// 	m++;
				// 	continue;
				// }
				for(n=m+1;n<_temp[i] && neighboor[offset[i]+m]==neighboor[offset[i]+n];n++){
					degree[i]--;
					neighboor[offset[i]+n] = INTMAX;
				}
				m = n;
			}
		}
	}

	sort_neighboor(_temp);
}

bool MyGraph::arc_exist(int u, int v) {
	return false;
}

bool MyGraph::inner_arc_exist(int u, int v, int* d) {
	return false;
}

void MyGraph::sort_neighboor(int* d) {
#pragma omp parallel for
	for (int64_t i = 0; i <= nodeid_max; i++) {
		sort(neighboor + offset[i], neighboor + offset[i] + d[i]);
	}
}

bool MyGraph::arc_exist_sorted(int u, int v) {
	int x, y;
	if (u<v) {
		x = u;
		y = v;
	}
	else {
		x = v;
		y = u;
	}
	return binary_search(neighboor + offset[x], neighboor + offset[x] + degree[x], y);
}

void loadbatch(MyGraph* G,std::ifstream* fin, int* _temp, bool* state){
	char buffer[BUFFERSIZE];
	G->fin_lock.lock();
	fin->read(buffer, BUFFERSIZE);
	G->fin_lock.unlock();
	int* u = reinterpret_cast<int*>(buffer);
	int x,y;
	for (int j = 0; j < BATCHSIZE; j++) {
		x = *(u + 2 * j);
		y = *(u + 2 * j + 1);
		if(x<y){
			G->lock[x].lock();
			G->neighboor[G->offset[x] + G->degree[x]++] = y;
			G->lock[x].unlock();
		}
		if(x>y){
			G->lock[y].lock();
			G->neighboor[G->offset[y] + G->degree[y]++] = x;
			G->lock[y].unlock();
		}	
	}
	*state = false;
	return;
}