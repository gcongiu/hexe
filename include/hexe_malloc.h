#ifndef HEXE_MALLOC_H
#define HEXE_MALLOC_H
#include "stdint.h"
#include <stdlib.h>
#include <stddef.h>
#include <numa.h>
#include <numaif.h>
#include <sys/mman.h>
#include "hexe_intern.h"
#include "list.h"
#define TOUCH_PAGES 0x1
#ifdef __cplusplus



extern "C" {
#endif
/*initilize hexe (detect the mode etc.*/

int hexe_init();
/*test, if hexe is init */
int hexe_is_init();
////
/*detect, the CPU and numa-node the current thread is runnig on */


/* static function to detect the core an socked a thread is running on */
static unsigned long tacc_rdtscp(int *chip, int *core)
{
    unsigned long a,d,c;
    __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
    *chip = (c & 0xFFF000)>>12;
    *core = c & 0xFFF;
    return ((unsigned long)a) | (((unsigned long)d) << 32);;
}

static inline void* hexe_alloc_hbw_local(size_t size )
{
    int node, cpu;
    void* addr;

    if( !hexe_is_init() ){

        hexe_init();
    }


    tacc_rdtscp(&node,&cpu);
     addr =  hwloc_alloc_membind_nodeset(mem_manager->topology, size, mem_manager->mcdram_sets[node], 
     HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_STRICT);
    insertlist_fixed(addr, size, MCDRAM_FIXED);
    return addr;
}


/*Return available MCDRAM/HBW memory */
size_t hexe_avail_mcdram();

/* request HBW memory. The memoy will be allocated, but not pinned, this happens if
 * hexe_bind_requested_memory is called, based on the size and priority  */
void* hexe_request_hbw(void* addr, size_t size, int priority);

/* Verify the memory location.. Note that this function  tests  the location of every page */
int hexe_verify_memory_region(void* addr, size_t size, int flags);

/*free requested memory */
void hexe_free_memory(void *ptr);


static inline void* hexe_alloc_ddr(size_t size )
{
    void *map_addr;
    unsigned long node_mask;
    int mode;

    struct node *entry;
    int node, cpu;

    if( !hexe_is_init() ){

        hexe_init();
    }

    map_addr =  hwloc_alloc_membind_nodeset(mem_manager->topology, size, mem_manager->all_ddr, 
     HWLOC_MEMBIND_INTERLEAVE, HWLOC_MEMBIND_STRICT);
 
    if (size > (1024 * 1024 * 2) )
        madvise(map_addr, size, MADV_HUGEPAGE);

    insertlist_fixed(map_addr, size, DDR_FIXED);
    return map_addr;
}


static inline void* hexe_alloc_hbw(size_t size ){
    void *map_addr;
    unsigned long node_mask;
    int mode;

    struct node *entry;
    int node, cpu;
    if( !hexe_is_init() ){
        hexe_init();
    }
    map_addr =  hwloc_alloc_membind_nodeset(mem_manager->topology,  size, mem_manager->all_mcdram, 
     HWLOC_MEMBIND_INTERLEAVE, HWLOC_MEMBIND_STRICT);


    if (size > (1024 * 1024 * 2) )
        madvise(map_addr, size, MADV_HUGEPAGE);

      insertlist_fixed(map_addr, size, MCDRAM_FIXED);
      return map_addr;


}

static inline void* hexe_alloc_ddr_local(size_t size ){
    int node, cpu;
    void *addr;
    if( !hexe_is_init() ){
        hexe_init();
    }

    tacc_rdtscp(&node,&cpu);
     addr =  hwloc_alloc_membind_nodeset(mem_manager->topology, size, mem_manager->ddr_sets[node], 
     HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_STRICT);

    insertlist_fixed(addr, size, DDR_FIXED);
    return addr;
}

static inline int hexe_bind_to_hbw(void * addr, size_t size)
{
    int ret;
    unsigned long node_mask;

    if( !hexe_is_init() ){
        hexe_init();
    }

    hwloc_membind_policy_t  mode = (mem_manager->mcdram_nodes > 1) ? HWLOC_MEMBIND_INTERLEAVE: HWLOC_MEMBIND_BIND;

    hwloc_set_area_membind_nodeset(mem_manager->topology, addr, size,
                           mem_manager->all_mcdram, mode,  HWLOC_MEMBIND_MIGRATE);

    if(size >= (1024 *1024 * 2))
        madvise(addr, size, MADV_HUGEPAGE);
    return ret;
}


static inline int hexe_bind_to_hbw_local(void * addr, size_t size)
{


    int ret;
    unsigned long node_mask;
    unsigned int mode;
    struct node *entry; 
    int node, cpu;
    if( !hexe_is_init() ){
        hexe_init();
    }

    tacc_rdtscp(&node,&cpu);
    hwloc_set_area_membind_nodeset(mem_manager->topology, addr, size,
                           mem_manager->mcdram_sets[node],HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_MIGRATE);

   if(size > 1024 * 1024 * 2)
        madvise(addr, size, MADV_HUGEPAGE);
    return ret;
}

static inline int hexe_bind_to_ddr(void * addr, size_t size)
{
    int ret;
    unsigned long node_mask;

    hwloc_membind_policy_t  mode = (mem_manager->mcdram_nodes > 1) ? HWLOC_MEMBIND_INTERLEAVE: HWLOC_MEMBIND_BIND;
    hwloc_set_area_membind_nodeset(mem_manager->topology, addr, size,
                           mem_manager->all_ddr,HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_MIGRATE);

    if(size > 1024 * 1024 * 2)
        madvise(addr, size, MADV_HUGEPAGE);
    return ret;
}

static inline int hexe_bind_to_ddr_local(void * addr, size_t size)
{
    int ret;
    int node, cpu;
    if( !hexe_is_init() ){
        hexe_init();
    }
    tacc_rdtscp(&node,&cpu);

    hwloc_set_area_membind_nodeset(mem_manager->topology, addr, size,
                           mem_manager->ddr_sets[node],HWLOC_MEMBIND_BIND, HWLOC_MEMBIND_MIGRATE);

    if(size > 1024 *1024 * 2)
        madvise(addr, size, MADV_HUGEPAGE);

    return ret;
}
static inline int hexe_bind_to_hbw_dist(void * addr, size_t size)
{


    int ret;
    struct node *entry; 


    if(mem_manager->cluster_mode &&( QUADRANT || HEMISPHERE || ALL2ALL))
        return hexe_bind_to_hbw_local(addr, size);

    ret = bind_strided(addr, size, 0);
    if(size > 1024*1024*1024)
        madvise(addr, size, MADV_HUGEPAGE);
    return ret;
}

static inline int hexe_bind_to_ddr_dist(void * addr, size_t size)
{
    int ret;
    struct node *entry; 

    if(mem_manager->cluster_mode &&( QUADRANT || HEMISPHERE || ALL2ALL))
        return hexe_bind_to_ddr_local(addr, size);

    ret = bind_strided(addr, size, 1);
    if(size > 1024*1024*1024)
        madvise(addr, size, MADV_HUGEPAGE);

    return ret;
}


static inline void* hexe_realloc_ddr(void* old,  size_t size )
{
    void *map_addr;
    unsigned long node_mask;
    struct node *entry;
    int mode;
    int old_size = 0;



    if( !hexe_is_init() )
        hexe_init();


    if (old == NULL)
        return hexe_alloc_ddr(size);

    entry = find(old, 1);
    if(entry)
        old_size = entry->size;
    map_addr = mremap(old, old_size, size, 0);

    hexe_bind_to_ddr_local(map_addr, size);

    if(entry) {
        entry->addr = map_addr;
        entry->size = size;
    }
    else {
        entry = insertlist_fixed(map_addr, size, DDR_FIXED); 
    }

    madvise(map_addr, size, MADV_HUGEPAGE);

    return map_addr;
}



static inline void* hexe_realloc_hbw(void* old,  size_t size )
{
    void *map_addr;
    unsigned long node_mask;
    struct node *entry;
    int mode;
    size_t old_size;
    if( !hexe_is_init() )
        hexe_init();

    if(old == NULL)
        return hexe_alloc_hbw(size);

    entry = find(old, 1);
    if(entry)
        old_size = entry->size;
    map_addr = mremap(old, old_size, size, 0);
    hexe_bind_to_hbw_local(map_addr, size);

    if(entry) {
        entry->addr = map_addr;
        entry->size = size;
    }
    else {
        entry = insertlist_fixed(map_addr, size, DDR_FIXED); 
    }

    madvise(map_addr, size, MADV_HUGEPAGE);
    return map_addr;
}

static inline void* hexe_calloc_hbw(size_t num, size_t el_size )
{
    void *map_addr;
    unsigned long node_mask;
    int mode;
    struct node* entry;
    size_t size = num * el_size;
    if( !hexe_is_init() )
        hexe_init();
    map_addr = hexe_alloc_hbw(size);
    memset(map_addr, 0, size); 
    return map_addr;
}




static inline void* hexe_calloc_ddr(size_t num, size_t el_size)
{
    void *map_addr;
    unsigned long node_mask;
    int mode;
    struct node *entry;
    size_t size = num * el_size;
    int cpu, node;
   tacc_rdtscp(&node,&cpu);
    if( !hexe_is_init() )
        hexe_init();

    map_addr = hexe_alloc_hbw(size);

    memset(map_addr, 0, size);
    return map_addr;
}
static inline int hexe_is_in_hbw (void *ptr) {


    if(mem_manager->memory_mode == CACHE)
        return 0;
    struct node* entry = find(ptr,0);
    if(! entry)
        entry = find(ptr,1);
    if(! entry)
        return -EINVAL;
    return ((entry->location == MCDRAM_KNP || entry->location ==  MCDRAM_FIXED )? 1: 0);

}


int  hexe_bind_requested_memory(const int redistribute);
int hexe_change_priority(void* ptr, int priority);

int hexe_mpi_bind(int rank, int size);
#ifdef __cplusplus
}
#endif
#endif
