#include "prefetch.h"
#include "hexe.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

struct hexe{
	hwloc_cpuset_t *cpusets;
	hwloc_topology_t topology;
	prefetch_thread_t thread;
	void **cache_pool;
	void *cache;
	size_t cache_size;
	int start_id;
	int ncaches;
	int prefetch_threads;
        prefetch_handle_t* handle;
};



struct hexe *prefetcher;


int hexe_init( int threads )
{
    prefetcher =(struct hexe*)  malloc(sizeof(struct hexe));
    memset(prefetcher, 0, sizeof(struct hexe));
    prefetcher->prefetch_threads = threads;
}

int hexe_finalize()
{
  finish_thread( prefetcher->thread);

}
int hexe_set_topology(hwloc_cpuset_t *cpusets, hwloc_topology_t topology) {
	prefetcher->cpusets = cpusets;
	prefetcher->topology = topology;

}

int hexe_start()
{
    if(!prefetcher)
        return -1;
    if(! prefetcher->cache_pool)
        return -1;
    if (prefetcher->cpusets)
        prefetcher->thread =  create_new_thread_with_topo(prefetcher->ncaches, prefetcher->topology, prefetcher->cpusets, prefetcher->prefetch_threads);
    else
        prefetcher->thread =  create_new_thread(prefetcher->ncaches, prefetcher->prefetch_threads);
}


int hexe_alloc_pool(size_t size, int n){

    size_t total_size = (size * n);
    int i;
/*    if (cpusets)
        thread =  create_new_thread_with_topo(n, id, topology, cpusets, prefetch_threads);
    else
        thread =  create_new_thread(n, id);
*/
    prefetcher->handle = (prefetch_handle_t*) memkind_malloc(MEMKIND_DEFAULT, sizeof(prefetch_handle_t) * n);;
    memset(prefetcher->handle, 0x0, sizeof(prefetch_handle_t)*n);

    if(total_size % _SC_PAGE_SIZE != 0)
        total_size =  (total_size + _SC_PAGE_SIZE) & ~(0xfff);
    prefetcher->ncaches = n;
    prefetcher->cache_size = total_size;
    prefetcher->cache_pool = (void**) memkind_malloc(MEMKIND_DEFAULT, sizeof(void*) * n);

    if(!prefetcher->cache_pool)
        return -1;
     prefetcher->cache =  memkind_malloc(MEMKIND_HBW,  total_size);

    if(!prefetcher->cache){
        memkind_free(MEMKIND_DEFAULT, prefetcher->cache_pool);
        return -1;
    }
    for(i = 0; i<n; i++){
        prefetcher->cache_pool[i] = &((char*)(prefetcher->cache))[i* size];
    }

    return 0;

}


int hexe_start_fetch_continous(void *start_addr, size_t size, int idx)
{
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_prefetch_continous(start_addr, prefetcher->cache_pool[idx], size ,prefetcher->thread);

    return 0;
}


int hexe_start_write_back_continous(int cache_idx, void *start_addr, size_t size){


    hexe_sync_fetch(cache_idx);

    prefetcher->handle[cache_idx] = start_prefetch_continous(prefetcher->cache_pool[cache_idx], start_addr, size, prefetcher->thread);

    return 0;
}

int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx){ 

    prefetcher->handle[idx] = start_prefetch_noncontinous(start_addr, prefetcher->cache_pool[idx], offset_list, element_size, elements, prefetcher->thread);

    return 0;
}

void *hexe_get_cache(int idx) {

    return   hexe_sync_fetch(idx);

}
void* hexe_sync_fetch(int idx){

    if(prefetcher->handle[idx]!=0)
        prefetch_wait(prefetcher->handle[idx]); 
    prefetcher->handle[idx] = 0;


  return prefetcher->cache_pool[idx];
}

void* hexe_wait_index(int idx, int index){

    prefetch_wait_index(prefetcher->handle[idx], index); 
    return prefetcher->cache_pool[idx];
    return 0;
}
