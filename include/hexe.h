#ifndef HEXE_H
#define HEXE_H
#include "memkind.h"
#include "stddef.h"
#include "stdlib.h"
#include "prefetch.h"
#include <hwloc.h>


int hexe_init();
int hexe_finalize();
int hexe_alloc_pool(size_t size, int n);
void hexe_free_pool();
void* hexe_alloc_hbw(size_t size, int flag);

int hexe_start();
int hexe_set_threads(int threads);
int hexe_start_fetch_continous(void *start_addr, size_t size, int idx);
int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx);
int hexe_start_write_back_continous(int cache_idx, void *start_addr, size_t size);
void *hexe_get_cache(int idx);
void *hexe_wait_index(int idx, int index);
void *hexe_sync_fetch(int idx);
int hexe_set_prefetch_threads(int threads);
int hexe_set_compute_threads(int threads);
#endif
