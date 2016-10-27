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
#define SIZE (1024*1024*1024)

int main (int argc, char *argv[])
{

    char*  fieldA;
    size_t i;
    char goal;
    int t;
    struct aclock timer;
    int threads = 1;
    if(argc >= 2){
        threads = atoi(argv[1]);
    }
     omp_set_num_threads(threads);
    init_aclock(&timer);
#ifdef HBW_MEM
    fieldA = memkind_malloc(MEMKIND_HBW, SIZE+(1024*1024));
#else
    fieldA = malloc(SIZE+(1024*1024));
#endif

    start_aclock(&timer);
    for(t = 0; t<ITER; t++) {
#pragma omp parallel 
        for(i = 0; i< SIZE; i++) {
            goal+=fieldA[i]+fieldA[i+1024*1024];

        }
    }
    end_aclock(&timer);
    printf("Timer %f\n", get_seconds(&timer));

}
