#ifndef HEXE_H
#define HEXE_H
#include "stddef.h"
#include "stdlib.h"
#include <hwloc.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TOUCH_PAGES 0x1
int hexe_init();
int hexe_finalize();
int hexe_is_init();
int hexe_alloc_pool(size_t size, int n);
void hexe_free_pool();
void* hexe_request_hbw(void* addr, size_t size, int priority); 
void* hexe_request_hbw2(uint64_t  size, int priority); 
int hexe_verify_memory_region(void* addr, size_t size, int flags);
void hexe_free_memory(void *ptr);
int  hexe_bind_requested_memory(const int redistribute);
int hexe_change_priority(void* ptr, int priority);
int hexe_is_in_hbw (void *ptr);
int hexe_start();
int hexe_set_threads(int threads);
int hexe_start_fetch_continous(void *start_addr, size_t size, int idx);
int hexe_start_fetch_strided(void *start_addr, size_t count, size_t block_len, size_t stride,  int idx);
int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx);
int hexe_start_write_back_continous(size_t size, int idx);
int hexe_start_write_back_strided(size_t count, size_t block_len, size_t stride,  int idx);
void *hexe_get_cache_for_write_back(int idx, void *addr);
void *hexe_wait_index(int idx, int index);
void *hexe_sync_fetch(int idx);
int hexe_set_prefetch_threads(int threads);
int hexe_set_compute_threads(int threads);
#ifdef __cplusplus
}
#endif


#endif
