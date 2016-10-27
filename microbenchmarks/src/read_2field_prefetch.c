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
#include <hexe.h>
#define ITER 32
#define SIZE (1024*1024*768)
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
    //omp_set_dynamic(0);
#pragma omp parallel
    {
        int id = omp_get_thread_num();
        hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
    }



    hexe_init(prefetch_threads);
    hexe_set_topology(cpusets+threads, topology); 

    hexe_alloc_pool((POOL_SIZE+1024*1024)*sizeof(double), 2);
    hexe_start();




    fieldA = malloc(2*sizeof(double)*(SIZE+1024*1024));
    fieldB = memkind_malloc(MEMKIND_HBW, 2*sizeof(double)*SIZE);

    printf("memsize is %u \n",  (2*sizeof(double)*SIZE)/(1024*1024));

    init(fieldA); 
    init(fieldB);


    start_aclock(&timer);
    size_t block = (size_t)SIZE/(POOL_SIZE)*2;
    size_t prefetch_size = sizeof(double)*(POOL_SIZE + 1024*1024);
    double *__restrict cache; // = memkind_malloc(MEMKIND_DEFAULT, prefetch_size);
    printf("block size %u\n, prefetch size %u\n", block, prefetch_size/(1024*1024));
    int q = 0;

    for(t = 0; t<ITER; t++) {
//        hexe_start_fetch_continous(fieldA, prefetch_size, q);

  //       hexe_sync_fetch(q);
        for(k = 0; k< block; k++) {
            q = 1-q;
      //      if(k< block-1)
    //            hexe_start_fetch_continous(fieldA+(k+1)*POOL_SIZE, prefetch_size, q);
            cache = hexe_sync_fetch(1-q); // (double*)(fieldA+k*POOL_SIZE);

#pragma omp parallel for firstprivate(k)  
            for(i = 0; i< POOL_SIZE; i++) {
                fieldB[i+POOL_SIZE*k]=(cache[i]+cache[i+1024*1024])*02.43;
            }
        }
    }

    end_aclock(&timer);
    printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);
ende:

    hexe_finalize();
    free(fieldA);
    memkind_free(MEMKIND_HBW, fieldB);
    return 0;
}
