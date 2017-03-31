#ifndef HEXE_H
#define HEXE_H

#ifdef __cplusplus
extern "C" {
#endif
#define TOUCH_PAGES 0x1

#include "hexe_malloc.h"


/* finish hexe, behavior of all other functions is undefined after exit */
int hexe_finalize();

/*allocate prefetch pool, *n* pools of size *size*/
int hexe_alloc_pool(size_t size, int n);
/*free the allocation pool */

/* free the  prefetch pool */
void hexe_free_pool();

/*start hexe (distribute threads, if prefetch_threads >0, start the prefetch- wait- thread*/
int hexe_start();

/* set the number of prefetch threads - the threads are  distributed when hexe_start is called */
int hexe_set_prefetch_threads(int threads);
/* set the number of compute threads */
int hexe_set_compute_threads(int threads);

/*start  asyncronous prefetchung from *start_addr* of *size* data, use pool *idx*. IDx must be
 * between 0 and n_pools -1 */
int hexe_start_fetch_continous(void *start_addr, size_t size, int idx);
/*start a taged prefetching (for odering */
int hexe_start_fetch_continous_taged(void *start_addr, size_t size, int idx, int tag);
/*start a strided prefetch. The data will be on contious addresses in the cache */
int hexe_start_fetch_strided(void *start_addr, size_t count, size_t block_len, size_t stride,  int idx);
/*non -continous prefetching, requires an list of offstes an a size of the individual elements not
 * testes*/
int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx);
/*write back data from cache to address. note: before "hexe_get_cache_for writeback* must be used to
 * set the destination address (or prefetch, to write back to the same destination */
int hexe_start_write_back_continous(size_t size, int idx);
/*strided write back. note that the application exect the data to have continous addresses */
int hexe_start_write_back_strided(size_t count, size_t block_len, size_t stride,  int idx);
/* get a write-cache (the cache is bounded to addres *addr*. all other operations on the cache with
 * "idx" are syncronized before the function returns*/
void *hexe_get_cache_for_write_back(int idx, void *addr);

/* synchronizes a prefetch operation on cache *idx* and returns the address of the cache*/
void *hexe_sync_fetch(int idx);
/* taged syncrhonization */
void* hexe_sync_fetch_taged(int idx, int tag);
#ifdef __cplusplus
}
#endif


#endif
