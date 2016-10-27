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
#define ITER 32
#define SIZE (1024*1024*1024)
#define POOL_SIZE (16*1024*1024)


void init(double* field) {
	int i,k;
for(k = 0; k<2; k++) {
#pragma omp parallel for 
        for(i = 0; i< SIZE; i++) {
            field[i+SIZE*k] = (double)i/1024.0;

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
    int prefetch_threads = 32;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;


    if(argc >= 2){
        threads = atoi(argv[1]);
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


    fieldA = memkind_malloc(MEMKIND_DEFAULT, 3*sizeof(double)*(SIZE+1024*1024));
    fieldB = memkind_malloc(MEMKIND_DEFAULT, 3*sizeof(double)*(SIZE));

    printf("memsize is %u \n",  (3*sizeof(double)*SIZE)/(1024*1024));

    init(fieldA); 
    init(fieldB);




    start_aclock(&timer);
    size_t block = (size_t)SIZE/(POOL_SIZE)*3;

    int q = 0;

    for(t = 0; t<ITER; t++) {
        for(k = 0; k< block; k++) {
            q = 1-q;
#pragma omp parallel for  firstprivate(k)
            for(i = 0; i< POOL_SIZE; i++) {
                fieldB[i+POOL_SIZE*k]=(fieldA[k*POOL_SIZE+i]+fieldA[k*POOL_SIZE+i+1024*1024])*02.43;
            }
        }
    }

end_aclock(&timer);
printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);
ende:

    free(fieldA);
    free(fieldB);

    return 0;
}
