#ifndef HEXE_H
#define HEXE_H
#include "memkind.h"
#include "stddef.h"
#include "stdlib.h"
#include "prefetch.h"
#include <hwloc.h>



int hexe_init(int n_threads);
int hexe_finalize();
int hexe_alloc_pool(size_t size, int n);
int hexe_start();
int hexe_start_fetch_continous(void *start_addr, size_t size, int idx);
int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx);
int hexe_start_write_back_continous(int cache_idx, void *start_addr, size_t size);
int hexe_set_topology(hwloc_cpuset_t *cpusets, hwloc_topology_t topology); 
void *hexe_get_cache(int idx);
void *hexe_wait_index(int idx, int index);
void *hexe_sync_fetch(int idx);
#endif
