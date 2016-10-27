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
#define ITER 16
#define SIZE (1024*1024*488)

#ifdef USE_PAPI
int EventSet[3] ={ PAPI_NULL, PAPI_NULL, PAPI_NULL};

#pragma omp threadprivate(EventSet)
#endif
int id;
#pragma omp threadprivate(id)




void init(double* field) {
	int i,k;

	for(k = 0; k< 2; k ++) {
#pragma omp parallel for 
		for(i = 0; i< SIZE; i++) {
			field[i+k*SIZE] = (double)i/1024.0;

		}
	} 
}
int main (int argc, char *argv[])
{

	double*  fieldA, *fieldB;
	size_t i,k;
	double goal;
	int t;
	struct aclock timer;
	int threads = 1;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;


	if(argc >= 2){
		threads = atoi(argv[1]);
	}
	omp_set_num_threads(threads);
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);
    nroots = hwloc_get_nbobjs_by_depth(topology, 0);
    roots = (hwloc_obj_t *) malloc(nroots*sizeof(hwloc_obj_t));
    for(i = 0; i < nroots; i++)
       roots[i] = hwloc_get_obj_by_depth(topology, 0, i);
     cpusets = (hwloc_cpuset_t *) malloc((threads)*sizeof(hwloc_bitmap_t));

    hwloc_distrib(topology, roots, nroots,cpusets, threads, INT_MAX, 0); 

 omp_set_dynamic(0);
#pragma omp parallel
     {
         id = omp_get_thread_num();
         hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}




	init_aclock(&timer);
#ifdef HBW_MEM
	fieldB = memkind_malloc(MEMKIND_HBW,2*sizeof(double)*(SIZE+3*1024*1024));
#else
	fieldB = malloc(2*sizeof(double)*(SIZE+3*1024*1024));
#endif
	fieldA = malloc(2*sizeof(double)*SIZE+2*1024*1024);

	init(fieldA); 
	init(fieldB);
	start_aclock(&timer);
	for(t = 0; t<ITER; t++) {
		for(k = 0; k< 2; k ++) {
#pragma omp parallel for firstprivate(k)
			for(i = 0; i< SIZE; i++) {
			fieldB[k*SIZE+i]  = fieldA[k*SIZE+i]+
				      fieldA[k*SIZE+i+1024*1024];
/*				      fieldA[k*SIZE+i+2*1024*1024]+
				      fieldA[k*SIZE+i+3*1024*1024]+
*/

			}
		}
	}
	end_aclock(&timer);
	printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);

}
