#ifndef HEXE_H
#define HEXE_H

#ifdef __cplusplus
extern "C" {
#endif
#define TOUCH_PAGES 0x1

#include "hexe_malloc.h"
int hexe_finalize();

int hexe_alloc_pool(size_t size, int n);
void hexe_free_pool();

int hexe_start();
int hexe_set_threads(int threads);
int hexe_start_fetch_continous(void *start_addr, size_t size, int idx);
int hexe_start_fetch_continous_taged(void *start_addr, size_t size, int idx, int tag);
int hexe_start_fetch_strided(void *start_addr, size_t count, size_t block_len, size_t stride,  int idx);
int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx);
int hexe_start_write_back_continous(size_t size, int idx);
int hexe_start_write_back_strided(size_t count, size_t block_len, size_t stride,  int idx);
void *hexe_get_cache_for_write_back(int idx, void *addr);
void *hexe_wait_index(int idx, int index);
void *hexe_sync_fetch(int idx);
void* hexe_sync_fetch_taged(int idx, int tag);
int hexe_set_prefetch_threads(int threads);
int hexe_set_compute_threads(int threads);
#ifdef __cplusplus
}
#endif


#endif
