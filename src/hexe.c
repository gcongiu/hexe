#include "prefetch.h"
#include "hexe.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <numaif.h>
#include "list.h"
#include <sys/mman.h>
#include "hexe_intern.h"

static unsigned long tacc_rdtscp(int *chip, int *core)
{
    unsigned long a,d,c;
    __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
    *chip = (c & 0xFFF000)>>12;
    *core = c & 0xFFF;
    return ((unsigned long)a) | (((unsigned long)d) << 32);;
}
static void detect_knl_mode(struct hexe *h)
{

    hwloc_obj_t root;
    hwloc_obj_t obj;
    const char *cluster_mode;
    const char *memory_mode;
    int numa_nodes, i;
    int cores;
    hwloc_topology_init(&h->topology);
    hwloc_topology_load(h->topology);
    root = hwloc_get_root_obj(h->topology);
    h->mcdram_memory = 0 ;
    memory_mode = hwloc_obj_get_info_by_name(root, "MemoryMode");

    if(memory_mode) {
        if(strncmp(memory_mode, "Flat" , sizeof("Flat"))== 0) {
            printf("Flat\n");
            h->memory_mode = FLAT;
        }

        else if(strncmp(memory_mode, "Cache" , sizeof("Cache")) == 0) {
            printf("Cache mode \n"); 
            h->memory_mode = CACHE;
        }
        else if(strncmp(memory_mode, "Hybrid25" , sizeof("Hybrid25")) == 0) {
            printf("Hybrid25 mode \n"); 
            h->memory_mode = HYBRID25;
        }
        else if(strncmp(memory_mode, "Hybrid50" , sizeof("Hybrid50")) == 0) {
            printf("Hybrid50 mode \n"); 
            h->memory_mode = HYBRID25;
        }
    }
    else{
        h->memory_mode = CACHE;  /*we assume not to have different types */;
        h->mcdram_memory = 0;
        }

    cluster_mode = hwloc_obj_get_info_by_name(root, "ClusterMode");
    if(cluster_mode){
        if(strncmp(cluster_mode, "Quadrant" , sizeof("Quadrant"))== 0) {
            printf("Quadrant\n");
            h->cluster_mode = QUADRANT;
        }
         if(strncmp(cluster_mode, "Hemisphere" , sizeof("Hemisphere"))== 0) {
            printf("Hemisphere\n");
            h->cluster_mode =  HEMISPHERE;
        }

        else if(strncmp(cluster_mode, "SNC2" , sizeof("SNC2"))== 0) {
            printf("SNC2\n");
            h->cluster_mode = SNC2;
        }
        else if(strncmp(cluster_mode, "SNC4" , sizeof("SNC4"))== 0) {
            printf("SNC4\n");
            h->cluster_mode = SNC4;
        }
        else if(strncmp(cluster_mode, "All2All" , sizeof("All2All"))== 0) {
            printf("All2All\n");
            h->cluster_mode = SNC2;
        }

    }
    else
        h->cluster_mode = -1;

    numa_nodes = hwloc_get_nbobjs_by_type(h->topology, HWLOC_OBJ_NUMANODE);
    cores =  hwloc_get_nbobjs_by_type(h->topology, HWLOC_OBJ_CORE);

    if (h->memory_mode & (FLAT | HYBRID25 | HYBRID50)) {

        h->prefetch_threads = cores/4;
        h->compute_threads = cores - cores/4;

            /*some checkse */
        if(h->cluster_mode & (QUADRANT | HEMISPHERE | ALL2ALL)) {
            assert(numa_nodes == 2);
        }
        else if(h->cluster_mode & (SNC2)) {
            assert(numa_nodes == 4);
        }
        else if(h->cluster_mode & (SNC4)) {
            assert(numa_nodes == 8);
        }
        h->ddr_sets = malloc(sizeof(hwloc_bitmap_t) * numa_nodes/2);
        h->mcdram_sets = malloc(sizeof(hwloc_bitmap_t) * numa_nodes/2);
        h->all_ddr = hwloc_bitmap_alloc();
        h->all_mcdram = hwloc_bitmap_alloc();
        h->mcdram_nodes = numa_nodes/2;
        h->ddr_nodes = numa_nodes/2;
        for (i = 0; i<numa_nodes; i++) {
            obj=hwloc_get_obj_by_type(h->topology, HWLOC_OBJ_NUMANODE, i);

            if(!hwloc_bitmap_iszero(obj->cpuset)) {
                if(i/2==2)
                h->ddr_sets[i/2]=obj->nodeset;
                hwloc_bitmap_or(h->all_ddr, obj->nodeset, h->all_ddr);
            }
            else{
                h->mcdram_sets[i/2]=obj->nodeset; 
                hwloc_bitmap_or(h->all_mcdram, obj->nodeset, h->all_mcdram);
                h->mcdram_memory += obj->memory.total_memory;
            }
        }

   printf(" have %ld \n", hwloc_bitmap_to_ulong (h->all_mcdram) );

    }
    else { /*cache mode or not known*/
        if(h->cluster_mode & (QUADRANT | HEMISPHERE | ALL2ALL)) {
            assert(numa_nodes == 1 || numa_nodes == 0);
        }
        else if(h->cluster_mode & (SNC2)) {
            assert(numa_nodes == 2);
        }
        else if(h->cluster_mode & (SNC4)) {
            assert(numa_nodes == 4);
        }
        if(numa_nodes) {
            h->ddr_sets = malloc(sizeof(hwloc_bitmap_t) * numa_nodes);
            h->mcdram_sets = NULL;
            h->all_ddr = hwloc_bitmap_alloc();
            h->compute_threads = cores ;
            for (i = 0; i<numa_nodes; i++) {
                obj=hwloc_get_obj_by_type(h->topology, HWLOC_OBJ_NUMANODE, i);
                h->ddr_sets[i]=obj->nodeset;
                hwloc_bitmap_or(h->all_ddr, obj->nodeset, h->all_ddr);
            }
        }

    }

    printf("Have a total of %ld Gbyte hbw memory\n", h->mcdram_memory/(1024*1024*1024));
    h->total_mcdram = h->mcdram_memory;
    if(h->mcdram_memory > 0)
        h->mcdram_memory -= (512*1024*1024);
}
static int _hexe_find_sibling_knl(int core_id) {

    int     n;
    char    core_siblings_path[1024];
    char    core_siblings[1024];
    FILE   *f = NULL;
    char   *map, *src;

    unsigned long lmap;
    sprintf(core_siblings_path, "/sys/devices/system/cpu/cpu%d/cache/index2/shared_cpu_list", core_id);
    f = fopen(core_siblings_path, "r");
    if (f){
         n = fread (core_siblings, 1, 255, f);
      if (n > 0) {
        lmap = strtoull(core_siblings, &map, 10);
        if(lmap != core_id)
            return lmap; 
        if (map[0] == '-'){
            return core_id+1;
             }
         else if (map[0] == ',')
            lmap = strtoull(map+1, &map, 10);
            return lmap;

         }
      else
         return -1;
      }
  return 0;

}
static inline long hexe_distribute_compute_only()
{
    int cores;
    int i, current_core = 0;
    int add = 1;
    unsigned long mask;
    if(prefetcher->compute_threads <= 32)
        add = 2;

    cores = hwloc_get_nbobjs_by_type(prefetcher->topology, HWLOC_OBJ_CORE);
    prefetcher->prefetch_cpusets = NULL;
    prefetcher->compute_cpusets = malloc(sizeof(hwloc_cpuset_t) * prefetcher->compute_threads);
    if(prefetcher->compute_threads > cores) {
        cores =  hwloc_get_nbobjs_by_type(prefetcher->topology, HWLOC_OBJ_PU);
    }

    for (i = 0; i<prefetcher->compute_threads;i++) {
          prefetcher->compute_cpusets[i] =hwloc_bitmap_alloc();
          hwloc_bitmap_set(prefetcher->compute_cpusets[i],current_core); 
        current_core += 16;
        if(current_core >= cores)
            current_core = current_core%cores+add;
    }

}

static int hexe_distribute_threads_knl()
{
    int total_threads = prefetcher->prefetch_threads + prefetcher->compute_threads;
    int root, cores, pu;
    int i;
    int current_core;
    int add = 1;
    if(total_threads == 0)
        return -1;
   if(prefetcher->prefetch_threads == 0)
        return hexe_distribute_compute_only();
     cores = hwloc_get_nbobjs_by_type(prefetcher->topology, HWLOC_OBJ_CORE);
     pu = hwloc_get_nbobjs_by_type(prefetcher->topology, HWLOC_OBJ_PU);

     if(pu < total_threads) {
        printf("Warining: oversubscribing, no thread binding\n");
        prefetcher->prefetch_cpusets = NULL;
        prefetcher->compute_cpusets = NULL;
     return 0;
    }
    if(total_threads <= 32)
        add = 2;


    /*we create two node-sets, one for prefetch-threads and one for the compute-threads
     * we start with the prefetch threads, the compute threads are distributed among thre remaining 
     * cores */

    current_core = 0;
    prefetcher->prefetch_cpusets = malloc(sizeof(hwloc_cpuset_t) * prefetcher->prefetch_threads);
    prefetcher->compute_cpusets = malloc(sizeof(hwloc_cpuset_t) * prefetcher->compute_threads);
    hwloc_obj_t tmp;

        for (i = 0; i<prefetcher->compute_threads;i++) {
            tmp = hwloc_get_obj_by_type(prefetcher->topology, HWLOC_OBJ_CORE, current_core);
            prefetcher->compute_cpusets[i] = hwloc_bitmap_dup(tmp->cpuset);
            current_core += 16;
            if(current_core >= cores)
                current_core = current_core%cores+add;

        }

    for (i=0; i<prefetcher->prefetch_threads;i++) {
        tmp = hwloc_get_obj_by_type(prefetcher->topology, HWLOC_OBJ_CORE, current_core);
        prefetcher->prefetch_cpusets[i] = hwloc_bitmap_dup(tmp->cpuset);
        current_core += 16;
        if(current_core >= cores)
            current_core = current_core%cores+add;

    }

}
int hexe_init()
{
    int threads;
    char *sthreads;
    if(hexe_is_init())
        return 0;
    prefetcher =(struct hexe*)  malloc(sizeof(struct hexe));
    memset(prefetcher, 0, sizeof(struct hexe));
    detect_knl_mode(prefetcher);
    if(prefetcher->memory_mode != CACHE){
        sthreads =  getenv("HEXE_PREFETCH_THREADS");
        if(sthreads)
             prefetcher->prefetch_threads = atoi(sthreads);
    }
    sthreads =  getenv("HEXE_COMPUTE_THREADS");
    if(sthreads)
       prefetcher->compute_threads = atoi(sthreads);

    hexe_distribute_threads_knl();
    prefetcher->is_init = 1;
    return 0;
}

int hexe_is_init() 
{
    if(!prefetcher)
        return 0;
    else
        return prefetcher->is_init;
}
int hexe_set_prefetch_threads(int threads){


    if(prefetcher->memory_mode == CACHE)
        return 0;
    prefetcher->prefetch_threads = threads;
    hexe_distribute_threads_knl();
}
int hexe_set_compute_threads(int threads){

    prefetcher->compute_threads = threads;
    hexe_distribute_threads_knl();
}

int hexe_finalize()
{
    if(prefetcher->memory_mode == CACHE){
        if(prefetcher->cache_pool)
            free(prefetcher->cache_pool);
    }
    else {

	if(prefetcher->is_started)  
      finish_thread( );
        if(prefetcher->cache) 
            hexe_free_pool();

    }

   hwloc_topology_destroy(prefetcher->topology);
    free(prefetcher);
  
}

int hexe_start()
{
    if(prefetcher->compute_threads) {
         omp_set_num_threads(prefetcher->compute_threads);
         #pragma omp parallel
        {
            int id = omp_get_thread_num();
            hwloc_set_cpubind    (prefetcher->topology, prefetcher->compute_cpusets[id], HWLOC_CPUBIND_THREAD);
        }
    }
    if(prefetcher->memory_mode == CACHE)
        return 0;
    if(!prefetcher)
        return -1;
    if(! prefetcher->cache_pool)
        return -1;
    if (prefetcher->prefetch_cpusets)
       create_new_thread_with_topo(prefetcher->ncaches, prefetcher->topology, prefetcher->prefetch_cpusets, prefetcher->prefetch_threads);
    else
        create_new_thread(prefetcher->ncaches, prefetcher->prefetch_threads);
    prefetcher->is_started=1;
}

void* hexe_request_hbw(void* map_addr, size_t size, int priority) {

  if(map_addr == NULL)
      map_addr= mmap(prefetcher->cache, size, PROT_READ | PROT_WRITE, MAP_PRIVATE |MAP_ANON, -1, 0);

      insertList(map_addr, size, priority);
    return map_addr;

}
void* hexe_request_hbw2(uint64_t size, int priority) {

    void *map_addr;

      map_addr= mmap(prefetcher->cache, size, PROT_READ | PROT_WRITE, MAP_PRIVATE |MAP_ANON, -1, 0);

      insertList(map_addr, (unsigned long long) size , priority);
    return map_addr;

}

int hexe_bind_requested_memory(int redistribute){

    unsigned long node_mask, ddr_mask;
    if(prefetcher->memory_mode == CACHE)
        return 0;
    if(redistribute) {
        prefetcher->mcdram_memory= prefetcher->total_mcdram;
        prefetcher->mcdram_memory-=(prefetcher->cache_size +
                            1024*1024*512);
        reset_list();

    }


   sort_memory_list();

   node_mask = hwloc_bitmap_to_ulong(prefetcher->all_mcdram);
   ddr_mask = hwloc_bitmap_to_ulong(prefetcher->all_ddr);

   get_best_layout(node_mask, ddr_mask);
    print_list();
   return 0;

}

int hexe_change_priority(void* ptr, int priority) {

    struct node* entry = find(ptr);
    int old;
    if(prefetcher->memory_mode == CACHE)
        return 0;

    if(! entry)
        return -1;
    old = entry->priority;
    entry->priority = priority;
   return 0;
}
int hexe_is_in_hbw (void *ptr) {


    if(prefetcher->memory_mode == CACHE)
        return 0;
    struct node* entry = find(ptr);
    if(! entry)
        return -1;
    return entry->location == 1 ? 1: 0;

}

void hexe_free_memory(void *ptr) {

    struct node* entry = delete_from_list(ptr);
    if(entry == NULL)
            return;
    munmap(entry->addr, entry->size);
    if(entry->location == 1) {
        prefetcher->mcdram_memory+= entry->size;

    }

}

static inline int malloc_fake_pool(int n) {

    int i;
    prefetcher->ncaches = n;
    prefetcher->cache_pool = (void**)malloc( sizeof(void*) * n);
     if(!prefetcher->cache_pool)
        return -1;
    prefetcher->cache = NULL;
    for(i = 0; i<n; i++){
        prefetcher->cache_pool[i]  = NULL;
    }
}


int hexe_alloc_pool(size_t size, int n){

    size_t total_size = (size * n);
    int i;
    unsigned long node_mask;
    int nodes = prefetcher->mcdram_nodes + prefetcher->ddr_nodes;

    int mode = (prefetcher->mcdram_nodes > 1) ? MPOL_INTERLEAVE: MPOL_BIND;
    prefetcher->handle = (prefetch_handle_t*)  malloc(sizeof(prefetch_handle_t) * n);;
    memset(prefetcher->handle, 0x0, sizeof(prefetch_handle_t)*n); 

    if(prefetcher->memory_mode == CACHE)
        return malloc_fake_pool(n);

   if(total_size % _SC_PAGE_SIZE != 0)
        total_size =  (total_size + _SC_PAGE_SIZE) & ~(0xfff);
    prefetcher->ncaches = n;
    prefetcher->cache_size = total_size;
    prefetcher->cache_pool = (void**)malloc( sizeof(void*) * n * 2);
 
    node_mask = hwloc_bitmap_to_ulong(prefetcher->all_mcdram);
    if(!prefetcher->cache_pool)
        return -1;

    prefetcher->cache = mmap(prefetcher->cache, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE |MAP_ANON, -1, 0);

   if(!prefetcher->cache){
        free(prefetcher->cache_pool);
        return -1;
    }
   mbind(prefetcher->cache, total_size, mode,
          &node_mask, NUMA_NUM_NODES, MPOL_MF_MOVE);

    madvise(prefetcher->cache, total_size, MADV_HUGEPAGE);
    prefetcher->mcdram_memory -= total_size;
    for(i = 0; i<n; i++){
        prefetcher->cache_pool[i*2] = &((char*)(prefetcher->cache))[i* size];
    }
    prefetcher->counter = malloc(sizeof(int)*prefetcher->ncaches);
   return 0;

}

void hexe_free_pool()
{
     if(prefetcher->memory_mode == CACHE)
        return;
    if(prefetcher->cache) {
        munmap(prefetcher->cache, prefetcher->cache_size);
        prefetcher->cache = NULL;
    }
    if(prefetcher->cache_pool)
        free(prefetcher->cache_pool);
    prefetcher->cache_pool = NULL;
    if(prefetcher->handle)
        free(prefetcher->handle);
    prefetcher->handle = NULL;

}

int hexe_start_fetch_continous(void *start_addr, size_t size, int idx)
{
    if(prefetcher->memory_mode == CACHE){
        prefetcher->cache_pool[idx]= start_addr;
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_prefetch_continous(start_addr, prefetcher->cache_pool[2*idx], size);

    return 0;
}

int hexe_start_fetch_continous_taged(void *start_addr, size_t size, int idx, int tag)
{
    if(prefetcher->memory_mode == CACHE){
        prefetcher->cache_pool[idx]= start_addr;
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_prefetch_continous(start_addr, prefetcher->cache_pool[2*idx], size);
    prefetcher->counter [idx]= tag;
    return 0;
}
int hexe_start_fetch_strided(void *start_addr, size_t count, size_t block_len, size_t stride,  int idx)
{
    if(prefetcher->memory_mode == CACHE){
        printf("not supported\n");
        prefetcher->cache_pool[idx]= start_addr;
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_prefetch_strided(start_addr, prefetcher->cache_pool[2*idx], count, block_len, stride);

    return 0;
}



int hexe_start_write_back_continous(size_t size, int cache_idx){

    if(prefetcher->memory_mode == CACHE)
        return 0; 

    hexe_sync_fetch(cache_idx);
    prefetcher->handle[cache_idx] = start_prefetch_continous(prefetcher->cache_pool[2*cache_idx], prefetcher->cache_pool[2*cache_idx+1], size);

    return 0;
}

int hexe_start_write_back_strided(size_t count, size_t block_len, size_t stride,  int idx)
{
    if(prefetcher->memory_mode == CACHE){
        printf("not supported\n");
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_write_back_strided(prefetcher->cache_pool[2*idx], prefetcher->cache_pool[2*idx+1],  count, block_len, stride);

    return 0;
}

int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx){ 

    if(prefetcher->memory_mode == CACHE)
        return 0; 

    prefetcher->handle[idx] = start_prefetch_noncontinous(start_addr, prefetcher->cache_pool[idx], offset_list, element_size, elements);

    return 0;
}

void *hexe_get_cache_for_write_back(int idx, void *addr) {

    if(prefetcher->memory_mode == CACHE)
    {
        prefetcher->cache_pool[idx]= addr;
        return addr;
    }
    prefetcher->cache_pool[idx*2+1]= addr;
    return   hexe_sync_fetch(idx);

}
void* hexe_sync_fetch(int idx){

   if(prefetcher->handle[idx]!=0)
       prefetch_wait(prefetcher->handle[idx]);

    prefetcher->handle[idx] = 0;

    return prefetcher->cache_pool[2*idx];
}
static inline void wait_tag(int idx, int tag) {
 volatile int tmp = prefetcher->counter[idx];
	while(tmp!= tag) {

		tmp = prefetcher->counter[idx];

	}
	
}
void* hexe_sync_fetch_taged(int idx, int tag){

    if(prefetcher->counter[idx] != tag)
        wait_tag(idx, tag);
   if(prefetcher->handle[idx]!=0)
       prefetch_wait(prefetcher->handle[idx]);

    prefetcher->handle[idx] = 0;

    return prefetcher->cache_pool[2*idx];
}

void* hexe_wait_index(int idx, int index){

    prefetch_wait_index(prefetcher->handle[idx], index); 
    return prefetcher->cache_pool[idx*2];

}
