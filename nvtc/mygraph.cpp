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
#define BUFFERSIZE 8192*64
#define BATCHSIZE BUFFERSIZE/8
#define INTMAX 2147483647
#define THREADNUM 8

using namespace std;

void foo(){return;};
void loadbatch_R2(MyGraph* G,std::ifstream* fin, int* _temp, int* _temp2, mutex* lock, bool* state);
void loadbatch_R3(MyGraph* G,std::ifstream* fin, int* _temp, int* _temp2, mutex* lock, bool* state);
void get_max(int*u, int64_t length, int64_t from, int64_t step, int* out);
void get_degree(int*u, int64_t length, int64_t from, int64_t step, int* temp2);
void get_length(int*u, int64_t length, int64_t from, int64_t step, mutex* lock, int* _temp2, int* _temp);

MyGraph::MyGraph(const char* file_name){
	// Temporal variables
    std::ifstream fin;
	char buffer[BUFFERSIZE];
	char u_array[4], v_array[4];
	int64_t counter = 0;
	int *u, *v;
	int x, y;
	int node_max = 0;
	//uint THREADNUM = thread::hardware_concurrency();
	int* node_max_thread = new int[THREADNUM]{0};
	thread* ths[THREADNUM];
	bool* thread_state = new bool[THREADNUM]{false};
	int i = 0;

	// Compute edge num by file length
	fin.open(file_name, ifstream::binary | ifstream::in);
	fin.seekg(0, fin.end);
	edge_num = fin.tellg()/8;
	fin.seekg(0, fin.beg);
	
	//Round 1, Get max id
	cout << "Round 1, Get max id" << endl;
	char* entire_data = new char[edge_num*8];
	fin.read(entire_data, edge_num*8);
	u = reinterpret_cast<int*>(entire_data);
	for(int i=0;i<THREADNUM;i++)
		ths[i] = new thread(get_max, u, edge_num*2, i, THREADNUM, node_max_thread+i);
	for(i=0;i<THREADNUM;i++){
		ths[i]->join();
		if(node_max_thread[i]>nodeid_max)
			nodeid_max = node_max_thread[i];
	}

	//Round 2, Get node degree, use this to decide where a edge should store
	cout << "Round 2, Get degree" << endl;
	int* _temp2 = new int[nodeid_max + 1];
	for(int i=0;i<THREADNUM;i++)
		ths[i] = new thread(get_degree, u, edge_num*2, 2*i, 2*THREADNUM, _temp2);
	for(i=0;i<THREADNUM;i++){
		ths[i]->join();
	}

	//Round 2, Get offset
	cout << "Round 3, Get offset" << endl;
	mutex* lock = new mutex[nodeid_max + 1];
	int* _temp = new int[nodeid_max + 1];
	for(int i=0;i<THREADNUM;i++)
		ths[i] = new thread(get_length, u, edge_num*2, 2*i, 2*THREADNUM, lock, _temp2, _temp);
	for(i=0;i<THREADNUM;i++){
		ths[i]->join();
	}

	delete[] entire_data;
	degree = new int[nodeid_max + 1];
	neighboor = new int[edge_num];
	offset = new int64_t[nodeid_max +2];
	offset[0] = 0;
	for (int64_t i = 1; i <= nodeid_max+1; i++) {
		offset[i] = offset[i - 1] + _temp[i - 1];
	}
	
	// //Round 2, Get node degree
	// cout << "Round 2, Get node degree" << endl;
	// fin.seekg(0, fin.beg);
	// counter = 0;
	// i = 0;
	// while (counter + BATCHSIZE < edge_num ) {
	// 	if(!thread_state[i]){
	// 		thread_state[i] = true;	
	// 		if (ths[i]->joinable())
	// 			ths[i]->join();
	// 		ths[i]->~thread();
	// 		ths[i] = new thread(loadbatch_R2,this,&fin,_temp,_temp2,lock,thread_state+i);
	// 		counter = counter + BATCHSIZE;
	// 	}
	// 	i = (i+1)%THREADNUM;	
	// }
	// for(i=0;i<THREADNUM;i++){
	// 	ths[i]->join();
	// }
	// fin.read(buffer, (edge_num-counter)*8);
	// u = reinterpret_cast<int*>(buffer);
	// for (int64_t i = 0; i < edge_num-counter; i++) {
	// 	x = *(u + 2 * i);
	// 	y = *(u + 2 * i + 1);
	// 	if( x!=y && (_temp2[x]<_temp2[y] || (_temp2[x]==_temp2[y] && x<y) ) )
	// 		_temp[x]++;
	// 	if( x!=y && (_temp2[x]>_temp2[y] || (_temp2[x]==_temp2[y] && x>y) ) )
	// 		_temp[y]++;
	// }
	
	// offset[0] = 0;
	// for (int64_t i = 1; i <= nodeid_max+1; i++) {
	// 	offset[i] = offset[i - 1] + _temp[i - 1];
	// }

	//Round 3, Record neighboors
	cout << "Round 4, Record neighboors" << endl;
	fin.seekg(0, fin.beg);
	counter = 0;
	i = 0;
	while (counter + BATCHSIZE < edge_num ) {
		if(!thread_state[i]){
			thread_state[i] = true;
			if (ths[i]->joinable())
				ths[i]->join();
			ths[i]->~thread();
			ths[i] = new thread(loadbatch_R3,this,&fin,_temp,_temp2,lock,thread_state+i);
			counter = counter + BATCHSIZE;
		}
		i = (i+1)%THREADNUM;	
	}
	for(i=0;i<THREADNUM;i++){
		ths[i]->join();
	}
	fin.read(buffer, (edge_num-counter)*8);
	u = reinterpret_cast<int*>(buffer);
	for (int64_t i = 0; i < edge_num-counter; i++) {
		x = *(u + 2 * i);
		y = *(u + 2 * i + 1);
		if( x!=y && (_temp2[x]<_temp2[y] || (_temp2[x]==_temp2[y] && x<y) ) )
			neighboor[offset[x] + degree[x]++] = y;
		if( x!=y && (_temp2[x]>_temp2[y] || (_temp2[x]==_temp2[y] && x>y) ) )
			neighboor[offset[y] + degree[y]++] = x;
	}

	delete [] lock;
	neighboor_start = new int[edge_num];
	#pragma omp parallel for
	for (int64_t i = 0; i <= nodeid_max; i++) {
		int64_t start = offset[i];
		for (int j=0; j<degree[i];j++)
			neighboor_start[start+j] = i;
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
	// for(int i=0;i<edge_num;i++)
	// cout<<neighboor[i]<<" ";
	// cout<<endl;
	// for(int i=0;i<edge_num;i++)
	// cout<<neighboor_start[i]<<" ";
	// cout<<endl;
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

void get_max(int*u, int64_t length, int64_t from, int64_t step, int* out){
	int max = 0;
	for(int64_t i = from;i<length;i+=step){
		if(u[i]>max)
			max = u[i];
	}
	*out = max;
}
void get_degree(int*u, int64_t length, int64_t from, int64_t step, int* temp2){
	for(int64_t i = from;i<length;i+=step){
		temp2[u[i]]++;
		temp2[u[i+1]]++;
	}
}
void get_length(int*u, int64_t length, int64_t from, int64_t step, mutex* lock, int* _temp2, int* _temp){
	int x,y;
	for(int64_t i = from;i<length;i+=step){
		x = *(u + i);
		y = *(u + i + 1);
		if( x!=y && (_temp2[x]<_temp2[y] || (_temp2[x]==_temp2[y] && x<y) ) ){
			lock[x].lock();
			_temp[x]++;
			lock[x].unlock();
		}
		if( x!=y && (_temp2[x]>_temp2[y] || (_temp2[x]==_temp2[y] && x>y) ) ){
			lock[y].lock();
			_temp[y]++;
			lock[y].unlock();
		}	
	}
}

void loadbatch_R2(MyGraph* G,std::ifstream* fin, int* _temp, int* _temp2, mutex* lock, bool* state){
	char buffer[BUFFERSIZE];
	G->fin_lock.lock();
	fin->read(buffer, BUFFERSIZE);
	G->fin_lock.unlock();
	int* u = reinterpret_cast<int*>(buffer);
	int x,y;
	for (int j = 0; j < BATCHSIZE; j++) {
		x = *(u + 2 * j);
		y = *(u + 2 * j + 1);
		if( x!=y && (_temp2[x]<_temp2[y] || (_temp2[x]==_temp2[y] && x<y) ) ){
			lock[x].lock();
			_temp[x]++;
			lock[x].unlock();
		}
		if( x!=y && (_temp2[x]>_temp2[y] || (_temp2[x]==_temp2[y] && x>y) ) ){
			lock[y].lock();
			_temp[y]++;
			lock[y].unlock();
		}	
	}
	*state = false;
	return;
}

void loadbatch_R3(MyGraph* G,std::ifstream* fin, int* _temp, int* _temp2, mutex* lock, bool* state){
	char buffer[BUFFERSIZE];
	G->fin_lock.lock();
	fin->read(buffer, BUFFERSIZE);
	G->fin_lock.unlock();
	int* u = reinterpret_cast<int*>(buffer);
	int x,y;
	for (int j = 0; j < BATCHSIZE; j++) {
		x = *(u + 2 * j);
		y = *(u + 2 * j + 1);
		if( x!=y && (_temp2[x]<_temp2[y] || (_temp2[x]==_temp2[y] && x<y) ) ){
			lock[x].lock();
			G->neighboor[G->offset[x] + G->degree[x]++] = y;
			lock[x].unlock();
		}
		if( x!=y && (_temp2[x]>_temp2[y] || (_temp2[x]==_temp2[y] && x>y) ) ){
			lock[y].lock();
			G->neighboor[G->offset[y] + G->degree[y]++] = x;
			lock[y].unlock();
		}	
	}
	*state = false;
	return;
}
int64_t get_split_v2(int64_t* offset, int nodeid_max, int split_num, int64_t cpu_offset, int64_t*& out){
	int64_t max_length = 0;
	out = new int64_t[split_num+1];
	out[0] = cpu_offset;
	for(int i=1;i<split_num;i++){
		int64_t target = out[i-1]+(offset[nodeid_max+1]-cpu_offset)/split_num;
		out[i] = *lower_bound(offset,offset+nodeid_max+2,target);
	}
	out[split_num] = offset[nodeid_max+1];
	for(int i=1;i<=split_num;i++){
		if(out[i]-out[i-1]>max_length)
			max_length = out[i]-out[i-1];
	}
	return max_length;
}
void cpu_counting_edge_first_v2(MyGraph* g, int64_t cpu_offset, int64_t* out){
    int64_t sum=0;
    int iit = 0;
    int jit = 0;
    int d = 0;
    int i,j;
    #pragma omp parallel for schedule(dynamic,1024) reduction(+:sum) private(iit,jit,d,i,j)
    for (int64_t k=0;k<cpu_offset;k++){
        i = g->neighboor_start[k];
        j = g->neighboor[k];
        if(j==INTMAX)
        continue;
        iit = 0;
        jit = 0;
            while(iit<g->degree[i] && jit<g->degree[j]){
                d = g->neighboor[g->offset[i]+iit]-g->neighboor[g->offset[j]+jit];
                if(d==0){
                    sum++;
                    iit++;
                    jit++;
                }
                if(d<0){
                    iit++;
                }
                if(d>0){
                    jit++;
                }
            }
        }
    *out = sum;
	cout<<"CPU Done."<<endl;
}