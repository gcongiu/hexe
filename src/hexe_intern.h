#ifndef _hexe_intern_h_
#define _hexe_intern_h_
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


struct hexe{
	void **cache_pool;
	void *cache;
	size_t cache_size;
	int start_id;
	int ncaches;
	int prefetch_threads;
    int compute_threads;
    prefetch_handle_t* handle;
    int is_init;
    int cluster_mode;
    int memory_mode;
    int ddr_nodes;
    int mcdram_nodes;
    size_t mcdram_memory;

	hwloc_cpuset_t *prefetch_cpusets;
	hwloc_cpuset_t *compute_cpusets;
	hwloc_topology_t topology;
    hwloc_bitmap_t *ddr_sets;
    hwloc_bitmap_t *mcdram_sets;
    hwloc_nodeset_t all_ddr;
    hwloc_nodeset_t all_mcdram;

};



struct hexe *prefetcher;

#endif
