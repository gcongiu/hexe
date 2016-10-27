#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "timer.h"
#include <omp.h>
#include <memkind.h>
#include <sys/mman.h>
#include<pthread.h>
#include <hwloc.h>
#define ITER 1
#define SIZE (1024*1024)
#define POOL_SIZE (16*1024*1024)


void init(int* field, int size) {
	int i,k;
        for(i = 0; i< size; i++) {
            field[i] = i/1024;

        }
 }
int main (int argc, char *argv[])
{

    int*  fieldA;
    size_t i,k;
    double goal;
    int t;
    struct aclock timer;
    int threads = 1;
    int prefetch_threads = 0;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;
    int stride = 1;

    if(argc >= 2){
        threads = atoi(argv[1]);
    } 
   if(argc >= 3){
        stride = atoi(argv[2]);
    }
   hwloc_topology_init(&topology);
   hwloc_topology_load(topology);
    nroots = hwloc_get_nbobjs_by_depth(topology, 0);
    roots = (hwloc_obj_t *) malloc(nroots*sizeof(hwloc_obj_t));
    for(i = 0; i < nroots; i++)
       roots[i] = hwloc_get_obj_by_depth(topology, 0, i);
     cpusets = (hwloc_cpuset_t *) malloc((prefetch_threads+threads)*sizeof(hwloc_bitmap_t));

    hwloc_distrib(topology, roots, nroots,cpusets, threads+prefetch_threads, INT_MAX, 0); 


     omp_set_num_threads(threads);
 omp_set_dynamic(0);
#pragma omp parallel
     {
         int id = omp_get_thread_num();
         hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}

#ifdef HBW_MEM
if( stride*sizeof(int)*(SIZE)>=(8192*1024*1024-1))
        return 0;

    fieldA = memkind_malloc(MEMKIND_HBW, stride*sizeof(int)*(SIZE));
#else
    fieldA = malloc(stride*sizeof(int)*(SIZE));
#endif
    init(fieldA, SIZE*stride); 

    start_aclock(&timer);
   for(t = 0; t<ITER; t++) {
    goal = 0;
#pragma omp parallel for reduction(+ :goal) firstprivate(k)
            for(i = 0; i< SIZE; i++) {
                goal+= fieldA[i*stride];
            }
        }

end_aclock(&timer);
printf("Timer %d %d \t %f\n", threads, stride*sizeof(int), get_seconds(&timer)/ITER);
ende:
#ifdef HBW_MEM
   memkind_free(MEMKIND_DEFAULT, fieldA);
#else

   free(fieldA);
#endif
    return 0;
}
