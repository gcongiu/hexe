#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <numaif.h>
#include <numa.h>
#include <sys/mman.h>
#include "hexe_malloc.h"
#include <string.h>
#include <hwloc/linux-libnuma.h>
#ifdef WITH_PREFETCHER
#include <omp.h>
#include "prefetch.h"
#endif

struct node *head = NULL;
struct node* pinned = NULL;
struct node* fixed = NULL;
struct hexe_malloc *mem_manager;
struct hexe_prefetcher *prefetcher;




static void detect_knl_mode()
{

    hwloc_obj_t root;
    hwloc_obj_t obj;
    const char *cluster_mode;
    const char *memory_mode;
    int numa_nodes, i;
    int cores;
    hwloc_topology_init(&mem_manager->topology);
    hwloc_topology_load(mem_manager->topology);
    root = hwloc_get_root_obj(mem_manager->topology);
    mem_manager->mcdram_avail = 0 ;
    memory_mode = hwloc_obj_get_info_by_name(root, "MemoryMode");

    if(memory_mode) {
        if(strncmp(memory_mode, "Flat" , sizeof("Flat"))== 0) {

            mem_manager->memory_mode = FLAT;
        }

        else if(strncmp(memory_mode, "Cache" , sizeof("Cache")) == 0) {

            mem_manager->memory_mode = CACHE;
        }
        else if(strncmp(memory_mode, "Hybrid25" , sizeof("Hybrid25")) == 0) {

            mem_manager->memory_mode = HYBRID25;
        }
        else if(strncmp(memory_mode, "Hybrid50" , sizeof("Hybrid50")) == 0) {

            mem_manager->memory_mode = HYBRID25;
        }
    }
    else{

        mem_manager->memory_mode = CACHE;  /*we assume not to have different types */;
        mem_manager->mcdram_avail = 0;
        }

    cluster_mode = hwloc_obj_get_info_by_name(root, "ClusterMode");
    if(cluster_mode){
       if(strncmp(cluster_mode, "Quadrant" , sizeof("Quadrant"))== 0) {

            mem_manager->cluster_mode = QUADRANT;
        }
         if(strncmp(cluster_mode, "Hemisphere" , sizeof("Hemisphere"))== 0) {

            mem_manager->cluster_mode =  HEMISPHERE;
        }

        else if(strncmp(cluster_mode, "SNC2" , sizeof("SNC2"))== 0) {

            mem_manager->cluster_mode = SNC2;
        }
        else if(strncmp(cluster_mode, "SNC4" , sizeof("SNC4"))== 0) {

            mem_manager->cluster_mode = SNC4;
        }
        else if(strncmp(cluster_mode, "All2All" , sizeof("All2All"))== 0) {

            mem_manager->cluster_mode = SNC2;
        }

    }
    else
        mem_manager->cluster_mode = -1;

    numa_nodes = hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_NUMANODE);
    cores =  hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_CORE);

    if (mem_manager->memory_mode & (FLAT | HYBRID25 | HYBRID50)) {

                   /*some checkse */
        if(mem_manager->cluster_mode & (QUADRANT | HEMISPHERE | ALL2ALL)) {
            assert(numa_nodes == 2);
        }
        else if(mem_manager->cluster_mode & (SNC2)) {
            assert(numa_nodes == 4);
        }
        else if(mem_manager->cluster_mode & (SNC4)) {
            assert(numa_nodes == 8);
        }
        mem_manager->ddr_sets = malloc(sizeof(unsigned long) * numa_nodes/2);
        mem_manager->mcdram_sets = malloc(sizeof(unsigned long) * numa_nodes/2);
        mem_manager->all_ddr = hwloc_bitmap_alloc();
        mem_manager->all_mcdram = hwloc_bitmap_alloc();
        mem_manager->mcdram_nodes = numa_nodes/2;
        mem_manager->ddr_nodes = numa_nodes/2;
        for (i = 0; i<numa_nodes; i++) {
            obj=hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_NUMANODE, i);

            if(!hwloc_bitmap_iszero(obj->cpuset)) {
                mem_manager->ddr_sets[i/2]=hwloc_bitmap_to_ulong(obj->nodeset);
                hwloc_bitmap_or(mem_manager->all_ddr, obj->nodeset, mem_manager->all_ddr);

            }
            else{
                mem_manager->mcdram_sets[i/2]=hwloc_bitmap_to_ulong(obj->nodeset);
                hwloc_bitmap_or(mem_manager->all_mcdram, obj->nodeset, mem_manager->all_mcdram);
                size_t tmp;
                mem_manager->total_mcdram += numa_node_size(hwloc_bitmap_first(obj->nodeset), &tmp);
                mem_manager->mcdram_avail += tmp;
            }
        }

      mem_manager->mcdram_bitmask =  hwloc_nodeset_to_linux_libnuma_bitmask(mem_manager->topology, mem_manager->all_mcdram);
    }
    else { /*cache mode or not known*/
        if(mem_manager->cluster_mode & (QUADRANT | HEMISPHERE | ALL2ALL)) {
            assert(numa_nodes == 1 || numa_nodes == 0);
        }
        else if(mem_manager->cluster_mode & (SNC2)) {
            assert(numa_nodes == 2);
        }
        else if(mem_manager->cluster_mode & (SNC4)) {
            assert(numa_nodes == 4);
        }
        if(numa_nodes) {
            mem_manager->ddr_sets = malloc(sizeof(hwloc_bitmap_t) * numa_nodes);
            mem_manager->mcdram_sets = NULL;
            mem_manager->all_ddr = hwloc_bitmap_alloc();
            mem_manager->ddr_nodes = numa_nodes;
            for (i = 0; i<numa_nodes; i++) {
                obj=hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_NUMANODE, i);
                mem_manager->ddr_sets[i]=hwloc_bitmap_to_ulong(obj->nodeset);
                hwloc_bitmap_or(mem_manager->all_ddr, obj->nodeset, mem_manager->all_ddr);
            }
        }
        printf("I have %d numa_nodes\n", numa_nodes);
    }


      mem_manager->ddr_bitmask =  hwloc_nodeset_to_linux_libnuma_bitmask(mem_manager->topology, mem_manager->all_ddr);
    //printf("have total %3.2fGB memory, %3.2fGB are avilavle\n", (double)prefetcher->total_mcdram/(1024*1024.0*1024.0),(double) mem_manager->mcdram_avail/(1024.0*1024.0 * 1024));


}

size_t hexe_avail_mcdram() {

    return mem_manager->mcdram_avail;
}
int hexe_init()
{
    int threads;
    char *sthreads;
    if(hexe_is_init())
        return 0;

    mem_manager = (struct hexe_malloc*) malloc (sizeof(struct hexe_malloc));
    memset(mem_manager, 0, sizeof(struct hexe_malloc));
    detect_knl_mode();
#ifdef WITH_PREFETCHER

    prefetcher =(struct hexe_prefetcher*)  malloc(sizeof(struct hexe_prefetcher));
    prefetcher->prefetch_threads = 0;
    prefetcher->compute_threads = 64;

    if(mem_manager->memory_mode != CACHE){

        sthreads =  getenv("HEXE_PREFETCH_THREADS");
        if(sthreads)
             prefetcher->prefetch_threads = atoi(sthreads);
    }
    sthreads =  getenv("HEXE_COMPUTE_THREADS");
    if(sthreads)
       prefetcher->compute_threads = atoi(sthreads);
#endif
    mem_manager->is_init = 1;
    return 0;
}

int hexe_is_init() 
{
    if(!mem_manager)
        return 0;
    else
        return mem_manager->is_init;
}

int hexe_mpi_bind(int rank, int size) {

    hwloc_obj_t tmp;
    int current_core = 0; 
    int add  = size > 32? 1:2;
    int i;

    for (i = 0; i<rank; i++) {
        current_core += 16;
        if(current_core >= 64)
            current_core = current_core%64+add;
    }

        tmp = hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_CORE, (current_core));

    hwloc_set_cpubind(mem_manager->topology,tmp->cpuset, HWLOC_CPUBIND_THREAD);

    numa_set_localalloc();
    return 0;
}

void* hexe_request_hbw(void* map_addr, size_t size, int priority) {

  if(map_addr == NULL)
      map_addr= mmap(map_addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE |MAP_ANON, -1, 0);
   if(mem_manager->memory_mode == CACHE && (mem_manager->cluster_mode & (SNC2|SNC4)))
        bind_strided( map_addr, size, 1);

      insertList(map_addr, size, priority);
    return map_addr;
}


int hexe_bind_requested_memory(int redistribute){

    unsigned long node_mask, ddr_mask;
    if(mem_manager->memory_mode == CACHE)
        return 0;
    if(redistribute) {
        mem_manager->mcdram_avail= mem_manager->total_mcdram;
        mem_manager->mcdram_avail-=(prefetcher->cache_size +
                            1024*1024*512);
        reset_list();

    }

   sort_memory_list();

   get_best_layout();
    print_list();
   return 0;

}

int hexe_change_priority(void* ptr, int priority) {

    struct node* entry = find(ptr, 0);
    int old;
    if(mem_manager->memory_mode == CACHE)
        return 0;

    if(! entry)
        return -1;
    old = entry->priority;
    entry->priority = priority;
   return 0;
}




void hexe_free_memory(void *ptr) {

    struct node* entry = delete_from_list(ptr);
    if(entry == NULL) {

            return;
    }
    switch (entry->location){
        case MCDRAM_KNP:
        case DDR_KNP:
        case UNDEFINED:
            munmap(entry->addr, entry->size);
            break;
        case MCDRAM_FIXED:
        case DDR_FIXED:
            numa_free(entry->addr, entry->size);
            break;
    }

    if(entry->location == MCDRAM_KNP) {
        mem_manager->mcdram_avail+= entry->size;

    }

}

#ifdef WITH_PREFETCHER
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

static inline void hexe_free_pool()
{

    printf("free pool\n");
     if(mem_manager->memory_mode == CACHE)
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


static inline long hexe_distribute_compute_only()
{
    int cores;
    int i, current_core = 0;
    int add = 1;
    unsigned long mask;
    if(prefetcher->compute_threads <= 32)
        add = 2;

    cores = hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_CORE);
    prefetcher->prefetch_cpusets = NULL;
    prefetcher->compute_cpusets = malloc(sizeof(hwloc_cpuset_t) * prefetcher->compute_threads);
    if(prefetcher->compute_threads > cores) {
        cores =  hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_PU);
    }

    for (i = 0; i<prefetcher->compute_threads;i++) {
          prefetcher->compute_cpusets[i] =hwloc_bitmap_alloc();
          hwloc_bitmap_set(prefetcher->compute_cpusets[i],current_core); 
        current_core += cores/4;
        if(current_core >= cores)
            current_core = current_core%cores+add;
    }

}

int distribute_threads_snc() {

    int num_nodes = mem_manager->ddr_nodes;
    int c_thread_per_node = prefetcher->compute_threads / num_nodes;
    int f_thread_per_node = prefetcher->prefetch_threads / num_nodes;
    int cores;
    int add;
    int total_threads = prefetcher->prefetch_threads + prefetcher->compute_threads;
    int current_core;
    hwloc_obj_t tmp;
    cores = hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_CORE);
    add = total_threads > 32 ? 1: 2;
    prefetcher->prefetch_cpusets = malloc(sizeof(hwloc_cpuset_t) * prefetcher->prefetch_threads);
    prefetcher->compute_cpusets = malloc(sizeof(hwloc_cpuset_t) * prefetcher->compute_threads);
    int k = 0, i, j, l = 0;
    
    for(i = 0; i < num_nodes; i++) {
        current_core = (cores/num_nodes) * i;
        for(j = 0; j< c_thread_per_node; j++){

            tmp = hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_CORE, current_core);
            prefetcher->compute_cpusets[k] = hwloc_bitmap_dup(tmp->cpuset);
            current_core +=add;
            k++;
        }
        for(j = 0; j< f_thread_per_node; j++){


            tmp = hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_CORE, current_core);
            prefetcher->compute_cpusets[l] = hwloc_bitmap_dup(tmp->cpuset);
            current_core +=add;
            l++;
        }

    }

}

int hexe_distribute_threads_knl()
{
    int total_threads = prefetcher->prefetch_threads + prefetcher->compute_threads;
    int root, cores, pu;
    int i;
    int current_core;
    int add = 1;
    if(total_threads == 0)
        return -1;

   if(mem_manager->cluster_mode & (SNC2 | SNC4))
        distribute_threads_snc();
   if(prefetcher->prefetch_threads == 0)
        return hexe_distribute_compute_only();
     cores = hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_CORE);
     pu = hwloc_get_nbobjs_by_type(mem_manager->topology, HWLOC_OBJ_PU);

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
            tmp = hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_CORE, current_core);
            prefetcher->compute_cpusets[i] = hwloc_bitmap_dup(tmp->cpuset);
            current_core +=cores/4;
            if(current_core >= cores)
                current_core = current_core%cores+add;

        }

    for (i=0; i<prefetcher->prefetch_threads;i++) {
        tmp = hwloc_get_obj_by_type(mem_manager->topology, HWLOC_OBJ_CORE, current_core);
        prefetcher->prefetch_cpusets[i] = hwloc_bitmap_dup(tmp->cpuset);
        current_core += cores/4;
        if(current_core >= cores)
            current_core = current_core%cores+add;

    }

}
int hexe_set_prefetch_threads(int threads){


    if(mem_manager->memory_mode == CACHE)
        return 0;
    prefetcher->prefetch_threads = threads;
 //   hexe_distribute_threads_knl();
}
int hexe_set_compute_threads(int threads){

    prefetcher->compute_threads = threads;
   // hexe_distribute_threads_knl();
}

int hexe_finalize()
{
    if(mem_manager->memory_mode == CACHE){
        if(prefetcher->cache_pool)
            free(prefetcher->cache_pool);
    }
    else {
    printf("here %p \n", prefetcher->cache);
	if(prefetcher->is_started)  
      finish_thread( );
   if(prefetcher->cache) 
            hexe_free_pool();

    }

   hwloc_topology_destroy(mem_manager->topology);
    free(prefetcher);
  
}

int hexe_start()
{
      hexe_distribute_threads_knl();
    if(prefetcher->compute_threads) {
         omp_set_num_threads(prefetcher->compute_threads);
        #pragma omp parallel
        {
            int id = omp_get_thread_num();
            hwloc_set_cpubind    (mem_manager->topology, prefetcher->compute_cpusets[id], HWLOC_CPUBIND_THREAD);
        }
    }
 if(mem_manager->memory_mode == CACHE)
        return 0;
   

   if(!prefetcher)
        return -1;
    if(! prefetcher->cache_pool)
        return -1;
    if (prefetcher->prefetch_cpusets)
       create_new_thread_with_topo(prefetcher->ncaches, mem_manager->topology, prefetcher->prefetch_cpusets, prefetcher->prefetch_threads);
    else
        create_new_thread(prefetcher->ncaches, prefetcher->prefetch_threads);
    prefetcher->is_started=1;
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
    size_t real_size = size; 
   int i;
    unsigned long node_mask;

    int mode = (mem_manager->mcdram_nodes > 1) ? MPOL_INTERLEAVE: MPOL_BIND;
    prefetcher->handle = (prefetch_handle_t*)  malloc(sizeof(prefetch_handle_t) * n);;
    memset(prefetcher->handle, 0x0, sizeof(prefetch_handle_t)*n); 

    if(mem_manager->memory_mode == CACHE)
        return malloc_fake_pool(n);

   if(real_size & 0xfff )
        real_size =  (real_size + 4096) & ~((unsigned long) 0xfff);
    total_size = real_size *n ;
    prefetcher->ncaches = n;
    prefetcher->cache_size = total_size;
    prefetcher->cache_pool = (void**)malloc( sizeof(void*) * n * 2);
    printf("the real size is l%ld %ld \n", real_size, size); 
    node_mask = hwloc_bitmap_to_ulong(mem_manager->all_mcdram);
    if(!prefetcher->cache_pool)
        return -1;

    prefetcher->cache = mmap(prefetcher->cache, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE |MAP_ANON, -1, 0);

   if(!prefetcher->cache){
        free(prefetcher->cache_pool);
        return -1;
    }
   madvise(prefetcher->cache, total_size, MADV_HUGEPAGE);
    mem_manager->mcdram_avail -= total_size;
    for(i = 0; i<n; i++){
        prefetcher->cache_pool[i*2] = &((char*)(prefetcher->cache))[i* real_size];
    }
//  if(mem_manager->mcdram_node == 1) {
        int err = mbind(prefetcher->cache, total_size, mode,
                &node_mask, NUMA_NUM_NODES, MPOL_MF_MOVE);
        printf("mbind err is %d\n", err);
         memset(prefetcher->cache,0,total_size); 
/*      }

      else {
       for(i = 0; i<n; i++){
//           mbind(prefetcher->cache_pool[i*2], real_size, mode,
  //              &node_mask, NUMA_NUM_NODES, MPOL_MF_MOVE);


          bind_strided(  prefetcher->cache_pool[i*2] , real_size, 0);
          memset(prefetcher->cache_pool[i*2], 0, real_size); 
        }
 
  }*/

  prefetcher->counter = malloc(sizeof(int)*prefetcher->ncaches);
   return 0;

}
void* hexe_sync_fetch(int idx){

   if(prefetcher->handle[idx]!=0)
       prefetch_wait(prefetcher->handle[idx]);

    prefetcher->handle[idx] = 0;

    return prefetcher->cache_pool[2*idx];
}


int hexe_start_fetch_continous(void *start_addr, size_t size, int idx)
{
    if(mem_manager->memory_mode == CACHE){
        prefetcher->cache_pool[idx]= start_addr;
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_prefetch_continous(start_addr, prefetcher->cache_pool[2*idx], size);

    return 0;
}

int hexe_start_fetch_continous_taged(void *start_addr, size_t size, int idx, int tag)
{
    if(mem_manager->memory_mode == CACHE){
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
    if(mem_manager->memory_mode == CACHE){
        printf("not supported\n");
        prefetcher->cache_pool[idx]= start_addr;
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_prefetch_strided(start_addr, prefetcher->cache_pool[2*idx], count, block_len, stride);

    return 0;
}


int hexe_start_write_back_continous(size_t size, int cache_idx){

    if(mem_manager->memory_mode == CACHE)
        return 0; 

    hexe_sync_fetch(cache_idx);
    prefetcher->handle[cache_idx] = start_prefetch_continous(prefetcher->cache_pool[2*cache_idx], prefetcher->cache_pool[2*cache_idx+1], size);

    return 0;
}

int hexe_start_write_back_strided(size_t count, size_t block_len, size_t stride,  int idx)
{
    if(mem_manager->memory_mode == CACHE){
        printf("not supported\n");
        return 0;
    }
    hexe_sync_fetch(idx);
    prefetcher->handle[idx] = start_write_back_strided(prefetcher->cache_pool[2*idx], prefetcher->cache_pool[2*idx+1],  count, block_len, stride);

    return 0;
}

int hexe_start_fetch_non_continous(void *start_addr, int* offset_list, size_t element_size, int elements, int idx){ 

    if(mem_manager->memory_mode == CACHE)
        return 0; 

    prefetcher->handle[idx] = start_prefetch_noncontinous(start_addr, prefetcher->cache_pool[idx], offset_list, element_size, elements);

    return 0;
}

void *hexe_get_cache_for_write_back(int idx, void *addr) {

    if(mem_manager->memory_mode == CACHE)
    {
        prefetcher->cache_pool[idx]= addr;
        return addr;
    }
    prefetcher->cache_pool[idx*2+1]= addr;
    return   hexe_sync_fetch(idx);

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

#endif
