#ifndef _hexe_intern_h_
#define _hexe_intern_h_
#include <hwloc.h>
typedef  uint64_t prefetch_handle_t;
typedef  uint64_t prefetch_thread_t;



enum ClusterMode
{
    QUADRANT = 1,
    HEMISPHERE = 2,
    SNC4 = 4,
    SNC2 = 8,
    ALL2ALL = 16
};

enum MemoryMode
{
    CACHE = 1,
    FLAT = 2,
    HYBRID25 = 4,
    HYBRID50 = 8
};


struct hexe_prefetcher{
	void **cache_pool;
	void *cache;
    int *counter;
	size_t cache_size;
	int ncaches;
	int prefetch_threads;
    int compute_threads;
    prefetch_handle_t* handle;
    int is_started;
	hwloc_cpuset_t *prefetch_cpusets;
	hwloc_cpuset_t *compute_cpusets;
};

struct hexe_malloc {
    int cluster_mode;
    int memory_mode;
    int ddr_nodes;
    int mcdram_nodes;
    size_t mcdram_avail;
    size_t total_mcdram;
    size_t mcdram_per_node[4];

	hwloc_topology_t topology;
    hwloc_nodeset_t *ddr_sets;
    hwloc_nodeset_t *mcdram_sets;
    hwloc_nodeset_t all_ddr;
    hwloc_nodeset_t all_mcdram;
    int is_init;
};

extern struct hexe_malloc *mem_manager;

extern struct hexe_prefetcher *prefetcher;

int hexe_distribute_threads_knl();


#endif
