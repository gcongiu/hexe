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
#define ITER 20
#define SIZE (1024*1024*512)


 unsigned short lfsr_low = 0xACE1u;
 unsigned short lfsr_high = 0xd1u;
  unsigned bit1;
unsigned bit2;


#pragma omp threadprivate(lfsr_low, lfsr_high, bit1, bit2)
 unsigned int my_rand()
  {
    bit1  = ((lfsr_low >> 0) ^ (lfsr_low >> 2) ^ (lfsr_low >> 3) ^ (lfsr_low >> 5) ) & 1;
    bit2  = ((lfsr_high >> 0) ^ (lfsr_high >> 2) ^ (lfsr_high >> 3) ^ (lfsr_high >> 5) ) & 1;
    lfsr_low =  (lfsr_low >> 1) | (bit1 << 15);
    lfsr_high =  (lfsr_high >> 1) | (bit2 << 15);
    return ((int)lfsr_low|(int)(lfsr_high>>16));
  }
int init_index(int *indx) {
    int i;
#pragma omp parallel  num_threads(32)
{
lfsr_low += (short) omp_get_thread_num();
lfsr_high -= (short) omp_get_thread_num();
}
#pragma omp parallel for num_threads(32)
    for(i = 0; i<SIZE+1024*1024; i++) {
        indx[i] = my_rand()%SIZE;

   }

}

void init(double* field) {
	int i,k;
#pragma omp parallel for num_threads(32)
        for(i = 0; i< SIZE; i++) {
            field[i] = (double)i/1024.0;

        }
 
}
int main (int argc, char *argv[])
{
    int* idx;
    double *fieldA;
    int i,t;

    struct aclock timer;
    int threads = 1;
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
     cpusets = (hwloc_cpuset_t *) malloc((threads)*sizeof(hwloc_bitmap_t));

    hwloc_distrib(topology, roots, nroots,cpusets, threads, INT_MAX, 0); 


     omp_set_num_threads(threads);
 omp_set_dynamic(0);
#pragma omp parallel
     {
         int id = omp_get_thread_num();
         hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}

    srand(time(NULL));

#ifdef HBW_MEM
    fieldA = (double*)memkind_malloc(MEMKIND_HBW, sizeof(double)*(SIZE+1024*1024));
#else
    fieldA = malloc(sizeof(double)*(SIZE+1024*1024));
#endif


   idx = malloc( (SIZE+1024*1024)*sizeof(int));
    init_aclock(&timer);
    start_aclock(&timer);
 
   init_index(idx);
   init(fieldA);
    end_aclock(&timer);



   double sum;

    init_aclock(&timer);
    start_aclock(&timer);
   for(t=0; t<ITER; t++) {
#pragma omp parallel for reduction(+: sum)
       for(i = 0; i<SIZE; i++) {

           sum += fieldA[idx[i]]*fieldA[idx[i+1]];

       } 
   }

    end_aclock(&timer);
printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);
ende:
#ifdef HBW_MEM
    memkind_free(MEMKIND_HBW, fieldA);


#else
   free(fieldA);
#endif

    free(idx);


}
