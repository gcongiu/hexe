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
#define ITER 64
//#define max_size (4*1024*1024)


int main (int argc, char *argv[])
{

   char *fieldA, *fieldB;
    int i,t;

    struct aclock timer;
    int threads = 1;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;
    size_t max_size = 1024; 
    init_aclock(&timer);

    if(argc >= 2){
        threads = atoi(argv[1]);
    }
   if(argc >=3){
        max_size = atoi(argv[2]);
 	}
     max_size*=1024; 
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);
    nroots = hwloc_get_nbobjs_by_depth(topology, 0);
    roots = (hwloc_obj_t *) malloc(nroots*sizeof(hwloc_obj_t));
    for(i = 0; i < nroots; i++)
       roots[i] = hwloc_get_obj_by_depth(topology, 0, i);
     cpusets = (hwloc_cpuset_t *) malloc((threads)*sizeof(hwloc_bitmap_t));

    hwloc_distrib(topology, roots, nroots,cpusets, threads, INT_MAX, 0); 


     omp_set_num_threads(threads);
/* omp_set_dynamic(0);
#pragma omp parallel
     {
         int id = omp_get_thread_num();
         //hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}
*/
    srand(time(NULL));


    fieldA = (char*)memkind_malloc(MEMKIND_HBW, ITER*(max_size));
	
    fieldB = (char*)memkind_malloc(MEMKIND_DEFAULT, ITER*(max_size+1024*1024));
     size_t copy_size = 1024;
     size_t chunk = 1;
	for(copy_size = max_size; copy_size<=max_size; copy_size*=2)
{
	memset(fieldA, 0x0, copy_size*ITER);	
	memset(fieldA, 0x03, copy_size*ITER);
	chunk = copy_size/threads;
        printf("chunk is %d\n", chunk);
	start_aclock(&timer);
	for(i = 0; i<ITER; i++){
	char *dst = fieldA + i*copy_size;
	char *src = fieldB + i*copy_size;
#pragma omp parallel
	{
         int id = omp_get_thread_num();
	  memcpy(dst+id*chunk, src+id*chunk, chunk); 
	}

	}
end_aclock(&timer);

printf("Timer %d %d \t %f\n", threads,copy_size,  (double)get_useconds(&timer)/ITER);

}
}

